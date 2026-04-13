/*
 * BZ2_TRACKER.CPP               Copyright (c) 2008-2009, Ilmatieteen laitos
 *
 * Serving archived (.sqd.bz2) data that matches a single file mask.
 *
 * Last revised:
 *       12-Mar-2009 AKa
 */
#include "Bz2_Tracker.h"
#include "Bz2_Cache.h"

#include "SQD_Data.h"

#include "Tools.h"

#include <cassert>
#include <cstring>
#include <errno.h>
#include <limits.h>

#include <sstream>
#include <stdexcept>

#include "LogTools.h"

using namespace std;

static Bz2_Cache *cache; // shared by all threads (one per process)

/* ======= Helpers ======== */

/*
 * Tell whether 'fn' matches a filename 'mask' (which can contain '*' or '?' and
 * '/ ** /' for skipping 1..N directory levels).
 */
static bool file_mask_match(const char *mask, const char *fn) {
  assert(mask && fn);

  // 'p' traverses 'fn'
  // 'r' traverses 'mask'
  //
  const char *p = fn;
  const char *r = mask;

  while (*p && *r) {
    switch (*r) {
    case '*':
      // skip any number of characters (or none) in 'fn' - for the tail to match
      //
      for (unsigned i = 0; i < strlen(p); i++) {
        if (file_mask_match(r + 1, p + i)) {
          return true; // found a matching tail
        }
      }
      return false; // no match

    case '/':
      if (begins_with(r, "/**/")) {
        if (*p != '/')
          return false;

        // Find the last '/' in remaining filename part
        //
        p = strrchr(p + 1, '/');
        if (!p)
          return false; // no remaining slash - no match

        ++p;    // rest is the filename part itself
        r += 4; // skip "/**/"
        continue;
      }
      [[fallthrough]];
    default:
      if ((*r == '?') || (*p == *r)) {
        r++;
        p++;
        continue; // skip one each
      } else {
        return false; // no match
      }
    }
  }
  return (!*p) && (!*r); // Both must have ended at same time
}

/* ======================== Bz2_Tracker ======================== */

/*
 * Called ONCE by 'Q3Engine' when reading the configuration file.
 *
 * 'fn' is nullptr for not having a cache file (we do memory caching anyways).
 */
void Bz2_Tracker::cache_init(const char *fn) {
  if (cache) {
    throw E_LOG_BUG0("Bz2 cache is ALREADY initialized.");
  }
  cache = new Bz2_Cache(fn); // may be nullptr
}

/*
 */
Bz2_Tracker::Bz2_Tracker(const string &fn_abs_mask, uint_t refresh_ms_,
                         unsigned wiping_[], bool relative_uv_,
                         const string_or_null &tmp_pattern_,
                         size_t tmp_threshold_)
    : TrackerBase(refresh_ms_, wiping_, relative_uv_, "TRACK"), base_path(),
      file_mask(), tmp_pattern(tmp_pattern_), tmp_threshold(tmp_threshold_),
      first_round(true), last_ot(0) {
  assert(cache);

  // 'fn_abs_mask' should be "<path>/**/<filemask>", where "/**/" is any
  // number of subdirs in between.
  //
  size_t i = fn_abs_mask.find("/**/");
  if (i == string::npos) {
    throw E_LOG_USAGE("Expecting '/**/' within path %s", fn_abs_mask.c_str());
  }

  base_path = fn_abs_mask.substr(0, i + 1);        // include '/' at the end
  file_mask = string(fn_abs_mask.c_str() + i + 4); // 4 = strlen("/**/")

  /* Launch a thread to list the matching files, and dig out their Origin Times.
   */
  init();

  INVARIANT();
}

/*
 * 'update()' called from 'TrackerBase' polling thread.
 */
bool Bz2_Tracker::update(set<string> &seen_already) throw() {
  assert(cache);

  // On the first round, update 'available_data' from cache. This allows us to
  // be ready for action as fast as possible (with the pre-known data).
  //
  if (first_round) {
    first_round = false;
    if (update_from_cache(seen_already)) {
      return false; // set 'initialized' and call us back
    }
  }

  update_subpath(base_path.c_str(), seen_already);
  return true; // ready to sleep
}

/*
 * First call to 'update()' takes here.
 *
 * Initialize 'available_data' and 'seen_already' by the contents of the cache.
 * Updates 'last_ot'.
 *
 * Returns:  true if a cache file existed
 *           false if we're running with memory caching only (nothing to
 * initialize)
 */
bool Bz2_Tracker::update_from_cache(set<string> &seen_already) throw() {
  assert(cache);

  if (!cache->hasCacheFile()) {
    return false;
  }

  const string id_s = id_str();
  const char *id = id_s.c_str(); // <basepath>**/<filename>

  unsigned count = 0;
  uint64_t t0 = now_ms(); (void)t0;

  {
    ClaimMutex lock(cache->mapping_m);

    for (map<string, JDay>::const_iterator it = cache->mapping.begin();
         it != cache->mapping.end(); ++it) {
      const string fn_s = it->first;
      const char *fn_abs = fn_s.c_str();
      const JDay ot = it->second;

      if (!ot) {
        LOG_WARNING("Bad cache line ignored (OT 0 for %s)", fn_abs);
        continue;
      }

      if (file_mask_match(id, fn_abs)) {
        // We can write 'available_data' without locking, since the object has
        // not been declared initialized, yet.
        //
        // Note: This only initializes a 'TrackedData' entry. It DOES NOT open
        // the file
        //      for extracting its info (vital for keeping launch-to-readiness
        //      times short). Info is extracted on on-demand basis.   --AKa
        //      30-Nov-2009
        //
        available_data[ot] = new TrackedData(fn_abs, ot, tmp_pattern,
                                             tmp_threshold, relative_uv);

        if (ot > last_ot)
          last_ot = ot;

        seen_already.insert(fn_abs);
        ++count;
      }
    }
  }

  LOG_DEBUG("%d initialized from cache in %d ms: %s", count, now_ms() - t0, id);
  return true;
}

/*
 * Go through all subdirs, and subdirs of subdirs of 'path_abs', calling
 * 'cb_file' for each file matching 'mask'.
 *
 * Returns the most recent origintime in the newly found data.
 */
void Bz2_Tracker::update_subpath(const char *path_abs,
                                 set<string> &seen_already) throw() {
  const char *mask = file_mask.c_str();

  assert(cache);
  assert(!first_round);

#ifdef UNIX
  glob_t gbuf;
  /*
   typedef struct {
      size_t gl_pathc;    // Count of paths matched so far
      char **gl_pathv;    // List of matched pathnames.
      size_t gl_offs;     // Slots to reserve in 'gl_pathv'.
   } glob_t;
  */

  /* Depth first (does not really matter)
   */
  int gbuf_i = -1; // first round
  const char *subdir_abs;

  string tmp = string(path_abs) + "/*";

  while ((subdir_abs = glob_fn(tmp.c_str(), gbuf, gbuf_i, true /*dirs*/)) !=
         nullptr)
#else // !UNIX
#error "Not implemented for Win32"
#endif
  {
    update_subpath(subdir_abs, seen_already);
  }

  /* Our level of files
   */
  gbuf_i = -1; // first round
  const char *fn_abs;

  tmp = string(path_abs) + "/" + mask;

  while ((fn_abs = glob_fn(tmp.c_str(), gbuf, gbuf_i)) != nullptr) {
    set<string>::iterator it1 = seen_already.find(fn_abs);
    if (it1 != seen_already.end())
      continue; // old chap (also cached entries are detected here)

    LOG_DEBUG("detected: %s", fn_abs);
    seen_already.insert(fn_abs);

    // New BZ2 file detected
    //
    // Keep 'NA_Info' around so 'TrackedData' can use it from us.
    //
    const NA_Info *info;
    try {
      info = new NA_Info(SQD_Data::read_info(fn_abs));
    } catch (const E_BAD_FILE &) {
      continue; // Origin Time not found; seems like bad data (log has been
                // done)
    }

    JDay ot = info->getOriginTime();
    assert(ot);

    cache->set(fn_abs, ot);

    LOG_OK("Caching: %s (OT %s)", fn_abs, ot.toString().c_str());

    {
      ClaimMutex lock(data_m);
      bool skip = false;

      map<JDay, TrackedData *>::iterator it = available_data.find(ot);
      if (it != available_data.end()) {
        /* Same origin time twice (the latest by filename is what's being used)
         */
        const char *fn_abs2 = it->second->source.c_str(); // the BZ2 filename

        if (strcmp(fn_abs, fn_abs2) < 0) {
          skip = true;
        } else {
          TrackedData *d = it->second;
          if (d->Wipe()) {
            available_data.erase(it); // renders 'it' invalid
            delete d;
          } else {
            LOG_ERROR("Cannot override %s (in use) with later file %s", fn_abs2,
                      fn_abs);
            skip = true; // skip the new one
          }
        }
      }

      if (!skip) {
        // Note: This is NOT taking much time (important since we're within the
        // locks). 'Acquire' will do the actual extraction & mapping or loading
        // of data.
        //
        available_data[ot] = new TrackedData(fn_abs, *info, relative_uv,
                                             tmp_pattern, tmp_threshold);
        if (ot > last_ot)
          last_ot = ot;
      }
    }
    delete info;
  }

  // Pass findings onto disk after each level
  cache->flush();
}
