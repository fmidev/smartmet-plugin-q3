/*
 * SQD_TRACKER.CPP              Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Serving and updating a single file mask's data.
 *
 * NOTE!
 *
 * We use a polling loop instead of more elegant non-polling solutions
 * (s.a. dnotify, inotify) because the data is on an NFS server. Inotify would
 * only work in specific situations (either running on the NFS server itself,
 * or for things changed from the same machine applications runs on). This is
 * not necessarily enough, so polling is used instead.
 *
 * Ref: Wikipedia on inotify: http://en.wikipedia.org/wiki/Inotify
 *
 * Last revised:
 *       12-Mar-2009 AKa
 */
#include "Sqd_Tracker.h"
#include "Tools.h"

#include "SQD_Data.h"

#include "LogTools.h"

#include <cassert>
#include <cstring>

#ifdef UNIX
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace std;

/*---=== Sqd_Tracker ===---*/

/*
 * Check if there's new files matching the pattern, and process them.
 *
 * Return the most recent origintime of newly added files (or 0 for no files
 * added).
 */
bool Sqd_Tracker::update(set<string> &seen_already) throw() {
  set<string> current_files;
  const char *fn_abs;

#ifdef UNIX
  glob_t gbuf;
  int gbuf_i = -1; // first round
  /*
  typedef struct {
  size_t gl_pathc;    // Count of paths matched so far
  char **gl_pathv;    // List of matched pathnames.
  size_t gl_offs;     // Slots to reserve in 'gl_pathv'.
  } glob_t;
  */
  while ((fn_abs = glob_fn(fn_abs_mask.c_str(), gbuf, gbuf_i)) != nullptr)
#else
#error "Not implemented for Win32"
#endif
  {
    // Current set of files to update seen_already
    current_files.insert(fn_abs);

    // Skip if filename known already
    //
    // "inserts val, but only if val doesn't already exist. The return value is
    // an iterator to the element inserted, and a boolean describing whether an
    // insertion took place."
    //
    pair<set<string>::iterator, bool> p = seen_already.insert(fn_abs);
    if (!p.second) {
      const char *fn_f = strrchr(fn_abs, '/');
      if (fn_f && *(fn_f + 1))
        fn_f++;
      else
        fn_f = fn_abs;
      LOG_OK("%s: Seen already(%d: %d/%d): %s", getTrackName().c_str(),
             (int)pthread_self(), (int)seen_already.size(),
             (int)available_data.size(), fn_f);
      continue; // was already there
    }

    // We need to read the SQD file's info to get its origin time.
    // Keep the whole info around, so 'TrackedData' does not need to re-read it.
    //
    const NA_Info *info;
    try {
      info = new NA_Info(SQD_Data::read_info(fn_abs));
    } catch (const E_BAD_FILE &) {
      continue; // not grid data (log has been done)
    }

    JDay ot = info->getOriginTime();
    assert(ot);

    LOG_DEBUG("Loading... %s (ot %s)", fn_abs, ot.toString().c_str());

    /* Add to 'available_data'.
     *
     * Note: There should NOT be two files with same origintime. If there is
     *       we'll use the file with alphabetically later filename.
     */
    bool skip = false;
    {
      ClaimMutex lock(data_m);

      map<JDay, TrackedData *>::iterator it = available_data.find(ot);
      if (it != available_data.end()) {
        /* Same origin time twice (the latest by filename is being used)
         */
        const char *fn_abs2 = it->second->source.c_str(); // SQD filename

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
        // locks). 'Acquire' will do the actual extraction & loading of data.
        //
        try {
          available_data[ot] = new TrackedData(fn_abs, *info, relative_uv);
        } catch (const E_BAD_FILE &e) {
          LOG_ERROR("Exception reading %s (skipping it):\n%s", fn_abs,
                    e.what());
          skip = true;
        }
      }
    }
    delete info;

    if (skip)
      continue;

    LOG_OK("%s(%d: %d): Loading OK: %s", getTrackName().c_str(),
           (int)pthread_self(), (int)available_data.size(), fn_abs);
  } // while(files)

  seen_already.swap(current_files);

  return true; // ready for sleep
}
