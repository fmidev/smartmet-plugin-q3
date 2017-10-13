/*
* PROJ4_PROJECTION.CPP                          Copyright (c) 2010, Ilmatieteen laitos
*
* Ref. http://trac.osgeo.org/proj/wiki/ProjAPI
*/
#include "Proj4_Projection.h"

#include "Tools.h"

using namespace std;

/*
* NOTE: 'pj_init()', 'pj_fwd()', 'pj_inv()' are *deprecated functions* in the Proj4 API. Avoid them.
*/
#define pj_init DONT_USE_pj_init
#define pj_fwd DONT_USE_pj_fwd
#define pj_inv DONT_USE_pj_inv


/*
    Matti.Horttanainen@fmi.fi 4-Nov-2010 on which LatLon projection to use (in FMI):
<<
    Moi
    Koska meillä on Pallo, olen käyttänyt tuollaista
    +proj=longlat +ellps=sphere +a=6371220 +b=6371220
    
    a ja b tuntuvat vähän vaihtelevan...
    
    Joskus muunnoksissa on hyvä käyttää myös
    
    +nadgrids=@null
    
    http://osgeo-org.1803224.n2.nabble.com/Difference-between-nadgrids-null-and-towgs84-0-0-0-td4543036.html
    http://proj.maptools.org/faq.html#sphere_as_wgs84
    
    Matti
<<
*/
const char *LATLON_DECL= "+proj=longlat +ellps=sphere +a=6371220 +b=6371220";


/* The LatLon projection used in FMI.
*/
static const projPJ PJ_LATLON= pj_init_plus( LATLON_DECL );


/*---=== Helpers ===---*/

static projPJ pj_clone( projPJ o_pj ) {
    if (!o_pj) {
        return NULL;
    } else {
        return pj_init_plus( pj_get_def( o_pj, 0 /*options*/ ) );
    }
}


/*---=== Proj4_Projection ===---*/

/*
* Proj4 projection syntax is defined by 'pj_init_plus()' API. 
*
* Samples:
*   "+proj=utm +zone=11 +ellps=WGS84"
*   "+proj=merc +ellps=clrk66 +lat_ts=33"
*   "+proj=tmerc +lon_0 +datum=WGS84"
*
* Ref: 
*   http://trac.osgeo.org/proj/wiki/GenParms
*/

/*
*/
Proj4_Projection::Proj4_Projection( const char *proj_ ) throw(E_USAGE) : Projection_provider(), pj( pj_init_plus(proj_) ) {
    assert(proj_);

    if (!pj) {
        throw E_LOG_USAGE( "Bad projection: %s", proj_ );
    }

    INVARIANT();
}


/*
*/
/*virtual*/ Proj4_Projection::Proj4_Projection( const Proj4_Projection &o )
    : Projection_provider(), pj( pj_clone(o.pj) ) {
    
    INVARIANT();
}


/*
*/
Proj4_Projection::~Proj4_Projection() {
    pj_free(pj);
}


/*
* Get the relative grid coordinates (0..1, 0..1) within the projection for point 'll'.
*
* Returns 'true' if inside the projection (or at the rim); 'false' if outside.
*/
/*virtual*/ bool Proj4_Projection::at( const LatLon &ll, double &dx, double &dy ) const {

    double x_array= ll.getLat() * DEG_TO_RAD;
    double y_array= ll.getLon() * DEG_TO_RAD;

    int st= pj_transform( PJ_LATLON, pj, 1 /*values*/, 1 /*offset between array elements*/, &x_array, &y_array, NULL /*z*/ );
    if (st) {
        throw E_LOG_ERROR( "Proj4 transform error: %s", pj_strerrno(st) );
    }
    
    dx= x_array;
    dy= y_array;

    return true;
}


/*
*/
/*virtual*/ LatLon Proj4_Projection::latlon( double dx, double dy ) const {

    double x_array= dx;
    double y_array= dy;

    int st= pj_transform( pj, PJ_LATLON, 1 /*values*/, 1 /*offset between array elements*/, &x_array, &y_array, NULL /*z*/ );
    if (st) {
        throw E_LOG_ERROR( "Proj4 transform error: %s", pj_strerrno(st) );
    }
    
    return LatLon( x_array * RAD_TO_DEG, y_array * RAD_TO_DEG );
}


