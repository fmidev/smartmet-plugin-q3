/*
* NA_LEVEL.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Abstraction of a level, used in Q3.
*
* Revised:  17-Oct-10 AKa
*/
#ifndef NA_LEVEL_H
#define NA_LEVEL_H

#include "MatrixPos.h"
#include "Tools.h"

#include <string>
#include <vector>
#include <math.h>

class NA_Data;

/*
*/
struct NA_Level {
  public:
    enum Type {
        NO_LEVEL= 0,   // empty level description

        GROUND_LEVEL,       // "ground"         Ground level
        HYBRID_LEVEL,       // "hybrid[:nnn]"   Specific model levels (no interpolation); 'lv' is integer
        PRESSURE_LEVEL,     // "hpa[:xxx.x]"    Pressure levels; 'lv' in hPa (= mbar)

        HEIGHT_LEVEL,       // "height[:xxx.x]" Height for cross call; 'lv' in meters
        SOUNDING_LEVEL      // "sounding"		All sounding levels
    };

#ifdef MQD_ENABLED
    NA_Level( const char *s ) throw(E_USAGE);
#endif
    //NA_Level( const char *lt, double lv ) throw(E_USAGE);
    NA_Level( enum Type lt_, double lv_=NAN );

    NA_Level() : lt(NO_LEVEL), lv(NAN) {}
    
    // standard copy constructor, assignment and destructor are okay

    Type getType() const { return lt; }
    double getValue() const { return lv; }

    bool hasValue() const { return !isnan(lv); }
    std::string toString(bool getLongName = false) const;

    bool operator== ( const NA_Level &o ) const {
        return (lt==o.lt) && ((lt==GROUND_LEVEL) || (lt==SOUNDING_LEVEL) || (lv==o.lv));
    }

    bool isGroundLevel() const { return lt==GROUND_LEVEL; }
    bool isPressureLevel() const { return lt==PRESSURE_LEVEL; }
    bool isHybridLevel() const { return lt==HYBRID_LEVEL; }
    bool isHeightLevel() const { return lt==HEIGHT_LEVEL; }
    bool isSoundingLevel() const { return lt==SOUNDING_LEVEL; }

    bool covered_by( const NA_Data &data, bool &exact ) const;

    bool covered_by( const NA_Data &data ) const {
        bool dummy;
        return covered_by( data, dummy );
    }

    static Type lt_enum( const char * );

    operator bool() const { return lt != NO_LEVEL; }

  private:
    static bool levels_range( enum Type lt_, const std::vector<NA_Level> &vec, double &lv_min, double &lv_max );
    static bool has_at_least_n_of( enum Type lt_, const std::vector<NA_Level> &vec, unsigned n );

    enum Type lt;
    double lv;          // NAN for a generic level type (no value)

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        if ((lt==NO_LEVEL) || (lt==GROUND_LEVEL)) {
            assert_invariant( isnan(lv) );
        }
    }
#endif
};


#endif
    // NA_LEVEL_H
