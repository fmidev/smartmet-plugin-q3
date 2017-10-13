/*
* SQD_TRACKER.H               Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Serving and updating a single file mask's data.
*
* Last revised: 12-Mar-2009 /AKa
*/
#ifndef SQD_TRACKER_H
#define SQD_TRACKER_H

#include <string>
#include <map>
#include <vector>

#include <set>

#include "TrackerBase.h"
#include "Tools.h"

/*
* Class to track a certain .SQD file mask; updates 'TrackerBase::available_data'
* when new files are detected.
*/
class Sqd_Tracker : public TrackerBase
{
 public:
  // 09-Mar-2012 PKi: Track name (used for logging only)
  Sqd_Tracker(const std::string &fn_abs_mask_,
              unsigned refresh_ms,
              unsigned wiping_[],
              const std::string &trackName_)
      : TrackerBase(refresh_ms, wiping_, trackName_), fn_abs_mask(fn_abs_mask_)
  {
    init();
  }

  ~Sqd_Tracker() {}
  /*virtual*/ std::string id_str() const { return fn_abs_mask; }
 private:
  // private funcs
  bool update(std::set<std::string> &seen_already) throw();

  // data fields
  const std::string fn_abs_mask;
  // i.e. "/smartmet/data/hirlam/eurooppa/pinta/querydata/*_hirlam_eurooppa_pinta.sqd"
};

#endif
