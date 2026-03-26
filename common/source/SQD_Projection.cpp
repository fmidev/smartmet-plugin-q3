/*
* SQD_PROJECTION.CPP                          Copyright (c) 2010, Ilmatieteen laitos
*
* For more info on Newbase projection syntax, see 'newbase/source/NFmiAreaFactory.cpp'
*
<<
 The possible values for the projection part are
 * latlon
 * ykj
 * pkj
 * mercator
 * rotlatlon,polelatitude=-90,polelongitude=0
 * invrotlatlon,polelatitude=-90,polelongitude=0
 * orthographic,azimuth=0
 * stereographic,centrallongitude=0,centrallatitude=90,truelatitude=60
 * gnomonic,centrallongitude=0,centrallatitude=90,truelatitude=60
 * equidist,centrallongitude=0,centrallatitude=90
<<
*/
#include "SQD_Projection.h"
    
#include "newbase/NFmiAreaFactory.h"
#include "newbase/NFmiGrid.h"
#include "newbase/NFmiLocation.h"

using namespace std;


/*---=== Helpers ===---*/



/*---=== SQD_Projection ===---*/

/*
*/
SQD_Projection::SQD_Projection( const char *proj_ ) : Projection_provider(), ap( NFmiAreaFactory::Create( proj_ ) ) {

    INVARIANT();
}

SQD_Projection::SQD_Projection( const NFmiArea *area_ )
 : Projection_provider()
 , ap( area_->Clone() ) 
{
    INVARIANT();
}

/*
*/
/*virtual*/ SQD_Projection::SQD_Projection( const SQD_Projection &o )
    : Projection_provider(), ap( o.ap.get()->Clone() ) {      // using 'NFmiArea::Clone'
    
    INVARIANT();
}


/*
* Get the relative grid coordinates (0..1, 0..1) within the projection for point 'll'.
*
* Returns 'true' if 'dx' and 'dy' were written (whether they're inside projection or not)
*         'false' if 'dx' and/or 'dy' were not written (outside projection)
*
* Note: Possibility of returning 'false' is an optimization for projection implementations
*       where it would be fast to see if something's outside. We can always return 'true'
*       - the caller will still check if 'dx' and 'dy' are within 0..1 range.
*/
/*virtual*/ bool SQD_Projection::at( const LatLon &ll, double &dx, double &dy ) const {

    // TBD: Is this the way to go - should we generate such a grid at constructor or do we
    //      even need such a grid at all?
    //
    const unsigned X_SIZE= 10000;
    const unsigned Y_SIZE= 10000;

    NFmiGrid grid( ap.get(), X_SIZE, Y_SIZE );
    
    NFmiPoint p= grid.LatLonToGrid( ll.getLon(), ll.getLat() );     // yes, lon first

    dx= p.X() / X_SIZE;
    dy= p.Y() / Y_SIZE;
    return true;
}


/*
* Get the LatLon coordinates within the projection: (0,0)..(1,1).
*/
/*virtual*/ LatLon SQD_Projection::latlon( double dx, double dy ) const {

    assert( (dx>=0.0) && (dx<=1.0) && (dy>=0.0) && (dy<=1.0) );

    // TBD: Is this the way to go - should we generate such a grid at constructor or do we
    //      even need such a grid at all?
    //
    const unsigned X_SIZE= 10000;
    const unsigned Y_SIZE= 10000;

    NFmiGrid grid( ap.get(), X_SIZE, Y_SIZE );
    
    NFmiLocation loc( grid.GridToLatLon( dx*X_SIZE, dy*Y_SIZE ) );

    return LatLon( loc.GetLatitude(), loc.GetLongitude() );
}


