/*
* TRACK.H                    Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Managing a group of file filters given in a Q3 configuration file.
*
* This object also updates its contents regularily, and takes care of releasing
* old data no longer required.
*/
#ifndef Q3_TRACK_H
#define Q3_TRACK_H

#include <vector>
#include <string>

#include "TrackerBase.h"
#include "LuaNew.h"

#include "Q3Engine.h"
#include "JDay.h"

/*
* Class to keep an eye on a set of files (usually from a certain producer).
*
* These entries remain alive throughout the server process. Session Lua
* states access them via 'TrackProxy'.
*/
class Q3Engine::Track
{
 public:
  Track(const std::string &name_,
        time_t run_secs_,
        const char *tmp_pattern_,
        size_t tmp_threshold_,
        const std::string &alias_ = "");
  ~Track();

  TrackedData *getData_must_release_(const JDay &ot,
                                     const std::vector<JDay> &required_times,
                                     const std::vector<ApiParam> &required_params,
                                     const std::vector<NA_Level> &required_levels,
                                     bool only_ground,
                                     bool only_pressure,
                                     bool archivedData,
                                     bool metaQuery,
                                     std::string &err) const throw();

  std::vector<JDay> getOriginTimes(const TrackerBase::OriginTimeQuery originTimeQuery) const
      throw();
  std::vector<TrackedData *> getOriginTimeDatas(const JDay &ot) const throw();

  JDay getOriginTimeOfRun_(int x, NA_Level::Type &leveltype) const throw(E_USAGE);

  unsigned getRunSecs() const { return run_secs; }
  void add(const char *mask_fn, unsigned refresh_secs, unsigned wiping[], bool relative_uv) throw(E_USAGE);

  const std::string &getName() const { return name; }
  const std::string &getAlias() const { return alias; }
 private:
  /* 'watch' is constant after initialization and needs no locking.
   */
  /*const*/ std::vector<TrackerBase *> watch;
  const std::string name;
  const std::string alias; // Track alias name, e.g. 'MEPS' for 'HIR'

  const time_t run_secs;  // difference between subsequent runs (0 = no regular runs)

  const string_or_null tmp_pattern;
  const size_t tmp_threshold;

#ifndef NDEBUG
  void _INVARIANT(const char *, unsigned) const {}
#endif
};

/*
* Session Lua states have proxies like this as their 'HIR', 'EC' etc.
* global variables. They point to the shared 'Track' entries, which
* have a longer lifespan.
*/
class TrackProxy;

struct TrackProxyBind
{
 public:
  static LuaNew_ID ID;  // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "TrackProxy"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef TrackProxy CAST_T;

 private:
  static int call(lua_State *L) throw(E_ERROR);
};

class TrackProxy : public LuaNew<TrackProxyBind>
{
 public:
  TrackProxy(const Q3Engine::Track *t_) : t(t_) { INVARIANT(); }
  ~TrackProxy() {}
  const Q3Engine::Track *getTrack() { return t; }
 private:
  const Q3Engine::Track *t;

  friend class TrackProxyBind;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const { assert_invariant(t); }
#endif
};

#endif
// Q3_TRACK_H
