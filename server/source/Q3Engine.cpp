/*
 * Q3ENGINE.CPP                  Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Author:
 *      AKa (asko.kauppi@fmi.fi)
 */
#include "Q3Engine.h"
#include "Session.h"
#include "RegTools.h"

#include "Track.h"
#include "HealthCheck.h"

#include "Bz2_Tracker.h"

#include "Tools.h"
#include "LogTools.h"
#include "LuaWrap.h"
#include "Proto.h"

#ifndef NDEBUG
#include "Vector.h"
#endif

#include <string>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <math.h>

#include <boost/algorithm/string.hpp>

// 02-Dec-2011 PKi: Labelizer (contour labeling) configuration from configuration file
//
#include "Labelizer.h"

using namespace std;

/*---=== Configuration ===---*/

string read_config(const char *);

static char config_chunk[] =
#include "config.lch"

std::map<std::string, std::string> Q3Engine::addonConfigSettings;

/*
* Read in Q3 configuration.
*
* 02-Dec-2011 PKi: Get Labelizer config too
*/
static void q3_config(const char *conf,
                      map<string, Q3Engine::Track *> &trackers,
                      map<string, string> &addonCfg,
                      unsigned int recurse) /*throw(E_BUG)*/
{
  /*
  * Standard libraries for 'config.lua'.
  */
  static const luaL_Reg my_stdlibs[] = {
      {"", luaopen_base},                // 'select' etc. basic tools (needed)
      {LUA_STRLIBNAME, luaopen_string},  // 'string.*' (needed)
      {LUA_TABLIBNAME, luaopen_table},   // 'table.*'
      {LUA_OSLIBNAME, luaopen_os},       // 'os.*' ('os.getenv()' needed)
      {LUA_IOLIBNAME, luaopen_io},       // 'io.*'

      // These required by 'proto'
      {LUA_MATHLIBNAME, luaopen_math},     // 'math.*'
      {LUA_LOADLIBNAME, luaopen_package},  // 'module' and 'package.*'
      {nullptr, nullptr}};

  LuaWrap L(my_stdlibs, config_chunk, sizeof(config_chunk));

  proto_init(L);

  // Parameters for call to 'config_chunk'
  //
  lua_pushstring(L, conf);

  // Call 'config_chunk'
  //
  int st = lua_pcall(L, 1 /*args*/, 5 /*return values*/, 0 /*no error func needed*/);
  //
  // 0 (ok)
  // LUA_ERRRUN: runtime error
  // LUA_ERRMEM: no memory
  // (LUA_ERRERR: not for us)

  if (st != 0)
  {
    throw E_LOG_BUG("%s", lua_tostring(L, -1));
  }

  // 1: { [track]= { { filter_fn, refresh=secs }, ..., runs=secs }
  //      [, ...]
  //    }
  // 2: [cache_fn]
  // 3: { [[secs_num]= healthcheck_str]
  //      [, ...]
  //    }
  //
  // 02-Dec-2011 PKi: Labelizer configuration
  // 4: { [[field]= value]
  //      [, ...]
  //    }
  //
  // Addon configuration settings
  // 5: { [[addon.field]= value]
  //      [, ...]
  //    }
  //

  L_ASSERT(lua_istable(L, 1));

//---
// Cache entry
//
#if 1
  if (!recurse)
  {
    const char *cache_fn = lua_tostring(L, 2);
    Bz2_Tracker::cache_init(cache_fn);  // Must be done before creation of 'Bz2_Tracker's
  }
#endif

  //---
  // Healthcheck entries - start CPU etc. tracking
  {
    const unsigned t_idx = 3;
    L_ASSERT(lua_istable(L, t_idx));

    lua_pushnil(L);  // first key
    while (lua_next(L, t_idx))
    {
      // [-2]: key (secs as number)
      // [-1]: value (string)

      double secs = lua_tonumber(L, -2);
      const char *s = lua_tostring(L, -1);

      if ((secs <= 0.0) || (!s))
      {
        throw E_LOG_USAGE("Bad entry: healtcheck(%s) = %s", lua_tostring(L, -2), s);
      }

      // This launches a thread and the object is never released. Intentional.
      //
      new HealthCheck((unsigned)(secs * 1000), s);

      lua_pop(L, 1);  // remove value, keep key for next iteration
    }
  }

  //---
  // Data entries
  //
  lua_pushnil(L);  // first key
  while (lua_next(L, 1))
  {
    // [-1]: value
    // [-2]: key

    const char *key = lua_tostring(L, -2);
    L_ASSERT(key);

    // Currently we do not allow splitting track definition into multiple config files
    // (should we accept/check changes to runs etc after the track was created ?)

    if (trackers.find(key) != trackers.end())
      luaL_error(L, "Redefinition of track '%s'", key);

    // By default wipe "normal" data after 1min and archived data after 3min since last access,
    // and metadata (i.e. "normal" data for which metadata was queried) after 1h since first access.
    //
    // Filemask specific wiping delay is stored to last wiping[] slot.
    //
    // Note: paramname[] needs an extra (second) nullptr valued slot due to looping with index step 2
    // when loading filemask specific values

    unsigned wiping[] = {60, 180, 3600, 0};
    bool relative_uv = false;
    uint number_to_keep = 0;
    const char *paramname[] = {
      "relative_uv", "number_to_keep", "wiping", "archwiping", "metawiping", nullptr, nullptr
    };

    const int SUBT = lua_gettop(L);  // absolute index to '{ runs=..., ... }' table
    Q3Engine::Track *t;
    {
      // Forward queries to another track (e.g. HIR to MEPS) ?

      vector<string> aliasvect;

      lua_pushliteral(L, "alias");
      lua_gettable(L, SUBT);
      if (!lua_isnil(L, -1))
      {
        string aliases = lua_tostring(L, -1), tmp;
        boost::split(aliasvect, aliases, boost::is_any_of(","));

        for (vector<string>::const_iterator it = aliasvect.begin(); it != aliasvect.end(); it++)
        {
          auto alias = boost::trim_copy(*it);

          if (alias.empty())
            LOG_WARNING("Warning: empty alias for track '%s'", key);
          else if (alias == key)
          {
            LOG_WARNING("Warning: self alias '%s' ignored", alias.c_str());
            aliases.clear();
          }
          else if (trackers.find(alias) != trackers.end())
            throw E_LOG_USAGE("Redefinition of track '%s' ('%s')", alias.c_str(), key);
          else
            tmp += ((tmp.empty() ? "" : ",") + alias);
        }

        if (aliases.empty() && (!tmp.empty()))
          boost::split(aliasvect, tmp, boost::is_any_of(","));
      }

      lua_pushliteral(L, "runs");
      lua_gettable(L, SUBT);
      unsigned run_secs =
          lua_tointeger(L, -1);  // converted to seconds by 'track_proxy.lua' (default: 0)

      if ((run_secs == 0) && (!lua_isnil(L, -1)) && (!lua_isnumber(L, -1)))
      {
        LOG_WARNING("Bad configuration entry: 'runs=%s' (expected '[Xh][YYmin][ZZsec]')",
                    lua_tostring(L, -1));
      }

      lua_pushliteral(L, "tmp_pattern");
      lua_gettable(L, SUBT);
      const char *tmp_pattern = lua_tostring(L, -1);  // valid ONLY until popped!

      lua_pushliteral(L, "tmp_threshold");
      lua_gettable(L, SUBT);
      const char *s = lua_tostring(L, -1);  // "x[.x]MB" or "x[.x]GB"
      size_t tmp_threshold = 0;

      if (s)
      {
        double d;
        if (sscanf(s, "%lfMB", &d) == 1)
        {
          tmp_threshold = (size_t)((1024 * 1024) * d);
        }
        else if (sscanf(s, "%lfGB", &d) == 1)
        {
          tmp_threshold = (size_t)((1024 * 1024 * 1024) * d);
        }
        else
        {
          LOG_WARNING("Bad 'tmp_threshold' syntax: %s (ignored)", s);
        }
      }

      for (int w = 0; paramname[w]; w++)
      {
        lua_pushstring(L, paramname[w]);
        lua_gettable(L, SUBT);
        if (!lua_isnil(L, -1))
        {
          // relative_uv (boolean) is the first parameter

          if (((w == 0) && lua_isboolean(L, -1)) || ((w > 0) && lua_isnumber(L, -1)))
          {
            if (w == 0)
              relative_uv = lua_toboolean(L, -1);
            else if (w == 1)
              number_to_keep = lua_tointeger(L, -1);
            else
              wiping[w - 1] = lua_tointeger(L, -1);
          }
          else
          {
            LOG_WARNING("Bad '%s' value for track %s (ignored)", paramname[w], key);
          }
        }
        // printf("*** WIPING %d %u ***\n",w,wiping[w]);
        lua_pop(L, 1);
      }

      t = new Q3Engine::Track(key, run_secs, tmp_pattern, tmp_threshold);
      trackers[key] = t;

      if (!aliasvect.empty())
      {
        for (vector<string>::const_iterator it = aliasvect.begin(); it != aliasvect.end(); it++)
        {
          auto alias = boost::trim_copy(*it);

          auto ta = new Q3Engine::Track(alias.c_str(), run_secs, tmp_pattern, tmp_threshold, key);
          trackers[alias.c_str()] = ta;
        }
      }
    }
    lua_settop(L, SUBT);  // restore stack to where it was (pops 3)

    // Gather file filters as indexed members of 'SUBT'
    //
    unsigned n = lua_objlen(L, SUBT);

    for (unsigned i = 1; i <= n; i++)
    {
      lua_pushinteger(L, i);
      lua_gettable(L, SUBT);

      // [-1]: { filter_fn, refresh=... }

      L_ASSERT(lua_istable(L, -1));

      lua_pushliteral(L, "refresh");
      lua_gettable(L, -2);

      // We had some problems with 'refresh' 0 (no refresh); better to _force_ having some
      // value for it ("false" will disable refreshes).   --AKa 17-Mar-10
      //
      unsigned refresh_secs = 0;
      if (lua_isnumber(L, -1))
      {
        refresh_secs = lua_tointeger(L, -1);  // converted to seconds by 'config.lua'
      }
      else
      {
        // 22-Nov-2012 PKi: Protection against passing nullptr to strcmp
        //
        const char *s = (lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr);
        if (!s)
        {
          luaL_error(L, "Missing 'refresh' value (give 'false' for no refresh)");
        }
        else if (strcmp(s, "false"))
        {
          luaL_error(L, "Bad value: refresh = %s", s);
        }
      }
      lua_pop(L, 1);

      bool mask_relative_uv = relative_uv;
      uint mask_number_to_keep = number_to_keep;

      for (int w = 0; paramname[w]; w += ((w <= 1) ? 1 : 2))
      {
        lua_pushstring(L, paramname[w]);
        lua_gettable(L, -2);
        if (((w == 0) && lua_isboolean(L, -1)) || ((w > 0) && lua_isnumber(L, -1)))
        {
          if (w == 0)
            mask_relative_uv = lua_toboolean(L, -1);
          else if (w == 1)
            mask_number_to_keep = lua_tointeger(L, -1);
          else
            // Filemask specific wiping for archived data too is set using 'wiping' (not
            // 'archwiping'),
            // and the value is stored to last/additional array slot. When wiping, if the last array
            // value
            // is nonzero, it will be used instead of the global or track specific value
            wiping[(w == 2) ? 3 : w - 2] = lua_tointeger(L, -1);
//1=3, 3=2
//0=3, 2=2
        }
        else if (!lua_isnil(L, -1))
        {
          LOG_WARNING("Bad '%s' value for mask %d for track %s (ignored)", paramname[w], i, key);
        }
        // printf("*** WIPING %d %u ***\n",w,wiping[(w == 0) ? 3 : w]);
        lua_pop(L, 1);
      }

      lua_pushinteger(L, 1);
      lua_gettable(L, -2);
      const char *mask_fn = lua_tostring(L, -1);  // 'rootdir=' already applied
      t->add(mask_fn, refresh_secs, wiping, mask_relative_uv, mask_number_to_keep);

      wiping[3] = 0;
#if 1
      if (refresh_secs == 0)
      {
        LOG_WARNING("Filter with no refreshment: %s", mask_fn);
      }
#endif
      lua_pop(L, 2);
    }

    lua_pop(L, 1);  // remove SUBT (keep key)
  }

  //---
  // 16-Dec-2011 PKi: Load Labelizer's default configuration. When labeling contours the
  // configuration is
  //                  fetched/modified by script code and then passed to Labelizer.
  //
  Labelizer::loadDefaultCfg(L, 4);

  //---
  // Load addon configuration settings
  //
  {
    const unsigned t_idx = 5;
    L_ASSERT(lua_istable(L, t_idx));

    string includeFile, includeGlobals;

    lua_pushnil(L);  // first key
    while (lua_next(L, t_idx))
    {
      // [-2]: key (addon.setting)
      // [-1]: value (number, string or boolean)

      string key(lua_tostring(L, -2));
      string val(lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? "true" : "false") : lua_tostring(L, -1));

      if (key == "include.file")
        includeFile = val;
      else
      {
        // Globals are taken only from main config file
        //
        if (((recurse == 0) || (key.substr(0, 8) != "include.")) && !val.empty())
          addonCfg[key] = val;
      }

      lua_pop(L, 1);  // remove value, keep key for next iteration
    }

    if (!includeFile.empty())
    {
      recurse++;

      if (recurse <= 5)
      {
        // Pass on globals as defaults

        for (const auto &setting : addonCfg)
          if ((setting.first.substr(0, 8) == "include.") && !setting.second.empty())
            includeGlobals += (setting.first.substr(8) + " = " + setting.second + "\n");

        q3_config((includeGlobals + read_config(includeFile.c_str())).c_str(),
                  trackers,
                  addonCfg,
                  recurse);

        recurse--;
      }
      else
        luaL_error(L, "Configuration recursion too deep: '%s'", includeFile.c_str());
    }
  }

  return;
}

/*---=== Q3Engine ===---*/

/*
* Initialize the Q3 engine.
*
* 'conf': configuration Lua script
*/
Q3Engine::Q3Engine(const char *conf) : trackers(map<string, Q3Engine::Track *>())
{
#ifndef NDEBUG
  JDay::selftest();
  Vector::selftest();

// ... add other module selftests here
#endif
  q3_config(conf, trackers, addonConfigSettings, 0);
}

Q3Engine::~Q3Engine()
{
  // Release all of the 'Track' objects (no sessions running any more)
  //
  for (map<string, Q3Engine::Track *>::iterator it = trackers.begin(); it != trackers.end(); ++it)
  {
    delete it->second;
  }
}

/*
*/
vector<string> Q3Engine::getNames(bool getAliases) const
{
  vector<string> ret;

  for (map<string, Track *>::const_iterator it = trackers.begin(); it != trackers.end(); ++it)
  {
    if (getAliases || it->second->getAlias().empty())
      ret.push_back(it->first);
  }
  return ret;
}

/*
*/
const Q3Engine::Track *Q3Engine::getTrack(const char *name, bool getAlias) const
{
  // Need to do it like this (and not straight indexing) because we're 'const'
  //
  map<string, Track *>::const_iterator it = trackers.find(name);

  if (getAlias && (it != trackers.end()))
  {
    auto alias = it->second->getAlias();

    if (!alias.empty())
    {
      it = trackers.find(alias.c_str());
    }
  }

  if (it != trackers.end())
  {
    return it->second;
  }
  else
  {
    LOG_DEBUG("No tracker named '%s'", name);
    return nullptr;
  }
}

/*
* Bypass to 'Session::' which is otherwise not available for the plugin.
*/
string_or_null /*mime*/ Q3Engine::result_(lua_State *L, ostream &os, unsigned i)
{
  return Session::result_(L, os, i);
}

/*
* Return addon configuration setting, e.g. fminames.dbhost etc
*/
int Q3Engine::getAddonConfigSetting(lua_State *L)
{
  proto(L, "string");

  string addonsetting(lua_tostring(L, 1));

  if (addonConfigSettings.find(addonsetting) != addonConfigSettings.end())
    lua_pushstring(L, addonConfigSettings[addonsetting].c_str());
  else
    lua_pushnil(L);

  return 1;
}
