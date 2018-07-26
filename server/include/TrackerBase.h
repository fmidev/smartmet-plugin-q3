/*
* TRACKERBASE.H                 Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Base class for 'Sqd_Tracker' and 'Bz2_Tracker'
*
* Last revised: 12-Mar-2009 /AKa
*/
#ifndef TRACKERBASE_H
#define TRACKERBASE_H

#include "TrackedData.h"
#include "NA_Data.h"

#include "Tools.h"

#include <map>
#include <time.h>

#include <set>
#include <string>
#include <vector>

/*
* Abstract base class for 'Sqd_Tracker' and 'Bz2_Tracker'
*/
class TrackerBase
{
 public:
  virtual ~TrackerBase() = 0;

  // Whether to return origintimes for current, archived or all known data
  enum OriginTimeQuery
  {
    Current,
    Archived,
    All,
    NA
  };

  TrackedData *getData_must_release(const JDay &origin_time,
                                    bool archivedData,
                                    bool metaQuery) throw();
  std::vector<JDay> getOriginTimes_sort() const throw();
  void addOriginTimes_unsorted(std::vector<JDay> &vec,
                               OriginTimeQuery originTimeQuery = Current) const throw();
  void addDatas_must_release(const JDay &ot, std::vector<TrackedData *> &vec) const throw();

  JDay getLastOriginTime_fast() const throw();
  JDay getFirstOriginTime_fast() const throw();

  virtual bool archived() const { return false; }
 protected:
  TrackerBase(unsigned refresh_secs_, unsigned wiping_[], bool relative_uv_, const std::string &trackName_);

  void init();  // to be called by derived constructors, once they're all done

  virtual bool update(std::set<std::string> &seen_already) throw() = 0;

  virtual std::string id_str() const = 0;

  // 09-Mar-2012 PKi: Track name (used for logging only)
  const std::string &getTrackName() const { return trackName; }
 private:
  static void *polling_thread(void *me_v);
  void wait_until_initialized() const;

  JDay getLastOriginTime_LOCKED() const throw();

  // data fields
  volatile bool initialized;
  pthread_t thread_h;

  // Data that has been opened recently. This is being regularily wiped
  // (in server mode) to take away data that has not been used in a while.
  //
  // Data we know exists but hasn't been opened yet has a nullptr pointer
  // value.
  //
 protected:
  mutable Mutex data_m;
  //
  std::map<JDay, TrackedData *> available_data;
  unsigned
      wiping[4];  // wiping cycle for "normal", archived and metadata access, plus an extra slot
                  // for filemask specific setting ("normal" or archived)
  bool relative_uv;  // Set if U and V are relative to the grid

 private:
  const unsigned refresh_secs;  // Look for new archives every N secs (0=no refresh)
                                // Also run wipe cycle while looking at them

  // 09-Mar-2012 PKi: Track name (used for logging only)
  const std::string trackName;
};

#endif
