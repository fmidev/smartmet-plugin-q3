/*
* NA_LEVEL.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  17-Oct-10 AKa
*/
#include "NA_Level.h"

#include "Matrix.h"
#include "Tools.h"
#include "LogTools.h"

#include <string>
#include <map>
#include <memory>
    // auto_ptr
#include <fstream>

using namespace std;


/*---=== Helpers ===---
*/

/*
* Return 'true' if 'vec' has at least 'n' levels of given type.
*/
bool NA_Level::has_at_least_n_of( enum Type lt_, const vector<NA_Level> &vec, unsigned n ) {
    unsigned found= 0;

    for( vector<NA_Level>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        if (it->getType() == lt_) {
            if (++found >= n) { return true; }
        }
    }
    return false;
}
#define has_at_least_1_of( lt_, vec )  has_at_least_n_of( lt_, vec, 1 )
#define has_at_least_2_of( lt_, vec )  has_at_least_n_of( lt_, vec, 2 )

/*
* Returns 'true' if 'vec' contains the level, exactly as-is.
*/
static bool has_level( const NA_Level &lev, const vector<NA_Level> &vec ) {
    int idx= find_index( vec, lev );
    return idx >= 0;
}


/*
*/
string NA_Level::toString(bool getLongName) const {

    if (lt==NA_Level::GROUND_LEVEL) {
        return (getLongName ? "Ground" : "ground");
    } else if (lt==NA_Level::NO_LEVEL) {
        return (getLongName ? "None" : "none");
    }

    const char *s= (lt==NA_Level::HYBRID_LEVEL) ? (getLongName ? "HybridLevel" : "hybrid") :
                   (lt==NA_Level::PRESSURE_LEVEL) ? (getLongName ? "PressureLevel" : "hPa") :
                   (lt==NA_Level::HEIGHT_LEVEL) ? (getLongName ? "HeightLevel" : "height") :	// 04-Oct-2011 PKi: "Height" level for cross call
                   (lt==NA_Level::SOUNDING_LEVEL) ? (getLongName ? "SoundingLevel" : "sounding") :
                    NULL;
    assert(s);
    
    if (std::isnan(lv)) {
        return s;

    } else if (lv == floorf(lv)) {
        return string_fmt( "%s:%d", s, (int)lv );
    
    } else {
        return string_fmt( "%s:%lf", s, lv );
    }
}


/*---=== NA_Level ===---
*
* Level names:
*
* "ground"      Ground level
* "hybrid:5"    Hybrid level (model level) #5
* "hPa:850"     Pressure level of 850 hPa
*
* Pressure level data is actually precalculated constant-pressure cross sections from
* hybrid data.
*
* Levels in the SQD file can be in either ascending or descending order; Q3 swaps them
* to always being in ascending order from ground upwards (pressure declines, height rises).
*/

/*
* Note: This code is needed ONLY FOR MQD HEADER handling. SQD handling or Q3 scripts
*       don't need this.
*/
#ifdef MQD_ENABLED
NA_Level::NA_Level( const char *sc ) throw(E_USAGE) :lt(), lv() {
    assert(sc);
    
    if (strcmp(sc,"ground")==0) {
        lt= GROUND_LEVEL; 
        lv= NAN;
    } else {
        int d_tmp;

        if (sscanf( sc, "hybrid:%d", &d_tmp ) == 1) {
            lt= HYBRID_LEVEL;
            lv= d_tmp;

        } else {
            double tmp;

            if ((sscanf( sc, "hPa:%lf", &tmp ) == 1) || (sscanf( sc, "hpa:%lf", &tmp ) == 1)) {
                lt= PRESSURE_LEVEL;
            } else {
                throw E_LOG_USAGE( "Bad level name: %s", sc );
            }
            lv= tmp;
        }
    }

    INVARIANT();
}
#endif


/*
*/
NA_Level::NA_Level( enum Type lt_, double lv_ ) : lt(lt_), lv(lv_) {
    INVARIANT();
}


/*
*/
NA_Level::Type NA_Level::lt_enum( const char *s ) {
    assert(s);

    if (strcmp(s,"ground")==0) {
        return GROUND_LEVEL; 
    } else if (strcmp(s,"hybrid")==0) {
        return HYBRID_LEVEL;
    } else if ((strcmp(s,"hpa")==0) || (strcmp(s,"hPa")==0)) {
        return PRESSURE_LEVEL;
    } else if (strcmp(s,"height")==0) {		// 04-Oct-2011 PKi: "Height" level for cross call
    	 return HEIGHT_LEVEL;
    } else if (strcmp(s,"sounding")==0) {
        return SOUNDING_LEVEL; 
    } else {
        return NO_LEVEL;
    }
}


/*
* Find the min and max values of certain type of levels (if any).
*
* Returns:  true if there was at least one level of that type ('lv_min' and 'lv_max' set)
*           false if there were no levels of that type
*
* Note: The ordering is by the 'lv' numbers, not "from the ground up" as in q3 scripting.
*       Thus i.e. 'hpa:300' would be min and 'hpa:1000' max.
*/
bool NA_Level::levels_range( enum Type lt_, const vector<NA_Level> &vec, double &lv_min, double &lv_max ) {

    bool found= false;

    for( vector<NA_Level>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        if (it->lt == lt_) {
            double lv= it->lv;
            
            if (!found) {
                lv_min= lv_max= lv;
                found= true;
            } else {
                lv_min= ::min( lv_min, lv );
                lv_max= ::max( lv_max, lv );
            }
        }
    }
    return found;
}


/*
* Find out whether a level is covered by 'data'.
*
* Note: Some levels (height, pressure) can be calculated from parameters in data, so we cannot
*       simply feed a vector of levels here (also, this allows us to delegate parts of the decision-making
*       to the actual data adapter).
*
* If 'lv' is NAN, checks whether such kind of level(s) exist (or are calculatable) at all.
*
* Returns:  true if covered ('exact' set to true if available right as-is, 
*                               false to mark need of interpolations and/or calculations)
*           false if not covered
*/
bool NA_Level::covered_by( const NA_Data &data, bool &exact ) const {
    exact= false;

    const vector <NA_Level> &vec= data.getLevels();

    if (lt==GROUND_LEVEL) {
        exact= true;
        return has_level( *this, vec );

    } else if (lt==HYBRID_LEVEL) {
        if (std::isnan(lv)) {      // any hybrid level(s), at all
            return has_at_least_1_of( lt, vec );
        } else {
            exact= true;
            return has_level( *this, vec );     // must match precisely
        }

    } else if (lt==PRESSURE_LEVEL) {
        /*
        * Pressure levels (pre-calculated) can exist in the data, either exact or interpolated.
        */
        if (has_at_least_1_of( PRESSURE_LEVEL, vec )) {
            if (isnanf(lv)) {
                return true;
            } else {
                double lv_min, lv_max;
                levels_range( PRESSURE_LEVEL, vec, lv_min, lv_max );

                if ((lv_min <= lv) && (lv_max >= lv)) {
                    exact= has_level( *this, vec );
                    return true;
                }
            }
        }
        
        /*
        * In SQD files, pressure levels can be calculated from hybrid data with parameter ':1' (kFmiPressure,
        * "P").
        */

        return data.providesPressureLevelsFromHybrid();

    } else if (lt==HEIGHT_LEVEL) {			// 04-Oct-2011 PKi: "Height" level for cross call
        exact= false;						//				    Don't (need to) know

        /* 13-Oct-2011 PKi
        * 
        * In SQD files, height levels can be calculated from hybrid data with parameter ':2' or ':3'
        * (kFmiGeopHeight or kFmiGeomHeight).
        *
        * 23-Nov-2012 PKi: Height value query support. Currently no value based checks are made for the height value(s) requested;
        * 				   if support for height level query data (in addition to intepolating data to given heights by using the
        * 				   Z parameter) is added later, availability of the requested level(s) shall be checked
        */

        return data.providesHeightLevelsFromHybrid();

    } else if (lt==NO_LEVEL) {
        /*
        * First level of any data (always true, data cannot exist without a level).
        */
        exact= true;
        return true;

    } else if (lt==SOUNDING_LEVEL) {
        exact= true;
        return has_level( *this, vec );

    } else {
        throw( E_LOG_BUG( "Unexpected level type: %d", (int)lt ));
    }
}

