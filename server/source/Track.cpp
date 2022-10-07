/*
* TRACK.CPP                  Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Tracking changes to Q3 data files.
*
* Note:
*      In server mode, changes to the configuration file itself COULD be
*      informed via asynchronous SIGHUP signal. This is currently NOT supported;
*      the configuration is executed once and thought to be non-changing.
*      Simply kill and restart the server when there are changes to the config.
*       -- AKa 2-Mar-2009
*/
#include "Track.h"
#include "Sqd_Tracker.h"
#include "Bz2_Tracker.h"

#include "SQD_Data.h"

#include "LogTools.h"
#include "Tools.h"
#include "MoreTools.h"
#include "Proto.h"

#include "Raw.h"
#include "Grid.h"
#include "Matrix.h"

#include <cassert>

#include <map>
#include <vector>
#include <cstdlib>
#include <cassert>
#include <cstring>

extern "C" {
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
}

using namespace std;

#ifdef HEIGHT_TRUE_PULLS_IN_Z
static const ApiScalarParam Z("Z");
#endif

/*---=== Helpers ===---
*/

/*---=== Track ===---
*/

Q3Engine::Track::Track(const string &name_,
                       time_t run_secs_,
                       const char *tmp_pattern_,
                       size_t tmp_threshold_,
                       const string &alias_)
    : watch(),
      name(name_),
      alias(alias_),
      run_secs(run_secs_),
      tmp_pattern(tmp_pattern_),
      tmp_threshold(tmp_threshold_)
{
}

/*
* Note: We don't really come here in the server (exit via ctrl-c)
*/
Q3Engine::Track::~Track()
{
  for (vector<TrackerBase *>::const_iterator it = watch.begin(); it != watch.end(); ++it)
  {
    delete *it;
  }
}

/*
* Called for adding each file mask entry in the configuration file.
*/
void Q3Engine::Track::add(const char *mask_fn,
                          unsigned refresh_secs,
                          unsigned wiping[],
                          bool relative_uv) throw(E_USAGE)
{
  // 09-Mar-2012 PKi: Pass track name too (used for logging only)
  //
  if (ends_with(mask_fn, ".sqd"))
  {
    // Trackers for unarchived data first
    //
    vector<TrackerBase *>::iterator it = watch.begin();
    for (; ((it != watch.end()) && (!((*it)->archived()))); it++)
      ;

    if (it != watch.end())
      watch.insert(it, new Sqd_Tracker(mask_fn, refresh_secs, wiping, relative_uv, getName()));
    else
      watch.push_back(new Sqd_Tracker(mask_fn, refresh_secs, wiping, relative_uv, getName()));
  }
  else if (ends_with(mask_fn, ".bz2"))
  {
    watch.push_back(new Bz2_Tracker(string(mask_fn), refresh_secs, wiping, relative_uv, "/tmp/q3-XXXXXX", 0));
  }
  else
  {
    throw E_LOG_USAGE("Bad filter filename (postfix isn't '.sqd'): %s", mask_fn);
  }
}

/*
* Find a suitable data from _any_ configuration entries of the track for a particular
* origintime.
*
* 'err':    Initially set empty by the caller.
*           Collects reasons (one line per each data tried) as to why there was no match.
*
* Returns: pointer to suitable data, TO BE RELEASED by the caller
*          nullptr if no data + appends 'err' with the reason, why not.
*/
TrackedData *Q3Engine::Track::getData_must_release_(const JDay &ot,
                                                    const std::vector<JDay> &required_times,
                                                    const std::vector<ApiParam> &required_params,
                                                    const std::vector<NA_Level> &required_levels,
                                                    bool only_ground,    // mode==ONLY_GROUND
                                                    bool only_pressure,  // mode==ONLY_PRESSURE
                                                    bool archivedData,
                                                    bool metaQuery,
                                                    std::string &err) const throw()
{
  TrackedData *d = 0;
  TrackedData *d2 = 0;  // 2nd choice for ONLY_PRESSURE: first hybrid data match
  TrackedData *d3 = 0;  // 3rd choice for ONLY_PRESSURE: first pressure data match

  for (vector<TrackerBase *>::const_iterator it0 = watch.begin(); it0 != watch.end(); ++it0)
  {
    // 08-Mar-2012 PKi: Release previous data (was acquired once more than released)
    if (d)
      d->Release();

    d = (*it0)->getData_must_release(ot, archivedData, metaQuery);
    if (!d)
      continue;  // go on to next config entry

    // 'd' is acquired for us; we (or our caller) need to '->Release()' it once done.

//  LOG_DEBUG("[%x] Considering raw: %s", (int)pthread_self(), d->getSource().c_str());

    NA_Info info = d->getInfo();

    //---
    // Does it cover the required times?
    //
    const vector<JDay> &times = info.getTimes();
    bool match = true;

    for (vector<JDay>::const_iterator it = required_times.begin(); it != required_times.end(); ++it)
    {
      if (!it->covered_by(times))
      {
        string tmp = it->toString();
        err += string_fmt("\t%s (no validtime %s)\n", info.getExtra_fn().c_str(), tmp.c_str());
        match = false;
        break;  // out of the iterator
      }
    }
    if (!match)
      continue;  // next mask

    //---
    // Does it have the required params?
    //
    if (required_params.size() > 0)
    {
      string missing_param;

      NA_Data *qd = d->Acquire(metaQuery);
      {
        const std::vector<NA_Param> &qd_params = qd->getParams();

        for (vector<ApiParam>::const_iterator it = required_params.begin();
             it != required_params.end();
             ++it)
        {
          if (!it->covered_by(qd_params))
          {
//          LOG_DEBUG("No param found: %s", it->toString().c_str());
            missing_param = it->toString();
            break;
          }
        }
      }
      d->Release();

      if (missing_param != "")
      {
        err +=
            string_fmt("\t%s (no param '%s')\n", info.getExtra_fn().c_str(), missing_param.c_str());
        continue;  // next mask
      }
    }

    //---
    // Does it cover the required levels?
    //
    const vector<NA_Level> &levels = info.getLevels();

    if (only_ground && (!levels.front().isGroundLevel()))
    {
      err += string_fmt("\t%s (not ground)\n", info.getExtra_fn().c_str());
      continue;
    }

    if (required_levels.size() > 0)
    {
      const bool is_hybrid_data = levels.front().isHybridLevel();
      bool all_match_exactly = true;
      string missing_level;

      NA_Data *qd = d->Acquire(metaQuery);
      {
        for (vector<NA_Level>::const_iterator it = required_levels.begin();
             it != required_levels.end();
             ++it)
        {
          bool exact;
          if (!it->covered_by(*qd, exact))
          {
            missing_level = it->toString();
            break;  // out of the iterator
          }
          else if (!exact)
          {
            all_match_exactly = false;  // at least one level would need to be calculated
          }
        }
      }
      d->Release();
      if (missing_level != "")
      {
        err +=
            string_fmt("\t%s (no level '%s')\n", info.getExtra_fn().c_str(), missing_level.c_str());
        continue;  // next mask
      }

      // Any match with ONLY_GROUND or ONLY_HYBRID is good; use it.

      if (only_pressure && (!all_match_exactly))
      {
        // Keep this one as a secondary (or third) alternative
        //
        if (is_hybrid_data)
        {
          if (!d2)
            d2 = d;  // first hybrid data we found
        }
        else
        {
          if (!d3)
            d3 = d;  // first pressure data we found
        }
//      LOG_DEBUG0("\tstill looking for perfect pressure data");

        // 08-Mar-2012 PKi: Keep the data when needed to release it
        if ((d == d2) || (d == d3))
          d = 0;

        continue;
      }
    }

    if (d)
    {
      if (d2)
        d2->Release();
      if (d3)
        d3->Release();
      return d;
    }

  }  // for all entries

  // 08-Mar-2012 PKi: Release last data (was acquired once more than released)
  if (d)
    d->Release();

  // Return the 2nd or 3rd best choice (if any)
  //
  if (d2)
  {
    if (d3)
      d3->Release();
    return d2;
  }
  else if (d3)
  {
    return d3;
  }

  return nullptr;  // 'err' has the collected reasons
}

/*
 * Get a table of available origintimes, most recent first.
 *
 * Note that multiple data files _can_ have the same origintime (if there are
 * multiple filemasks in the track). They can have other properties (s.a.
 * geographical area) that differ. Each origintime is reported only once.
 *
 * Note: Wiping of underlying data can happen while the list of origintimes we've
 *      returned is being used.
 */
vector<JDay> Q3Engine::Track::getOriginTimes(
    const TrackerBase::OriginTimeQuery originTimeQuery) const throw()
{
  vector<JDay> vec;

  for (vector<TrackerBase *>::const_iterator it = watch.begin(); it != watch.end(); ++it)
  {
    (*it)->addOriginTimes_unsorted(vec, originTimeQuery);
  }
  sort_descending(vec, true /*cut duplicates*/);

  return vec;
}

vector<TrackedData *> Q3Engine::Track::getOriginTimeDatas(const JDay &ot) const throw()
{
  vector<TrackedData *> vec;

  for (vector<TrackerBase *>::const_iterator it = watch.begin(); it != watch.end(); ++it)
  {
    (*it)->addDatas_must_release(ot, vec);
  }

  return vec;
}

/*
* Returns an origintime for run 'x' (0,-1,...).
*
* Returns:
*       For tracks with predictable runs ('run_secs' >0):
*           latest origintime - 'abs(x)'*'run_secs'  (whether data exists for such run or not)
*           0 if there is no data that old
*
*       For tracks with unpredictable runs ('run_secs'==0):
*           origintime of the latest, 2nd latest, ... run (which always exists)
*           0 for no data (at all, or not so many origintimes available)
*/
JDay Q3Engine::Track::getOriginTimeOfRun_(int x, NA_Level::Type &leveltype) const throw(E_USAGE)
{
  assert(x <= 0);
  const unsigned abs_x = (unsigned)(-x);

  // 'x'==0 is always treated the same (fastest)
  //
  if ((run_secs > 0) || (x == 0))
  {
    JDay ot_last;   // latest origintime of all tracks
    JDay ot_first;  // earliest -''-
    NA_Level::Type lt_last = NA_Level::NO_LEVEL;

    for (vector<TrackerBase *>::const_iterator it = watch.begin(); it != watch.end(); ++it)
    {
      // Note: Tracks may be empty, in which case the returned JDay object
      //       is 'empty' (test with boolean operator).
      //
      // BS-1973: Track can contain data with multiple level types, and each type can have different
      // latest origintime. To take leveltype into account when needed, pass on the required type
      // (which is NO_LEVEL if leveltype does not matter) to getLastOriginTime_fast()
      //
      NA_Level::Type lt = leveltype;
      JDay ot = (*it)->getLastOriginTime_fast(lt);
      if (ot &&
          (
           (!ot_last) || (ot > ot_last) ||
           (
            (ot == ot_last) &&
            (leveltype == NA_Level::PRESSUREORHYBRID_LEVEL) &&
            (lt_last == NA_Level::HYBRID_LEVEL)
           )
          )
         )
      {
        ot_last = ot;
        lt_last = lt;
      }

      JDay ot2 = (*it)->getFirstOriginTime_fast();
      if (ot2 && ((!ot_first) || (ot2 < ot_first)))
      {
        ot_first = ot2;
      }
    }

    if (ot_last)
    {
      JDay ot_run = ot_last.add_secs(-(abs_x * run_secs));
      if (ot_run >= ot_first)
      {
        if (leveltype == NA_Level::PRESSUREORHYBRID_LEVEL)
          leveltype = lt_last;

        return ot_run;
      }
    }
  }
  else
  {  // Unpredictable runs ('run_secs'==0), 'x'<0
    vector<JDay> vec;

    for (vector<TrackerBase *>::const_iterator it = watch.begin(); it != watch.end(); ++it)
    {
      (*it)->addOriginTimes_unsorted(vec);
    }
    sort_descending(vec, true);  // remove duplicates

    if (abs_x < vec.size())
    {
      return vec[abs_x];
    }
  }

  return JDay();  // no data that old
}

/*---=== TrackProxy ===---*/

LuaNew_ID TrackProxyBind::ID;

static unsigned char track_proxy_chunk[] =
#include "track_proxy.lch"

    /*
    * Set up a metatable (at [-1]) and return our ID.
    */
    void TrackProxyBind::setup(lua_State * L) /*throw(E_FATAL, E_BUG)*/
{
  assert(lua_istable(L, -1));

  L_GROW(2);

  // Running 'track_proxy.lua' (embedded) gives us the "__index" metamethod
  //
  int st = luaL_loadbuffer(
      L, (char *)track_proxy_chunk, sizeof(track_proxy_chunk), nullptr /*from precompiled*/);
  if (st != 0)
  {
    throw E_LOG_OUT_OF_MEMORY();  // out of memory (since it's precompiled)
  }

  st = lua_pcall(L, 0 /*args*/, 1 /*return values*/, 0 /*no error func needed*/);
  //
  // 0 (ok)
  // LUA_ERRRUN: runtime error
  // LUA_ERRMEM: no memory
  // (LUA_ERRERR: not for us)

  if (st != 0)
  {
    throw E_LOG_BUG("%s", lua_tostring(L, -1));
  }

  // [-1]: function ('__index' method for 'TrackProxyBind' objects)

  L_ASSERT(lua_isfunction(L, -1));

  // Metamethods
  //
  lua_pushliteral(L, "__index");
  lua_insert(L, -2);
  // [-1]: function 'trackproxy_index'
  // [-2]: "__index"
  //
  lua_settable(L, -3);

  lua_pushliteral(L, "__call");
  lua_pushcfunction(L, call);
  lua_settable(L, -3);
}

/*
* [raw_ud] [,err_str](*)= __call( track_proxy_ud, [{ [origintime=jday_ud|time_str|int|true,]
*                                               [times=jday|time_str|{jday|time_str [, ...]}],
*                                               [ground=true,]
*                                               [hybrid=true|uint|{uint, ...},]
*                                               [hpa=true|number|{number, ...},]
*                                               [height=true,]
*                                               [flight=true|uint|{uint, ...},] (**)
*                                               [sounding=true,]
*                                               [params=str|{str, ...},]
*                                              }] )
*
* { [epoch_uint [, ...]] }= __call( track_proxy_ud, "origintimes" )
*
* secs_uint= __call( track_proxy_ud, "runs" )
*
* time: YYYYMMDDHHMMSS (string or number) or seconds since epoch
*
* origintime: non-positive integer for latest (0), 2nd latest (-1) and so on origintime
*             'true' for any origintime (latest one that otherwise gives a match)
*             by default: _G["origintime"] or 'true' (progress backwards)
*
* ground:   Request to have data with ground level
* hpa:      Request to have data with pressuse levels (true=any level or certain level(s))
* hybrid:   Request to have data with hybrid levels (true=any level or certain level(s))
* height:   Request to have best data for height usage (and demand 'Z' param, optionally)
* sounding: Request to have sounding data (and demand 'P' param, optionally)
*
* [raw_ud] [,err_str](*)= __call( track_proxy_ud )     -- same as '__call{}' (all fields default)
*
* Returns the file object that carries the particular data (all levels, all times,
* no interpolations).
*
* If parameters are fine but no such data is available, the 'nil, err_string' mechanism
* is used. For plain wrong parameters, traditional Lua errors are given.
*
* NOTE: THE METQU CORE (see 'Raw.cpp') NOW HAS SIMILAR CAPABILITIES WITHIN IT;
*       IF YOU CHANGE SOMETHING IN HERE, CHECK THAT CODE AS WELL. Or implement
*       the Q3 engine part with its help?   --AKa 24-Sep-2009
*
* (*):  Returning the error message is dependent on 'CONFIG_TRACK_ERRORS_ENABLED' configuration
*       switch. If it is defined, the call will create Lua errors internally. If 'false', the
*       "nil,err_str" idiom is being applied.
*
* (**): Demanding 'flight=true' is valid only if 'CONFIG_FLIGHT_LEVELS_API' is defined.
*       Otherwise, script is expected to use 'hpa=true' and 'hpa= { fl_hpa(uint), ... }'.
*/
int TrackProxyBind::call(lua_State *L) throw(E_ERROR)
{
  if (lua_type(L, 2) != LUA_TSTRING)
  {
    proto(L,
          "TrackProxy, [{ origintime=[jday|time_str|int|true],"
          "times=[jday|time_str|{jday|time_str,...}],"
          "ground=[true],"
          "hybrid=[true|uint|{uint,...}],"
          "hpa=[true|number|{number,...}],"
          "height=[true|number|{number,...}],"
          "sounding=[true],"
#ifdef CONFIG_FLIGHT_LEVELS_API
          "flight=[true|uint|{uint,...}],"
#endif
          "params=[string|{string,...}]"
          "}]");
  }
  else
  {
    proto(L, "TrackProxy,string");
  }

  const TrackProxy *proxy = TrackProxy::instance(L, 1);
  const Q3Engine::Track &my = *(proxy->getTrack());

  if (!(proxy->getAlias().empty()))
    LOG_WARNING("Warning: using track alias %s (%s)",
                my.getName().c_str(), proxy->getAlias().c_str());

  JDay ot_absolute;     // origintime as YYYYMMDDHHMMSS (or NOW, TODAY)
  bool any_ot = false;  // reel back in time; any origintime that otherwise matches will do

  // Check the level requirement for sanity etc.
  //
  enum level_mode
  {
    UNDEFINED = 0,  // default: restrict like ONLY_GROUND
    ONLY_GROUND,    // ground level only
    HYBRID,         // hybrid level explicitly requested (may also have pressure level requests)
    ONLY_PRESSURE,  // Pressure levels only (or flight levels; they mean the same). Try to find
                    // exact,
    // if not interpolate from hybrid data if supports pressure data, if not interpolate
    // from pressure data.
    BEST_VERTICAL,  // Prefer matches with hybrid data (better vertical resolution) if
    // the hybrid data can be used for height calculations. Otherwise, use pressure data.
    ONLY_SOUNDING  // Sounding levels only
  } mode = UNDEFINED;

  /*
  * Times, levels and parameters required for the raw data to have.
  */
  vector<JDay> required_times;
  vector<ApiParam> required_params;
  vector<NA_Level> required_levels;
  string err;
  TrackedData *d = 0;  // (must be here to avoid goto's past a variable declaration)

  bool demand_z_param = false, demand_p_param = false, metaQuery = false;

  int otindex = 1;     // Set to <= 0 if relative origintime=n (<= 0) is given

  if (lua_gettop(L) >= 2)
  {
    /*
    * Read properties
    */
    const char *s = lua_tostring(L, 2);
    if (s)
    {
      TrackerBase::OriginTimeQuery originTimeQuery = TrackerBase::OriginTimeQuery::NA;
      if (strcmp(s, "origintimes") == 0)
        originTimeQuery = TrackerBase::OriginTimeQuery::Current;
      else if (strcmp(s, "archorigintimes") == 0)
        originTimeQuery = TrackerBase::OriginTimeQuery::Archived;
      else if (strcmp(s, "allorigintimes") == 0)
        originTimeQuery = TrackerBase::OriginTimeQuery::All;

      if (originTimeQuery != TrackerBase::OriginTimeQuery::NA)
      {
        vector<JDay> v = my.getOriginTimes(originTimeQuery);

        lua_newtable(L);
        for (unsigned i = 0; i < v.size(); i++)
        {
          lua_pushinteger(L, i + 1);
          new (L) JDay(v[i]);
          lua_settable(L, -3);
        }
        return 1;
      }
      else if (strcmp(s, "runs") == 0)
      {
        lua_pushinteger(L, my.getRunSecs());
        return 1;
      }
      // fall through and give "expected table, got ..." error
    }

    if (!lua_istable(L, 2))
    {
      luaL_error(L, "Expected table, got %s", L_typename(2));
    }

    // Iterate the table keys
    //
    lua_pushnil(L);  // first key
    while (lua_next(L, 2))
    {
      // [-2]: key
      // [-1]: value

      const char *key = lua_tostring(L, -2);
      if (!key)
      {
      BAD_KEY:
        luaL_error(L, "Unexpected key: %s", L_string_or_typename(-2));
      }

      int v_idx = lua_gettop(L);

      // Note: Bad values in origintime, times, level and params are NOT to be
      //      treated with Lua error. Return nil,err instead.
      //      This allows upper levels (those handing i.e. 'HIR.xxx') to not need
      //      'pcall()' - which is a heavy operation. --AKa 25-Jan-10
      //
      try
      {
        if (strcmp(key, "origintimes") == 0)
        {
          ot_absolute = JDay(L, v_idx);
          if (!ot_absolute)
            goto BAD_ORIGINTIME;

          vector<TrackedData *> v = my.getOriginTimeDatas(ot_absolute);
          NA_Level default_level;

          lua_newtable(L);
          for (unsigned i = 0; i < v.size(); i++)
          {
            lua_pushinteger(L, i + 1);
            new (L) Raw(v[i], default_level);
            lua_settable(L, -3);
          }
          return 1;
        }
        else if (strcmp(key, "origintime") == 0)
        {
          ot_absolute = JDay(L, v_idx);
          if (!ot_absolute)
          {
            if (lua_isboolean(L, v_idx) && lua_toboolean(L, v_idx))
            {
              any_ot = true;  // try origintimes from latest to oldest, until a match
            }
            else if (lua_isnumber(L, v_idx))
            {
              double dd = lua_tonumber(L, v_idx);
              if ((floor(dd) == dd) && dd <= 0.0)
              {
                /* Do not select origintime yet, get all keys first (taking leveltype etc into account too)
                //
                // Find by relative origintime (0,-1,...)
                //
                ot_absolute = my.getOriginTimeOfRun_((int)dd);  // [0] is latest ot

                // Empty 'ot' means there is absolutely no data
                //
                if (!ot_absolute)
                {
                  err = "no raw data";
                  goto ERROR;
                }
                */

                otindex = (int) dd;
              }
              else
              {
                goto BAD_ORIGINTIME;
              }
            }
            else
            {
            BAD_ORIGINTIME:
              err = string_fmt("Bad origintime: %s", L_string_or_typename(v_idx));
              goto ERROR;
            }
          }
        }
        else if (strcmp(key, "times") == 0)
        {
          required_times = vector_of_times(L, v_idx);  // may throw E_USAGE
        }
        else if (strcmp(key, "params") == 0)
        {
          required_params = vector_of_apiparams(L, v_idx);  // may throw E_USAGE
        }
        else if (strcmp(key, "metaquery") == 0)
        {
          metaQuery = true;

#ifdef CONFIG_FLIGHT_LEVELS_API
        }
        else if (strcmp(key, "flight") == 0)
        {
          // This call takes care of conversion to 'hpa' level(s)
          //
          one_or_many_levels(L, v_idx, key, required_levels);  // may throw E_USAGE

          // This demands best vertical resolution but unlike 'height=true', we don't require Z
          // param
          // to exist.
          //
          // 25-Aug-2011 PKi: Flight level handling analogical to pressure level (leaving mode
          // untouched)

          // mode= BEST_VERTICAL;

          ;
#endif
        }
        else
        {
          NA_Level::Type lt = NA_Level::lt_enum(key);
          if (lt)
          {
            one_or_many_levels(L, v_idx, key, required_levels);  // may throw E_USAGE

            // 23-Nov-2012 PKi: Height value query support
            //
            if (lt == NA_Level::HEIGHT_LEVEL)
              demand_z_param = true;
            else if (lt == NA_Level::SOUNDING_LEVEL)
              demand_p_param = true;
          }
          else
          {
            goto BAD_KEY;  // leads to Lua error
          }
        }
      }
      catch (const E_USAGE &e)
      {
        // Some of 'times', 'levels', 'params' required by the caller were of bad name.
        //
        err = e.what_nosource();  // just the message, no file & line
        goto ERROR;
      }

      lua_pop(L, 1);  // remove the value
    }                 // while( lua_next() )
  }

  if (demand_z_param)
  {
    mode = BEST_VERTICAL;

#ifdef HEIGHT_TRUE_PULLS_IN_Z
    required_params.push_back(Z);
#endif
  }

  if (demand_p_param)
  {
    required_params.push_back("P");
  }

  for (vector<NA_Level>::const_iterator it = required_levels.begin(); it != required_levels.end();
       ++it)
  {
    if ((it->isGroundLevel() && (mode != UNDEFINED)) || (mode == ONLY_GROUND))
    {
      err = "Cannot require ground level and some other level";
      goto ERROR;
    }
    else if ((it->isSoundingLevel() && (mode != UNDEFINED)) || (mode == ONLY_SOUNDING))
    {
      err = "Cannot require sounding levels and some other level";
      goto ERROR;
    }

    if (it->isGroundLevel())
    {
      mode = ONLY_GROUND;
    }
    else if (it->isHybridLevel())
    {
      // Having one requirement a hybrid level forces all other levels to be calculated
      // from it.
      //
      mode = HYBRID;
    }
    else if (it->isPressureLevel())
    {  // (also flight levels get here; stored as pressure levels)
      // HYBRID and BEST_VERTICAL remain; we can calculate pressures from such
      //
      // NOTE: We can ONLY calculate pressure from hybrid level if ':1' parameter is present.
      //
      if (mode == UNDEFINED)
      {
        mode = ONLY_PRESSURE;
      }

      // 23-Nov-2012 PKi: Height value query support
      //
    }
    else if (it->getType() == NA_Level::HEIGHT_LEVEL)
    {
      // HYBRID and BEST_VERTICAL remain
      //
      ;
    }
    else if (it->isSoundingLevel())
    {
      mode = ONLY_SOUNDING;
    }
    else
    {
      throw E_LOG_BUG0("Unexpected level type");  // shouldn't happen
    }
  }

  if (mode == UNDEFINED)
  {
    mode = ONLY_GROUND;
  }

  // Take default origintime from globals (if any)
  //
  if ((otindex > 0) && (!any_ot) && (!ot_absolute))
  {
    lua_pushliteral(L, "origintime");
    lua_rawget(L, LUA_GLOBALSINDEX);
    ot_absolute = JDay(L, -1);
    lua_pop(L, 1);

    if (!ot_absolute)
    {
      any_ot = true;
    }
  }

  assert((otindex <= 0) || any_ot || ot_absolute);

  // If required, loop 0,-1,... until there is a match (or none, at all)
  //
  if (any_ot)
  {
    NA_Level::Type leveltype = NA_Level::NO_LEVEL;

    for (int run = 0; true; run--)
    {
      ot_absolute = my.getOriginTimeOfRun_(run, leveltype);
      if (!ot_absolute)
        break;  // checked all data ('d' remains nullptr)

      // LOG_DEBUG( "[%x] getData_must_release_ run %d", (int) pthread_self(), run );
      d = my.getData_must_release_(ot_absolute,
                                   required_times,
                                   required_params,
                                   required_levels,
                                   mode == ONLY_GROUND,
                                   mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                   false,
                                   metaQuery,
                                   err);
      if (d)
        break;  // Got it!
    }
  }
  else if (otindex <= 0)
  {
    // For vertical nonsounding data try pressure data (primary to search for exact pressure match)
    // or hybrid data first whichever is newer
    //
    // getOriginTimeOfRun_ sets the selected leveltype (pressure or hybrid) when called
    // with PRESSUREORHYBRID_LEVEL
    //
    // If first data/leveltype does not match the query, try the other leveltype

    NA_Level::Type leveltype(mode == ONLY_SOUNDING ? NA_Level::NO_LEVEL
                             : mode == ONLY_GROUND ? NA_Level::GROUND_LEVEL
                             : mode == ONLY_PRESSURE ? NA_Level::PRESSUREORHYBRID_LEVEL
                             : NA_Level::HYBRID_LEVEL);

    for (int v = 0; (v < 2); v++)
    {
      ot_absolute = my.getOriginTimeOfRun_(otindex, leveltype);

      if (ot_absolute)
      {
        // LOG_DEBUG( "[%x] getData_must_release_ run %d", (int) pthread_self(), run );
        d = my.getData_must_release_(ot_absolute,
                                     required_times,
                                     required_params,
                                     required_levels,
                                     mode == ONLY_GROUND,
                                     mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                     false,
                                     metaQuery,
                                     err);

        if (d || ((leveltype != NA_Level::PRESSURE_LEVEL) && (leveltype != NA_Level::HYBRID_LEVEL)))
          break;

        // Try with the other (pressure or hybrid) leveltype

        if ((v == 0) && (leveltype == NA_Level::HYBRID_LEVEL))
          leveltype = NA_Level::PRESSURE_LEVEL;
        else
          leveltype = NA_Level::HYBRID_LEVEL;
      }
      else
        break;
    }
  }
  else
  {
    // Have a precise 'ot' to check
    //
    // Note: currently (by passing !metaQuery to my.getData_must_release_) we do not support/allow
    // meta query for archived data.
    // Lua side metadataquery() loops thru all track's 'normal' origintimes, and archived data
    // having the given origintime
    // could be selected by my.getData_must_release_()
    //
    // LOG_DEBUG( "[%x] getData_must_release_ abs %s", (int) pthread_self(),
    // ot_absolute.toString().c_str() );
    d = my.getData_must_release_(ot_absolute,
                                 required_times,
                                 required_params,
                                 required_levels,
                                 mode == ONLY_GROUND,
                                 mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                 !metaQuery,
                                 metaQuery,
                                 err);

    // Special treatment for randomly originated data: look for a nearby origintime
    //
    if ((!d) && (my.getRunSecs() == 0))
    {
      // Not exactly on a certain origintime, look at its surroundings.
      //
      // If 'ot' minutes==0, scan earlier hour backwards 59..01
      // It 'ot' minutes>0, scan ongoing hour back and forth (1..60) to
      //      find the _nearest_ available data.
      //
      string err_ignored;
      JDay ot1, ot2;  // time range of our swipe

      ot_absolute = JDay(ot_absolute.year(),
                         ot_absolute.month(),
                         ot_absolute.day(),
                         ot_absolute.hour(),
                         ot_absolute.min(),
                         0 /*secs*/);

      unsigned mm = ot_absolute.min();
      if (mm == 0)
      {
        // Sweep earlier hour 59..01 minutes
        //
        for (mm = 59; mm >= 1; --mm)
        {
          d = my.getData_must_release_(ot_absolute.sub_mins(60 - mm),
                                       required_times,
                                       required_params,
                                       required_levels,
                                       mode == ONLY_GROUND,
                                       mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                       true,
                                       metaQuery,
                                       err_ignored);
          if (d)
            break;  // match!
        }

        ot1 = ot_absolute.sub_mins(59);
        ot2 = ot_absolute;
      }
      else
      {
        // Scan back/forth within 1..60 range, nearest entries first
        //
        const unsigned step_rev_last = mm - 1;
        const unsigned step_fwd_last = 60 - mm;

        for (unsigned step = 1; step < 60; step++)
        {
          if (step <= step_rev_last)
          {
            d = my.getData_must_release_(ot_absolute.sub_mins(step),
                                         required_times,
                                         required_params,
                                         required_levels,
                                         mode == ONLY_GROUND,
                                         mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                         true,
                                         metaQuery,
                                         err_ignored);
            if (d)
              break;
          }
          if (step <= step_fwd_last)
          {
            d = my.getData_must_release_(ot_absolute.add_mins(step),
                                         required_times,
                                         required_params,
                                         required_levels,
                                         mode == ONLY_GROUND,
                                         mode == ONLY_PRESSURE || mode == BEST_VERTICAL,
                                         true,
                                         metaQuery,
                                         err_ignored);
            if (d)
              break;
          }
        }

        ot1 = ot_absolute.sub_mins(mm - 1);  // hh:01 ('mm'>=1)
        ot2 = ot1.add_mins(59);              // (hh+1):00
      }

      // When giving an error, mention all the times we've seen through (lowest first).
      //
      if (!d)
      {
        string ot1_s = ot1.toString();
        string ot2_s = ot2.toString();
        err = string_fmt("No matching data (origintimes %s .. %s)", ot1_s.c_str(), ot2_s.c_str());
      }
    }
  }

  if (d)
  {
    NA_Level default_level;  // DEFAULT_LEVEL type
    if (required_levels.size() == 1)
    {
      default_level =
          required_levels[0];  // used if later creating a 'Grid' without level specifier
    }

    new (L)
        Raw(d, default_level, MatrixPos::ZERO, metaQuery);  // use it - GC will call 'd->Acquire()'
    return 1;
  }
  else if (err == "")
  {
    // no data found, at all
    //
    if (ot_absolute)
    {
      string ot_s = ot_absolute.toString();
      err = string_fmt("No matching data (origintime %s)", ot_s.c_str());
    }
    else
    {
      err =
          "No data at all";  // Otherwise we should have gotten a reason in 'err' why it wasn't used
    }
    goto ERROR;
  }
  else
  {
    // Remove last newline
    //
    err = "No matching data (tried these):\n" + err.substr(0, err.length() - 1);
    goto ERROR;
  }

ERROR:
#ifdef CONFIG_TRACK_ERRORS_ENABLED
  luaL_error(L, err.c_str());
  return 0;  // never
#else
  return L_nilerr(err.c_str());
#endif
}
