/*
 * BZ2_TRACKER.H                 Copyright (c) 2008-2009, Ilmatieteen laitos
 *
 * Serving archived (.sqd.bz2) data that matches a single file mask.
 *
 * Last revised:
 *       25-Nov-2009 AKa
 *       24-Feb-2009 AKa
 */
#ifndef BZ2_TRACKER_H
#define BZ2_TRACKER_H

#include "TrackerBase.h"

#include <set>
#include <string>

/*
 * Class to access archive data (.sqd.bz2)
 */
class Bz2_Tracker : public TrackerBase {
public:
  Bz2_Tracker(const std::string &fn_abs_mask, uint_t refresh_ms,
              unsigned wiping_[], bool relative_uv_,
              const string_or_null &tmp_pattern_,
              size_t tmp_threshold_) throw(E_USAGE);
  ~Bz2_Tracker() {}
  TrackedData *getData_must_release(time_t ot) throw();

  static void cache_init(const char *fn);

  bool archived() const { return true; }
  // private funcs
private:
  /*virtual*/ bool update(std::set<std::string> &seen_already) throw();
  bool update_from_cache(std::set<std::string> &seen_already) throw();
  void update_subpath(const char *path_abs,
                      std::set<std::string> &seen_already) throw();

  /*virtual*/ std::string id_str() const {
    return base_path + "**/" + file_mask;
  }
  // data members
  /*const*/ std::string
      base_path; // i.e.
                 // "/smartmet/archive/pal/querydata/pal/skandinavia/maanpinta/"
  /*const*/ std::string file_mask; // i.e. "*.sqd.bz2"

  const string_or_null tmp_pattern; // control when to extract to disk (and
                                    // memory map) or open into memory
  const size_t tmp_threshold;

  bool first_round;
  JDay last_ot;

#ifndef NDEBUG
  void _INVARIANT(const char *, unsigned) const {}
#endif
};

#endif
