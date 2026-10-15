/*
 * SQD_PROJECTION.H                        Copyright (c) 2010, Ilmatieteen
 * laitos
 *
 * Created   28-Oct-10 AKa
 * Revised   ...
 */
#ifndef SQD_PROJECTION_H
#define SQD_PROJECTION_H

#ifndef USE_NEWBASE
#error "This file shouldn't be included when compiling without Newbase."
#endif

#include <memory>
#include <stdexcept>
// required for 'std::auto_ptr' (Centos works without it; Ubuntu must have this)

#include "Projection.h"

#include "newbase/NFmiArea.h"
#include "newbase/NFmiAreaFactory.h"

/*
 * Newbase projections
 */
class SQD_Projection : public Projection_provider {
public:
  SQD_Projection(const char *proj_);
  SQD_Projection(const NFmiArea *area_);
  ~SQD_Projection() = default;

  /*virtual*/ bool at(const LatLon &ll, double &dx, double &dy) const override;

  /*virtual*/ LatLon latlon(double dx, double dy) const override;

  /*virtual*/ Projection_provider *clone_() const override {
    return new SQD_Projection(*this);
  }

  std::string creationPrefix() const override {
    return (strcmp(ap->ClassName(), "kNFmiGdalArea") ? "" : "FMI|");
  }

private:
  SQD_Projection(const SQD_Projection &o); // accessed through '.clone()'
  SQD_Projection &operator=(const SQD_Projection &o) = delete; // not allowed

  // data members
  //
  NFmiAreaFactory::return_type ap;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(ap.get());
  }
#endif
};

#endif
// SQD_PROJECTION_H
