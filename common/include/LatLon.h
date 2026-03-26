/*
* LATLON.H                            Copyright (c) 2009-10, Ilmatieteen laitos
*
* Revised:  22-Oct-10 AKa
*/
#ifndef LATLON_H
#define LATLON_H

#include <vector>

#include "LuaNew.h"
#include "Tools.h"
#include "LogTools.h"
#include "MatrixPos.h"

class MemMatrix;
class Matrix;

extern /*const*/ void *REG_LATLON_FUNC;

class LatLon;

struct LatLonBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "LatLon"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef LatLon CAST_T;

  private:
    static int index( lua_State *L );
    static int eq( lua_State *L );
    static int tostring( lua_State *L );
};

/*
*/
class LatLon : public LuaNew<LatLonBind> {
  public:
    LatLon() : lat(0.0), lon(0.0) { INVARIANT(); }
    LatLon( double lat_, double lon_, bool allowMissing = false );

    bool operator==( const LatLon &o ) const {
        return (lat==o.lat) && (lon==o.lon);
    }

    static int is( lua_State *L ) {  // for 'proto.LatLon()'
        const LatLon *ll= LatLon::instance(L,1);
        lua_pushboolean( L, ll != nullptr );
        return 1;
    }

    double getLat() const { return lat; }
    double getLon() const { return lon; }

    bool init_from_ud( lua_State *L, int index );
    bool parse( lua_State *L, int index, bool errors_always );

    double haversine_great_circle_distance_km( const LatLon &o ) const;
    double spherical_great_circle_distance_km( const LatLon &o ) const;

  private:
    friend class LatLonList;
    friend class LatLonBind;

    static const LatLon MIN, MAX;

    static bool within_range( double lat_, double lon_ ) {
        return (lat_ >= MIN.lat) && (lat_ <= MAX.lat) &&
               (lon_ >= MIN.lon) && (lon_ <= MAX.lon);
    }

    bool set_if_within_range( double lat_, double lon_ ) {
        if (within_range(lat_,lon_)) {
            lat= lat_; lon= lon_; 
            return true;
        } else {
            return false;
        }
    }

    // data members
    //
    double lat,lon;

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant( within_range(lat,lon) );
    }
#endif
};


/*
* A list of 'lat,lon' coordinates. Either just a collection of unrelated
* coordinates or an area rim description.
*/
class LatLonList : public std::vector<LatLon> {
  public:
    LatLonList() : std::vector<LatLon>(), bb_min(LatLon::MAX), bb_max(LatLon::MIN) { INVARIANT(); }

    enum e_state {
        NONE=0,             // input was no LatLon
        PUSH_AS_VALUE,      // single value, to be pushed as such
        PUSH_AS_ARRAY       // single or multiple values, to be pushed as array
    };
    enum e_state init_from_ud( lua_State *L, int index, bool checkClosing = true );

    static int _areamask( lua_State *L );

  private:
    enum e_within { INSIDE=1, AT_EDGE=0, OUTSIDE=-1 };

    e_within within( const LatLon &loc ) const;
    //void collect_inside_outside( MemMatrix &m, const LatLonBlanket &bl ) const;

    LatLon bb_min, bb_max;    // bounding box coordinates (for fast check)

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        if (size()>0) {
            assert_invariant( bb_max.lat >= bb_min.lat );
            assert_invariant( bb_max.lon >= bb_min.lon );
        }
    }
#endif
};


#endif
    // LATLON_H
