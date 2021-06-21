/*
* SQD_PROJECTION.H                        Copyright (c) 2010, Ilmatieteen laitos
*
* Created   28-Oct-10 AKa
* Revised   ...
*/
#ifndef SQD_PROJECTION_H
#define SQD_PROJECTION_H

#ifndef USE_NEWBASE
# error "This file shouldn't be included when compiling without Newbase."
#endif

#include <stdexcept>
#include <memory>
    // required for 'std::auto_ptr' (Centos works without it; Ubuntu must have this)

#include "Projection.h"

#include "newbase/NFmiArea.h"
#include "newbase/NFmiAreaFactory.h"

/*
* Newbase projections
*/
class SQD_Projection : public Projection_provider {
  public:
    SQD_Projection( const char *proj_ ) throw (std::exception);
    SQD_Projection( const NFmiArea *area_ ) throw (std::exception);

    /*virtual*/ bool at( const LatLon &ll, double &dx, double &dy ) const;

    /*virtual*/ LatLon latlon( double dx, double dy ) const;

    /*virtual*/ Projection_provider *clone_() const {
        return new SQD_Projection(*this);
    }

    std::string creationPrefix() const { return (strcmp(ap->ClassName(),"kNFmiGdalArea") ? "" : "FMI|"); }

  private:
    SQD_Projection( const SQD_Projection &o );  // accessed through '.clone()'
    SQD_Projection & operator=( const SQD_Projection &o );      // not allowed

    // data members
    //
    NFmiAreaFactory::return_type ap;
            
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant(ap.get());
    }
#endif
};

#endif
    // SQD_PROJECTION_H
