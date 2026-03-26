/*
* LATLON.CPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Revised:  22-Oct-10 AKa
*/
#include "LatLon.h"
#include "Proto.h"

#include "MemMatrix.h"
#include "Tools.h"
#include "Projection.h"

// Proj4 headers define MIN and MAX as macros, which is plenty stupid.
//
const LatLon LatLon::MIN( -90, -180 );
const LatLon LatLon::MAX( 90, 180 );

#include <vector>

using namespace std;

static const int AT_THE_RIM= -1;

LuaNew_ID LatLonBind::ID;

// Note: Don't use 'DEG_TO_RAD' name - defined by Proj4 headers
//
#define DEG2RAD(deg)  ( (M_PI/180.0) * (deg) )


/*---=== Helpers ===---*/

//===================================================================
/*
Copyright 2001, softSurfer (www.softsurfer.com)

This code may be freely used and modified for any purpose
providing that this copyright notice is included with it.
SoftSurfer makes no warranty for this code, and cannot be held
liable for any real or imagined damage resulting from its use.
Users of this code must verify correctness for their application.
*/

// isLeft(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2 on the line
//            <0 for P2 right of the line
//    See: the January 2001 Algorithm "Area of 2D and 3D Triangles and Polygons"
//
static inline double
isLeft( const LatLon &P0, const LatLon &P1, const LatLon &P2 )
{
    return (P1.getLon() - P0.getLon()) * (P2.getLat() - P0.getLat()) - 
           (P2.getLon() - P0.getLon()) * (P1.getLat() - P0.getLat());
}

// cn_PnPoly(): crossing number test for a point in a polygon
//      Input:   P = a point,
//               V[] = vertex points of a polygon
//      Return:  0 = outside, 1 = inside
// This code is patterned after [Franklin, 2000]
//
/* not used
static unsigned
cn_PnPoly( const LatLon &P, const vector<LatLon> &V )
{
    unsigned cn = 0;    // the crossing number counter

    LatLon prev= V.back();

    // loop through all edges of the polygon
    //
    for( vector<LatLon>::const_iterator it= V.begin();
        it != V.end();
        ++it ) {    // edge from 'prev' to '*it'
       if (((prev.lat <= P.lat) && (it->lat > P.lat))    // an upward crossing
        || ((prev.lat > P.lat) && (it->lat <= P.lat))) { // a downward crossing
            // compute the actual edge-ray intersect x-coordinate
            double vt = (P.lat - prev.lat) / (it->lat - prev.lat);
            if (P.lon < prev.lon + vt * (it->lon - prev.lon)) // P.lon < intersect
                ++cn;   // a valid crossing of lat=P.lat right of P.lon
        }
        prev= *it;
    }
    return cn&1;    // 0 if even (out), and 1 if odd (in)
}
*/

// wn_PnPoly(): winding number test for a point in a polygon
//      Input:   P = a point,
//               V[] = vertex points of a polygon
//      Return:  wn = the winding number (=0 only if P is outside V[])
//
static int
wn_PnPoly( const LatLon &P, const vector<LatLon> &V )
{
    int wn = 0;    // the winding number counter

    LatLon prev= V.back();

    // loop through all edges of the polygon
    //
    for( vector<LatLon>::const_iterator it= V.begin();
        it != V.end();
        ++it ) {    // edge from 'prev' to '*it'
        if (prev.getLat() <= P.getLat()) {         // start lat <= P.lat
            if (it->getLat() > P.getLat())      // an upward crossing
                if (isLeft(prev, *it, P) > 0)  // P left of edge
                    ++wn;            // have a valid up intersect
        }
        else {                       // start lat > P.lat (no test needed)
            if (it->getLat() <= P.getLat())     // a downward crossing
                if (isLeft(prev, *it, P) < 0)  // P right of edge
                    --wn;            // have a valid down intersect
        }
        prev= *it;
    }
    return wn;
}

// end of SoftSurfer code
//===================================================================



/*---=== LatLonBind ===---*/

/*
* number= __index( latlon_ud, "lat"|"lon" )
*/
int LatLonBind::index( lua_State *L ) {
    const LatLon &my= *LatLon::instance(L,1);

    const char *s= lua_tostring(L,2);   // key
    if (s) {
        if (strcmp(s,"lat")==0) {
            lua_pushnumber( L, my.lat );
            return 1;
        }

        if (strcmp(s,"lon")==0) {
            lua_pushnumber( L, my.lon );
            return 1;
        }
    }

    luaL_error( L, "Bad index (for LatLon): %s", L_string_or_typename(2) );
    return 0;   // never
}


/*
* bool= __eq( latlon_ud, latlon_ud )
*/
int LatLonBind::eq( lua_State *L ) {
    const LatLon &a= *LatLon::instance(L,1);
    const LatLon &b= *LatLon::instance(L,1);

    lua_pushboolean( L, a==b );
    return 1;
}


/*
* [str]= __tostring( latlon_ud )
*/
int LatLonBind::tostring( lua_State *L ) {
    const LatLon &a= *LatLon::instance(L,1);

    char NS= a.lat >= 0.0 ? 'N':'S';
    char WE= a.lon >= 0.0 ? 'E':'W';

    lua_pushfstring( L, "%f%c %f%c", abs(a.lat), NS, abs(a.lon), WE );
    return 1;
}

/*
* Set up a metatable.
*/
void LatLonBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__eq");
    lua_pushcfunction(L,eq);
    lua_settable(L,-3);

    lua_pushliteral(L,"__tostring");
    lua_pushcfunction(L,tostring);
    lua_settable(L,-3);
}



/*---=== LatLon ===---*/

/*
*/
LatLon::LatLon( double lat_, double lon_, bool allowMissing ) : lat(lat_), lon(lon_) {
    if ( (!allowMissing) || (lat_ != kFloatMissing) || (lon_ != kFloatMissing) )
    {
        if ( !within_range(lat_,lon_) )
        {
            throw E_LOG_USAGE( "Bad latlon value (not in range): %lf %lf", lat_, lon_ );
        }
        INVARIANT();
    }
}


/*
* Read one latlon coordinate userdata object from 'L'.
*
* Returns: true if entry was valid, false if not.
*/
bool LatLon::init_from_ud( lua_State *L, int index ) {

    LatLon *ud= LatLon::instance(L,index);
    if (ud) {
        *this= *ud;
        return true;
    } else {
        return false;
    }
}


/*
* Read one latlon coordinate (or location string) from 'L'.
*
* Errors i.e. if an entry seems like a valid latlon string or table, but there's some
* problem with it (i.e. latlon values out of range). In completely unfamiliar cases, 
* returns 'false'.
*
* Returns: true if entry was valid, false if not.
*/
bool LatLon::parse( lua_State *L, int index, bool errors_always ) {

    index= L_ABS(index);

    const char *s= lua_tostring(L,index);
    
    // See if we can parse the string as number-starting latlon syntax ("yyyN xxxE").
    //
    if (s && isdigit(*s)) {
        lua_pushlightuserdata( L, REG_LATLON_FUNC );
        lua_gettable( L, LUA_REGISTRYINDEX );
            //
            // [-1]: function (from 'latlon.lua')
    
        L_ASSERT( lua_isfunction(L,-1) );
        lua_pushvalue( L, index );  // 2nd ref of the string
    
        int rc= lua_pcall( L, 1 /*params*/, 2 /*retvals*/, 0 /*no errfunc*/ );
        if (rc) {
            // nil, err_str returned
            throw E_LOG_BUG( "Failed to run LATLON parsing: %d %s", rc, lua_tostring(L,-1) );
        }
            // [-1]: lon number (or nil)
            // [-2]: lat number (or nil)

        if (!lua_isnil(L,-1)) {
            if (!set_if_within_range( lua_tonumber(L,-2), lua_tonumber(L,-1) )) {
                luaL_error( L, "Latlon out of range: %s", s );
            }
            lua_pop(L,2);   // remove the numbers from the stack
            return true;
        }
    }
    
    if (errors_always) {
        luaL_error( L, "Bad latlon value: %s", L_string_or_typename(index) );
    }
    return false;
}


/*---=== LatLonList ===---*
*
* Note: Using Newbase 'NFmiAreaMask' (rather 'NFmiMultiPolygonAreaMask' API was turned down, mostly
*       because of its interface complexity but also because it seems incapable of detecting when a
*       point is exactly at the rim.    -- AKa 12-Oct-2009
*
* Note: Rims spanning across the -180/180 longitude are NOT supported. Upper levels could check for
*       them and split the rims into two.
*/

/*
* Initialize from Lua:
*   'latlon_ud | { latlon_ud [, ...] }'
*
* Returns:  PUSH_AS_VALUE   for a single entry
*           PUSH_AS_ARRAY   for 1..n enclosed in a table
*           NONE            for anything else than Latlon
*/
enum LatLonList::e_state LatLonList::init_from_ud( lua_State *L, int index, bool checkClosing ) {
    index= L_ABS(index);

    unsigned n=0;   // >0 if the entries were enclosed in an array

    LatLon ll;
    if (ll.init_from_ud( L, index )) {
        this->push_back(ll);

    } else if (lua_istable(L,index)) {
        L_GROW(1);
    
        // 'index' has a table
        //
        n= lua_objlen(L,index);
    
        for( unsigned i=1; i<=n; i++ ) {
            lua_pushinteger( L,i );
            lua_gettable( L,index );
            
            LatLon ll2;
            if (!ll2.init_from_ud(L,-1)) {
                lua_pop(L,1);
                return NONE;    // table of some other sort
            }
            lua_pop(L,1);
            this->push_back(ll2);
        }
    } else {
        return NONE;    // not latlon, not table
    }

    // Update bounding box
    //
    for( vector<LatLon>::const_iterator it= this->begin();
        it != this->end();
        ++it ) {
        bb_min.lat= min( bb_min.lat, it->lat );
        bb_min.lon= min( bb_min.lon, it->lon );
        
        bb_max.lat= max( bb_max.lat, it->lat );
        bb_max.lon= max( bb_max.lon, it->lon );
    }

    // Remove explicit closing to start point (if such)
    //
    if (checkClosing && (n>1) && (front() == back())) {
        pop_back();
    }

    INVARIANT();
    
    return (n>0) ? PUSH_AS_ARRAY : PUSH_AS_VALUE;
}


/*
* Is a certain coordinate within the area defined by 'LatLonList' (inclusing closure from
* end point to start).
*
* Returns:
*   LatLonList::INSIDE
*   LatLonList::AT_EDGE
*   LatLonList::OUTSIDE
*
* Ref: <http://stackoverflow.com/questions/217578/point-in-polygon-aka-hit-test>
*      <http://softsurfer.com/Archive/algorithm_0103/algorithm_0103.htm>
*      <http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html>
*/
enum LatLonList::e_within LatLonList::within( const LatLon &ll ) const {

    // Fast bounding box check first
    //
    if ((ll.lon < bb_min.lon) || (ll.lon > bb_max.lon) || (ll.lat < bb_min.lat) || (ll.lat > bb_max.lat)) {
        return OUTSIDE;   // obviously outside
    }

    // 'wn_PnPoly' places edges inside/outside, depending on their facing (left/down or right/up). 
    //
    int st= wn_PnPoly( ll, *this );
    return st==0 ? OUTSIDE:INSIDE;     // never AT_EDGE with this algorithm
}


/*---=== Areamask ===---*/

/*
* m_mask= areamask( projection_str, gridsize_pos, value_num, invert_bool, { latlon, ... } [, ...] )
*
* Returns:
*      A mask matrix with values:
*          'value' inside the given area
*          'value'/2 at precise edge points of the area
*          'nan' outside the area
*/
int LatLonList::_areamask( lua_State *L ) {

    // q3.lua 'areamask()' does the 'proto()' calls (we're safe)
    //
#ifndef NDEBUG
    proto( L, "string, MatrixPos, number, bool, { latlon, ... }, ..." );
#endif

    const char *proj= lua_tostring(L,1);
    const MatrixPos &gs= *MatrixPos::instance(L,2);
    double value= lua_tonumber(L,3);
    bool invert= lua_toboolean(L,4);

    const unsigned first_latlon= 5;
    unsigned args= lua_gettop(L);

    L_GROW(2);

    // Matrix to be returned; we begin by counting the number of inside/outside/rim
    // hits as integers, but eventually use this matrix also as the return value.
    //
    MemMatrix &m= * new(L) MemMatrix( gs, 0.0f, NA_Param::UNIT_UNKNOWN_ /*no interpolation*/, proj );

    Projection pr( proj );

    double x_top= gs.getX()-1;
    double y_top= gs.getY()-1;

    for( unsigned i=first_latlon; i<= args; i++ ) {
        LatLonList lll;
        lll.init_from_ud( L, i );

        /*
        * Modifies 'm' accordingly:
        *   +1 for points within (unless they're marked by AT_THE_RIM)
        *   no change for points outside
        *   set to AT_THE_RIM (-1) for points exactly at the rim (which will
        *       be kept throughout subsequent runs)
        */
        for( MatrixIter it(gs); it.within(); ++it ) {
            LatLon ll= pr.latlon( it.getX()/x_top, it.getY()/y_top );   // range (0,0)..(1.1)
    
            if (m[it] != AT_THE_RIM) {
                switch( lll.within(ll) ) {
                    case LatLonList::INSIDE:
                        m[it] += 1.0f;      // inside
                        break;
                    case LatLonList::AT_EDGE:
                        m[it]= AT_THE_RIM;  // exactly at edge
                        break;
                    case LatLonList::OUTSIDE:
                        break;              // nothing                
                }
            }
        }
    }
    
    // 'm' contains integers only. 
    //
    // AT_THE_RIM (<0): values are at rim (always inside)
    // even values are outside
    // odd values are inside

    float value_at_rim= value/2.0;
    float value_inside= invert ? NAN : value;
    float value_outside= invert ? value : NAN;

    for( MatrixIter it(gs); it.within(); ++it ) {
        int v= (int)m[it];
        m[it]= (v<0) ? value_at_rim : (v%2) ? value_inside : value_outside;
    } 

    return 1;
}


/*
* Distance between two LatLon points, over the "great circle".
*
* Ref. http://www.movable-type.co.uk/scripts/latlong.html
*
* Note: 
<<
In fact, when Sinnott published the Haversine formula, computational precision was limited. Nowadays, JavaScript (and most modern computers & languages) use IEEE 754 64-bit floating-point numbers, which provide 15 significant figures of precision. With this precision, the simple spherical law of cosines formula gives well-conditioned results down to distances as small as around 1 metre. In view of this it is probably worth, in most situations, using either the simpler law of cosines or the more accurate ellipsoidal Vincenty formula in preference to Haversine!
<<
*/
static const double R_km = 6371.0;

double LatLon::haversine_great_circle_distance_km( const LatLon &o ) const {
    double lat_rad= DEG2RAD(lat); double o_lat_rad= DEG2RAD(o.lat);
    double lon_rad= DEG2RAD(lon); double o_lon_rad= DEG2RAD(o.lon);

    double aa= sin( (o_lat_rad-lat_rad) /2.0 );
    double bb= sin( (o_lon_rad-lon_rad) /2.0 );

    double a= (aa*aa) + cos(lat_rad) * cos(o_lat_rad) * (bb*bb);
    return 2 * atan2(sqrt(a), sqrt(1.0-a)) * R_km;
}

double LatLon::spherical_great_circle_distance_km( const LatLon &o ) const {
    double lat_rad= DEG2RAD(lat); double o_lat_rad= DEG2RAD(o.lat);
    double lon_rad= DEG2RAD(lon); double o_lon_rad= DEG2RAD(o.lon);
    
    return acos( sin(lat_rad) * sin(o_lat_rad) + 
                 cos(lat_rad) * cos(o_lat_rad) * cos(o_lon_rad-lon_rad) ) * R_km;
}

