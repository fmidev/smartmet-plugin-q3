/*
* PROJ4_PROJECTION.H                        Copyright (c) 2010, Ilmatieteen laitos
*
* Created   28-Oct-10 AKa
* Revised   ...
*/
#ifndef PROJ4_PROJECTION_H
#define PROJ4_PROJECTION_H

#include "Projection.h"

#include <ogr_spatialref.h>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>

#define PI 3.14159265358979323846
#define DEG_TO_RAD (PI / 180)
#define RAD_TO_DEG (180 / PI)
#define pj_strerrno(st) #st

/*
*/
class Proj4_Projection : public Projection_provider {
  public:
    Proj4_Projection( const char *proj_ ) throw (E_USAGE, std::runtime_error);
    
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
    Fmi::CoordinateTransformation pj;
            
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
    }
#endif
};

#endif
    // PROJ4_PROJECTION_H
