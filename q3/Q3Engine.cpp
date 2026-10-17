/*
 * Q3ENGINE.CPP                  Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Author:
 *      AKa (asko.kauppi@fmi.fi)
 */
#include "Q3Engine.h"
#include <macgyver/StringConversion.h>
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

#include <cmath>

#include <boost/algorithm/string.hpp>

// 02-Dec-2011 PKi: Labelizer (contour labeling) configuration from configuration file
//
#include "Labelizer.h"

using namespace std;

/*---=== Configuration ===---*/

static unsigned char config_chunk[] =
#include "config.lch"

std::map<std::string, std::string> Q3Engine::addonConfigSettings;

namespace
{
// Note: paramname[] needs an extra (second) nullptr valued slot due to looping with index step 2
// when loading filemask specific values
//
const char *const paramname[] = {"relative_uv", "wiping", "archwiping", "metawiping", nullptr, nullptr};

/*
* Healthcheck entries (table at 't_idx') - start CPU etc. tracking
*/
void start_healthchecks(lua_State *L, int t_idx)
{
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

    // codechecker_intentional [cplusplus.NewDeleteLeaks] the health check thread lives as long
    // as the process
    lua_pop(L, 1);  // remove value, keep key for next iteration
  }
}

/*
* Forward queries to another track (e.g. HIR to MEPS) ?
*
* Returns the aliases of track 'key' (table at 'SUBT').
*/
vector<string> read_track_aliases(lua_State *L,
                                  int SUBT,
                                  const char *key,
                                  const map<string, Q3Engine::Track *> &trackers)
{
  vector<string> aliasvect;

  lua_pushliteral(L, "alias");
  lua_gettable(L, SUBT);
  if (!lua_isnil(L, -1))
  {
    string aliases = lua_tostring(L, -1);
    string tmp;
    boost::split(aliasvect, aliases, boost::is_any_of(","));

    for (auto &it : aliasvect) {
      auto alias = boost::trim_copy(it);

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

  return aliasvect;
}

/*
* Read the track specific 'relative_uv' and wiping settings of track 'key'
* (table at 'SUBT')
*/
void read_track_wiping(lua_State *L,
                       int SUBT,
                       const char *key,
                       unsigned wiping[],
                       bool &relative_uv)
{
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
}

/*
* Create track 'key' (table at 'SUBT') and its aliases, and read its
* settings to 'wiping' and 'relative_uv'
*/
Q3Engine::Track *create_track(lua_State *L,
                              int SUBT,
                              const char *key,
                              map<string, Q3Engine::Track *> &trackers,
                              unsigned wiping[],
                              bool &relative_uv)
{
  // Forward queries to another track (e.g. HIR to MEPS) ?

  vector<string> aliasvect = read_track_aliases(L, SUBT, key, trackers);

  lua_pushliteral(L, "runs");
  lua_gettable(L, SUBT);
  unsigned run_secs =
      lua_tointeger(L, -1);  // converted to seconds by 'track_proxy.lua' (default: 0)

  if ((run_secs == 0) && (!lua_isnil(L, -1)) && (!lua_isnumber(L, -1)))
  {
    LOG_WARNING("Bad configuration entry: 'runs=%s' (expected '[Xh][YYmin][ZZsec]')",
                lua_tostring(L, -1));
  }

  read_track_wiping(L, SUBT, key, wiping, relative_uv);

  auto *t = new Q3Engine::Track(key, run_secs);
  trackers[key] = t;

  if (!aliasvect.empty())
  {
    for (auto &it : aliasvect) {
      auto alias = boost::trim_copy(it);

      auto *ta = new Q3Engine::Track(alias, run_secs, key);
      trackers[alias.c_str()] = ta;
    }
  }
  return t;
}

/*
* Read the 'refresh' value of the file filter at [-1]
*/
unsigned read_refresh_secs(lua_State *L)
{
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
    else if (strcmp(s, "false") != 0)
    {
      luaL_error(L, "Bad value: refresh = %s", s);
    }
  }
  lua_pop(L, 1);

  return refresh_secs;
}

/*
* Read the filemask specific 'relative_uv' and wiping settings of file
* filter 'i' (at [-1]) of track 'key'. Returns the 'relative_uv' setting.
*/
bool read_mask_wiping(lua_State *L,
                      unsigned i,
                      const char *key,
                      unsigned wiping[],
                      bool relative_uv)
{
  bool mask_relative_uv = relative_uv;

  for (int w = 0; paramname[w]; w += ((w == 0) ? 1 : 2))
  {
    lua_pushstring(L, paramname[w]);
    lua_gettable(L, -2);
    if (((w == 0) && lua_isboolean(L, -1)) || ((w > 0) && lua_isnumber(L, -1)))
    {
      if (w == 0)
        mask_relative_uv = lua_toboolean(L, -1);
      else
        // Filemask specific wiping for archived data too is set using 'wiping' (not
        // 'archwiping'),
        // and the value is stored to last/additional array slot. When wiping, if the last array
        // value
        // is nonzero, it will be used instead of the global or track specific value
        wiping[(w == 1) ? 3 : w - 1] = lua_tointeger(L, -1);
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

  return mask_relative_uv;
}

/*
* Gather file filters of track 't' as indexed members of 'SUBT'
*/
void add_track_filters(lua_State *L,
                       int SUBT,
                       const char *key,
                       Q3Engine::Track *t,
                       unsigned wiping[],
                       bool relative_uv)
{
  unsigned n = lua_objlen(L, SUBT);

  for (unsigned i = 1; i <= n; i++)
  {
    lua_pushinteger(L, i);
    lua_gettable(L, SUBT);

    // [-1]: { filter_fn, refresh=... }

    L_ASSERT(lua_istable(L, -1));

    unsigned refresh_secs = read_refresh_secs(L);

    bool mask_relative_uv = read_mask_wiping(L, i, key, wiping, relative_uv);

    lua_pushinteger(L, 1);
    lua_gettable(L, -2);
    const char *mask_fn = lua_tostring(L, -1);  // 'rootdir=' already applied
    t->add(mask_fn, refresh_secs, wiping, mask_relative_uv);

    wiping[3] = 0;
#if 1
    if (refresh_secs == 0)
    {
      LOG_WARNING("Filter with no refreshment: %s", mask_fn);
    }
#endif
    lua_pop(L, 2);
  }
}

/*
* Data entry of track 'key' (value at [-1])
*/
void read_track(lua_State *L, const char *key, map<string, Q3Engine::Track *> &trackers)
{
  // Currently we do not allow splitting track definition into multiple config files
  // (should we accept/check changes to runs etc after the track was created ?)

  if (trackers.find(key) != trackers.end())
    luaL_error(L, "Redefinition of track '%s'", key);

  // By default wipe "normal" data after 1min and archived data after 3min since last access,
  // and metadata (i.e. "normal" data for which metadata was queried) after 1h since first access.
  //
  // Filemask specific wiping delay is stored to last wiping[] slot.

  unsigned wiping[] = {60, 180, 3600, 0};
  bool relative_uv = false;

  const int SUBT = lua_gettop(L);  // absolute index to '{ runs=..., ... }' table
  Q3Engine::Track *t = create_track(L, SUBT, key, trackers, wiping, relative_uv);
  lua_settop(L, SUBT);  // restore stack to where it was

  // Gather file filters as indexed members of 'SUBT'
  //
  add_track_filters(L, SUBT, key, t, wiping, relative_uv);
}

/*
* Load addon configuration settings (table at 't_idx'). Returns the name of
* the configuration file to include ("" if none).
*/
string read_addon_settings(lua_State *L,
                           int t_idx,
                           map<string, string> &addonCfg,
                           unsigned int recurse)
{
  L_ASSERT(lua_istable(L, t_idx));

  string includeFile;

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

  return includeFile;
}

/*
* Read in Q3 configuration.
*
* 02-Dec-2011 PKi: Get Labelizer config too
*/
void q3_config(const char *conf,
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
  start_healthchecks(L, 3);

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

    read_track(L, key, trackers);

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
  string includeFile = read_addon_settings(L, 5, addonCfg, recurse);
  string includeGlobals;

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
}  // namespace

/*---=== Q3Engine ===---*/

/*
* Initialize the Q3 engine.
*
* 'conf': configuration Lua script
*/
Q3Engine::Q3Engine(const char *conf)  
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
  for (auto &tracker : trackers) {
    delete tracker.second;
  }
}

/*
*/
vector<string> Q3Engine::getNames(bool getAliases) const
{
  vector<string> ret;

  for (const auto &tracker : trackers) {
    if (getAliases || tracker.second->getAlias().empty())
      ret.push_back(tracker.first);
  }
  return ret;
}

/*
*/
const Q3Engine::Track *Q3Engine::getTrack(const char *name, bool getAlias) const
{
  // Need to do it like this (and not straight indexing) because we're 'const'
  //
  auto it = trackers.find(name);

  if (getAlias && (it != trackers.end()))
  {
    auto alias = it->second->getAlias();

    if (!alias.empty())
    {
      it = trackers.find(alias);
    }
  }

  if (it != trackers.end())
  {
    return it->second;
  }
  
      LOG_DEBUG("No tracker named '%s'", name);
    return nullptr;
 
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
