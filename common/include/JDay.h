/*
* JDAY.H                            Copyright (c) 2010, Ilmatieteen laitos
*
* Ref: http://en.wikipedia.org/wiki/Julian_day
*
* Revised:  25-Oct-2010
*/
#ifndef JDAY_H
#define JDAY_H

#include "LuaNew.h"
#include "Invariant.h"

#include <math.h>
#include <stdint.h>
    // int64_t
#include <time.h>

#include <vector>

class JDay;

struct JDayBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "JDay"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef JDay CAST_T;

  private:
    static int index( lua_State *L );
    static int newindex( lua_State *L );
    static int add( lua_State *L );
    static int sub( lua_State *L );
    static int eq( lua_State *L );
    static int lt( lua_State *L );
    static int tostring( lua_State *L );
};

/*
* Stores time in Julian day (http://en.wikipedia.org/wiki/Julian_day)
*
* "The Julian date (JD) is the interval of time in days and fractions of a day since January 1, 4713 BC Greenwich noon"
*/
class JDay : public LuaNew<JDayBind> {
  public:
    JDay( lua_State *L, int index );
    JDay() : jd_plus_half(INVALID_VALUE) { INVARIANT(); }

    JDay( unsigned year_, unsigned month_, unsigned day_, unsigned hour_, unsigned min_, unsigned sec_ )
        : jd_plus_half( calc( year_, month_, day_, hour_, min_, sec_ ) ) { INVARIANT(); }

    JDay( time_t ticks ) : jd_plus_half( calc(ticks) ) { INVARIANT(); }

    unsigned year() const;
    unsigned month() const;
    unsigned day() const;
    unsigned hour() const;
    unsigned min() const;
    unsigned sec() const;
    unsigned wday() const;
    unsigned yday() const;

    void set( unsigned year_, unsigned month_, unsigned day_, unsigned hour_, unsigned min_, unsigned sec_ ) {
        jd_plus_half= calc( year_, month_, day_, hour_, min_, sec_ );
    }

    std::string toString() const;

    operator bool () const { return jd_plus_half != INVALID_VALUE; }
    bool operator!() const { return ! operator bool(); }

    // Application is supposed to have checked for validity before doing comparisons
    // (keep things fast).
    //
    bool operator==( const JDay &o ) const { return jd_plus_half == o.jd_plus_half; }
    bool operator<( const JDay &o ) const { return jd_plus_half < o.jd_plus_half; }
    //
    bool operator!=( const JDay &o ) const { return !(*this==o); }
    bool operator>( const JDay &o ) const { return o<*this; }
    bool operator<=( const JDay &o ) const { return !(*this>o); }
    bool operator>=( const JDay &o ) const { return !(*this<o); }

    // Use 'add_secs()' instead of 'operator+(int)' to make units explicit.
    //
    JDay add_secs( long secs ) const { JDay ret(*this); ret.jd_plus_half += secs; return ret; }
    JDay sub_secs( long secs ) const { return add_secs(-secs); }
    JDay add_mins( double m ) const { return add_secs( lround(60.0*m) ); }
    JDay sub_mins( double m ) const { return add_mins(-m); }
    JDay add_hours( double h ) const { return add_secs( lround(3600.0*h) ); }
    JDay sub_hours( double h ) const { return add_hours(-h); }

    // The official Julian Day value (days since 12:00 UTC Jan 1str 4713 BC)
    //
    double jd_noon() const { return (jd_plus_half-43200)/86400.0; }

    bool covered_by( const std::vector<JDay> &vec, bool &exact ) const;

    bool covered_by( const std::vector<JDay> &vec ) const {
        bool ignore;
        return covered_by( vec, ignore );
    }

    void gregorian( unsigned *year_ptr, unsigned *month_ptr, unsigned *day_ptr ) const;

#ifndef NDEBUG
    static void selftest();
#endif

  private:
    friend class JDayBind;

    typedef int64_t jd_T;       // current (2010) values are around 212136537600 (>2^37)

    static jd_T calc( unsigned yyyy, unsigned mm, unsigned dd ) {
        // 
        // These secs are from midnight, essentially adding 43200 to the right 'Julian day' value 
        // (which is measured from noon).
        //
        return calc_jdn( yyyy, mm, dd ) * ((jd_T)86400);
    }

    static jd_T calc( unsigned yyyy, unsigned mm, unsigned dd, unsigned hour, unsigned min, unsigned sec ) {
        return calc( yyyy, mm, dd ) + 3600*hour + 60*min +sec;
    }

    static jd_T calc( time_t ticks );
 
    static unsigned calc_jdn( int yyyy, int mm, int dd );

    static jd_T fromstring( const char *s );

    // data members
    //
    // Using a discrete measure of time is more precise than using a double. 
    //
    // Julian day from MIDNIGHT (full days since 0:00 UTC Jan 1st 4713 BC); INVALID_VALUE for empty
    //
    jd_T jd_plus_half;     // jdn*86400 + (secs_since_noon + 43200)

    static jd_T INVALID_VALUE;

#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        (void)file; (void)line;
    }
#endif
};

#endif
    // JDAY_H
