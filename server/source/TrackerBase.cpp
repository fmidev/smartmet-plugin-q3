/*
* TRACKERBASE.CPP               Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Last revised:
*       27-Nov-2009 AKa
*       12-Mar-2009 AKa
*/
#include "TrackerBase.h"
#include "Tools.h"
#include "NA_Level.h"
#include "NA_Data.h"

#include <string>
#include <fstream>

#include <algorithm>  // sort
#include <vector>

using namespace std;

/*---=== Helpers ===---*/

/*---=== TrackerBase ===---*/

/*
*/
// 09-Mar-2012 PKi: Track name (used for logging only)
TrackerBase::TrackerBase(unsigned refresh_secs_, unsigned wiping_[], bool relative_uv_, const std::string &trackName_)
    : thread_h(), data_m(), available_data(), refresh_secs(refresh_secs_), relative_uv(relative_uv_), trackName(trackName_)
{
  // NOTE: Must not launch 'thread_h' here yet; derived constructor will
  //      do it via 'init()' (so it is ready for 'update()' callbacks)

  memcpy(wiping, wiping_, sizeof(wiping));
}

/*
* Derived constructor is now done, and allows us to start the update cycle.
*/
void TrackerBase::init()
{
  PTHREAD_CALL(pthread_create, &thread_h, nullptr, polling_thread, (void *)this);
}

/*
* Destruction of 'TrackerBase' (and derived) objects.
*
* Note: Server never gets here, so this is rather theoretical (and not tested).
*/
TrackerBase::~TrackerBase()
{
  PTHREAD_CALL(pthread_cancel, thread_h);
  data_m.Destroy();

  /*
  * Entries in 'available_data' should no longer be in use.
  *
  * We're the only thread now, so can use the '_LOCKED' functions.
  */
  for (map<JDay, TrackedData *>::iterator it = available_data.begin(); it != available_data.end();
       ++it)
  {
    TrackedData *d = it->second;
    if (d)
    {
      d->Wipe_LOCKED();
      delete d;
    }
  }
}

/*
* Polling thread.
*
* Find out periodically whether new data is available, and update 'available_data'.
*
* Note:
*   A FAR better change tracking could be done using 'inotify'
* (http://en.wikipedia.org/wiki/Inotify)
*   on the server that hosts the data files (it does not work over NFS shares)
*   and then notifying others over an "enterprise bus" s.a. ZeroMQ
*   (http://www.zeromq.org).   -- AKa 24-Nov-2009
*/
void *TrackerBase::polling_thread(void *me_v)
{
  TrackerBase *my = (TrackerBase *)me_v;

  // Simple map to avoid handling the same filename twice
  //
  set<string> seen_already;

  const string id = my->id_str();

  unsigned refresh_secs = my->refresh_secs;

  uint64_t ms0 = now_ms();  // Beginning of the update cycle
                            // outside the loop to keep two cycles of .bz2 added up
  // Background loop
  //
  while (true)
  {
    // Calls to 'update' are NOT locked; allows refresh to cause minimal
    // disturbance to server fetches.
    //
    bool tired = my->update(seen_already);
    my->initialized = true;

    // LOG_DEBUG( "Polling done, %s", tired ? "tired":"not tired" );

    // If we're initializing BZ2 data, the first round 'tired' will be 'false'
    // since the initialization wants two rounds (one to get stuff from cache;
    // second to go through the files on disk).
    //
    // There's many ways we can deal with this - just sleep normally or wait
    // until other kinds of (normal) initializations are succesfully done.
    //
    if (!tired)
      continue;  // go on run another 'update()' (no wiping in between)

    if (!refresh_secs)
      break;  // no updates, no wipe (exits the thread)

    uint64_t ms1 = now_ms();

    LOG_OK("Going to sleep (%.3lf secs spent updating data)", (ms1 - ms0) / 1000.0);
    Sleep_ms(refresh_secs * 1000);
    uint64_t ms2 = now_ms();
    LOG_OK("Waking up (slept %.3lf secs)", (ms2 - ms1) / 1000.0);

    /*
    * Wiping round
    *
    * Loop all 'available_data' and try to release their memory mappings.
    *
    * Note: Must be fast here; we're blocking server data fetches.
    */
    {
      ClaimMutex lock(my->data_m);

      for (map<JDay, TrackedData *>::iterator it = my->available_data.begin();
           it != my->available_data.end();
           ++it)
      {
        TrackedData *d = it->second;
        if (!d)
          continue;  // BZ2 entry based on cache information (skip)

        // Release memory mapping if no users and last acquire was old enough.
        // Use global or track specific, or filemask specific wiping cycle (the last array slot) if
        // given
        d->Wipe(my->wiping[my->wiping[3] ? 3 : (my->archived() ? 1 : 0)], my->wiping[2]);
      }
    }

    LOG_DEBUG("Wiping done (%.3lf secs)", (now_ms() - ms2) / 1000.0);
    ms0 = now_ms();
  }

  LOG_WARNING("Exiting update thread (refresh_secs==%d)", refresh_secs);

  return nullptr;  // exits only if 'refresh_secs'==0
}

/*
*/
void TrackerBase::wait_until_initialized() const
{
  while (!initialized)
  {
    Sleep_ms(100);
    LOG_DEBUG("waiting... %p %s", (void *)this, id_str().c_str());
  }
}

/*
* Get pointer to SQD/SQD.BZ2 data for an exact origintime (>0)
* or the most recent only (ot is nondetermined).
*
* The 'ot' nondetermined case is common. Instead of first calling
* 'getOriginTimes()' and then us, we can do it with just one call
* and one lock of 'data_m'.
*
* The returned data is '->Acquire':d by us, to make sure it does not get
* wiped. The caller must '->Release' it when done.
*/
TrackedData *TrackerBase::getData_must_release(const JDay &ot_orig,
                                               bool archivedData,
                                               bool metaQuery) throw()
{
  if (!archivedData && archived())
    return nullptr;

  TrackedData *data = 0;
  JDay ot(ot_orig);
  NA_Level::Type leveltype = NA_Level::NO_LEVEL;

  wait_until_initialized();

  {
    ClaimMutex lock(data_m);

    if (!ot)
    {
      ot = getLastOriginTime_LOCKED(leveltype);
    }

    if (ot)
    {
      map<JDay, TrackedData *>::iterator it = available_data.find(ot);
      if (it != available_data.end())
      {
        data = it->second;
      }

      // 'Acquire()' must be done within the lock.
      //
      // Note: This is the first and important acquire - this WILL fail (with
      //      E_BAD_FILE thrown) if the file has vanished from the disk. If
      //      this succeeds so will the rest, since the file is being taken
      //      into use (and kept in use) already. Linux allows removal of a file
      //      where open handles to its contents still remain valid.
      //
      if (data)
      {
        try
        {
          data->Acquire(metaQuery);  // matching 'Release()' by the caller
        }
        catch (const E_BAD_FILE &e)
        {
          // A file has been removed (or is invalid by its contents). Take it
          // away from 'available_data' and return nullptr
          //
          available_data.erase(it);
          return nullptr;
        }
      }
    }
  }

  return data;
}

/*
* Get the available set of origintimes, sorted to descending order ([0] is latest).
*/
vector<JDay> TrackerBase::getOriginTimes_sort() const throw()
{
  vector<JDay> vec;
  addOriginTimes_unsorted(vec);

  sort_descending(vec);  // latest becomes first
  return vec;
}

/*
* Append the available set of origintimes to an existing vector.
*
* Duplicate entries are not removed.
*/
void TrackerBase::addOriginTimes_unsorted(vector<JDay> &vec,
                                          const OriginTimeQuery originTimeQuery) const throw()
{
  if ((originTimeQuery != TrackerBase::OriginTimeQuery::All) &&
      ((originTimeQuery == TrackerBase::OriginTimeQuery::Archived) != archived()))
    return;

  wait_until_initialized();

  {
    ClaimMutex lock(data_m);
    for (map<JDay, TrackedData *>::const_iterator it = available_data.begin();
         it != available_data.end();
         ++it)
    {
      vec.push_back(it->first);  // JDay
    }
  }
}

void TrackerBase::addDatas_must_release(const JDay &ot, vector<TrackedData *> &vec) const throw()
{
  wait_until_initialized();

  {
    ClaimMutex lock(data_m);
    for (map<JDay, TrackedData *>::const_iterator it = available_data.begin();
         it != available_data.end();
         ++it)
    {
      if (it->first == ot)
      {
        it->second->Acquire();
        vec.push_back(it->second);
      }
    }
  }
}

/*
* Returns the most recent origintime available, or 'empty' (test with 'operator bool') for no data.
*
* This function is FAST and does not cause locks to be required.
*/
JDay TrackerBase::getLastOriginTime_fast(NA_Level::Type &leveltype) const throw()
{
  wait_until_initialized();

  {
    ClaimMutex lock(data_m);
    return getLastOriginTime_LOCKED(leveltype);
  }
}

/*
*/
JDay TrackerBase::getLastOriginTime_LOCKED(NA_Level::Type &leveltype) const throw()
{
  JDay last_ot;
  NA_Level::Type leveltype1, leveltype2, lt = NA_Level::NO_LEVEL;

  if (leveltype == NA_Level::PRESSUREORHYBRID_LEVEL)
  {
    leveltype1 = NA_Level::PRESSURE_LEVEL;
    leveltype2 = NA_Level::HYBRID_LEVEL;
  }
  else
    leveltype1 = leveltype2 = leveltype;

  // Exact origintime must be used to access archived data
  if (!archived())
    for (map<JDay, TrackedData *>::const_iterator it = available_data.begin();
         it != available_data.end();
         ++it)
    {
      JDay ot = it->first;

      // For PRESSUREORHYBRID_LEVEL use pressure (primary) or hybrid data whichever is newer

      if (
          (
           (leveltype == NA_Level::NO_LEVEL) ||
           (leveltype1 == it->second->getLevelType()) || (leveltype2 == it->second->getLevelType())
          ) &&
          (
           (!last_ot) || (ot > last_ot) ||
           (
            (ot == last_ot) &&
            (leveltype == NA_Level::PRESSUREORHYBRID_LEVEL) &&
            (lt == NA_Level::HYBRID_LEVEL)
           )
          )
         )
      {
        last_ot = ot;
        lt = it->second->getLevelType();
      }
    }

  if (leveltype == NA_Level::PRESSUREORHYBRID_LEVEL)
    leveltype = lt;  // pressure or hybrid

  return last_ot;
}

/*
* Returns the oldest origintime available, or 'empty' (test with 'operator bool') for no data.
*
* TBD: Getting this faster (not needing the lock) might be welcome for upper layers.
*/
JDay TrackerBase::getFirstOriginTime_fast() const throw()
{
  JDay first_ot;

  // Exact origintime must be used to access archived data
  if (!archived())
  {
    wait_until_initialized();

    {
      ClaimMutex lock(data_m);

      for (map<JDay, TrackedData *>::const_iterator it = available_data.begin();
           it != available_data.end();
           ++it)
      {
        JDay ot = it->first;
        if ((!first_ot) || (ot < first_ot))
        {
          first_ot = ot;
        }
      }
    }
  }

  return first_ot;
}
