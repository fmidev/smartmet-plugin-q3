/*
* PROJ4_PROJECTION.H                        Copyright (c) 2010, Ilmatieteen laitos
*
* Created   28-Oct-10 AKa
* Revised   ...
*/
#ifndef PROJ4_PROJECTION_H
#define PROJ4_PROJECTION_H

#include "Projection.h"

#include <projects.h>

/*
*/
class Proj4_Projection : public Projection_provider {
  public:
    Proj4_Projection( const char *proj_ ) throw (E_USAGE);
    
    ~Proj4_Projection();

    /*virtual*/ bool at( const LatLon &ll, double &dx, double &dy ) const;

    /*virtual*/ LatLon latlon( double dx, double dy ) const;

    /*virtual*/ Projection_provider *clone_() const {
        return new Proj4_Projection(*this);
    }

  private:
    Proj4_Projection( const Proj4_Projection & );   // accessed through '.clone_()'
    Proj4_Projection & operator=( const Proj4_Projection &o );      // not allowed

    // data members
    //
    projPJ pj;
            
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant(pj);
    }
#endif
};

#endif
    // PROJ4_PROJECTION_H
