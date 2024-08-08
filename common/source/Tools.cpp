/*
* TOOLS.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#include "Tools.h"

#include "MatrixPos.h"
#include "NA_Level.h"

#include <cstring>
#include <float.h>
#include <math.h>

#ifdef UNIX
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <errno.h>
#endif

//
// 24-Aug-2011 PKi: If USE_NEWBASE_FL is defined, using newbase flightlevel calculation
//					(skipping the code build into Q3)
//

#define USE_NEWBASE_FL

#ifdef USE_NEWBASE_FL
#include "newbase/NFmiMetMath.h"
#endif

using namespace std;

Profiler_ms::ms_t Profiler_ms::offset= 0;


/*---=== ... ===---*/

/*
* Give a static text string explaining a pthread error
*/
const char *pthread_rc2str(int rc) {
    static char buf[20];    // "rc xxx"

    snprintf( buf, sizeof(buf), "rc %d", rc );

    return (rc==EBUSY) ? "EBUSY" :
           (rc==EINVAL) ? "EINVAL" :
                       buf;
}


/*---=== OS features ===---*/

/*
* Sleep until next time
*
* Note: Waiting N milliseconds in between the rounds (instead of using a timer)
*       eventually balances the threads to wake up at any time; not all
*       Trackers at the same time. This should be good for system
*       load but means one can never know, exactly when a certain new
*       data is going to be detected by the system.
*
*       Keeping the sleep time small enough (say, 1-5 mins) is the right
*       cure for this: checking the files that haven't changed and then
*       sleeping again is not burdening the system much.
*/
void Sleep_ms( unsigned ms ) {
    struct timespec ts_sleep = { (time_t)(ms/1000), (long)( (ms%1000) )*1000000L /*ns*/ };

    // Linux (CentOS 5.x; crash.fmi.fi) does not seem to have 'pthread_delay_np'
#if 0
    _PTHREAD_CALL( pthread_delay_np, &ts_sleep );
#else
    struct timespec ts_remaining;
    const struct timespec *ts= &ts_sleep;

    while( nanosleep( ts, &ts_remaining ) != 0 ) {
        ts= &ts_remaining;  // wakes up here for system signals etc.
    }
#endif
}


/*
* Returns millisecond timing for the current time.
*/
uint64_t now_ms() {
    struct timeval tv;
        // {
        //   time_t       tv_sec;   /* seconds since Jan. 1, 1970 */
        //   suseconds_t  tv_usec;  /* and microseconds */
        // };

    int rc= gettimeofday( &tv, nullptr /*time zone not used any more (in Linux)*/ );
    assert( rc==0 ); (void)rc;

    return ((uint64_t)tv.tv_sec)*1000 + (tv.tv_usec/1000);
}


/*
* Get next filename matching a mask.
*
* 'gbuf' contents must be 0 before first call.
*
* Returns the matching filename, or nullptr for no more files.
*/
#ifdef UNIX
const char *glob_fn( const char *fn_mask, glob_t &gbuf, int &gbuf_i, bool is_dir ) {
    assert( fn_mask );

    if (gbuf_i < 0) { /* first call */
        gbuf.gl_offs = 0;
        int rc= glob( fn_mask, 0 /*flags*/, nullptr /*errfunc*/, &gbuf );
            // 
            // 0: ok
            // GLOB_NOSPACE:    attempt to allocate memory failed
            // GLOB_ABORTED:    scan was stopped because ...
            // GLOB_NOMATCH:    pattern did not match a pathname and GLOB_NOCHECK was not set

        if (rc!=0) {
            if (rc!=GLOB_NOMATCH)
                LOG_ERROR( "'glob()' gave error %d (%s)", rc, 
                            (rc==GLOB_NOSPACE) ? "GLOB_NOSPACE" :
                            (rc==GLOB_ABORTED) ? "GLOB_ABORTED" : "??" );
            return nullptr;
        }
        gbuf_i= 0;  // take first file 
    }

    while ((size_t)gbuf_i < gbuf.gl_pathc) {
       const char *fn= gbuf.gl_pathv[gbuf_i];
       gbuf_i++;

       assert( fn );

       struct stat st;
       if ( stat( fn, &st ) != 0 ) {
            //
            // EACCES   Search permission is denied for a component of the path prefix
            // EFAULT   Sb or name points to an invalid address
            // EIO      An I/O error occurs while reading from or writing to the file system
            // ELOOP    Too many symbolic links are encountered in translating the pathname.
            //          This is taken to be indicative of a looping symbolic link.
            // ENAMETOOLONG A component of a pathname exceeds {NAME_MAX} characters, or an
            //          entire path name exceeds {PATH_MAX} characters.
            // ENOENT   The named file does not exist.
            // ENOTDIR  A component of the path prefix is not a directory.
            // EOVERFLOW The file size in bytes or the number of blocks allocated to the
            //          file or the file serial number cannot be represented correctly in the
            //          structure pointed to by buf.
            //
            LOG_ERROR( "'stat()' gave errno=%d", errno );
       } else {
            if (S_ISDIR(st.st_mode) ? is_dir : !is_dir )
                return fn;
       }
    }

    // Iterated all files
    //
    globfree( &gbuf );
    gbuf_i= -1;
    return nullptr;
}
#else
# error "Not implemented for Win32 API"
#endif


/*
* Does 'a' begin with 'b'?
*/
bool begins_with( const char *a, const char *b ) {
    assert( a && b );

    return strncmp( a,b, strlen(b) ) == 0;
}


/*
* Does 'a' end with 'b'?
*/
bool ends_with( const char *a, const char *b ) {
    assert( a && b );
    
    size_t a_len= strlen(a);
    size_t b_len= strlen(b);

    if (a_len>=b_len) {
        return strcmp( a+(a_len-b_len), b )==0;
    }
    return false;
}


/*
*/
void remove_file( const char *fn ) {
    assert(fn);

    int rc= unlink( fn );
    if (rc!=0) {
        // EACCES:  Search permission is denied for a component of the path
        //          Write permission is denied on the parent directory
        // EBUSY:   mount point for a mounted file system
        //          being used by the system or by another process
        // EIO:     I/O error occurs while deleting the directory entry
        // ELOOP:   looping symbolic link
        // ENAMET:  Component of a pathname exceeds {NAME_MAX} characters, or an entire path name exceeds {PATH_MAX}
        // ENOENT:  file does not exist
        // ENOTDI:  Component of the path prefix is not a directory
        // EPERM:   The named file is a directory and no sudo rights
        //          The directory containing the file is marked sticky, and neither the containing directory nor the file to be removed are owned by the effective user ID.
        // EROFS:   Read-only file system

        LOG_ERROR( "Unable to remove %s: errno %d", fn, errno );
    }
}   


/*---=== Profiler ===---*/

void Profiler_ms::start() /*throw(E_BUG)*/ {
    if (last_start!=0) {
        throw E_LOG_BUG( "Profiler '%s' already running", name.c_str() );
    }

    last_start= cpu_ms();
    if (!offset) offset= last_start;
}
    
unsigned long Profiler_ms::ms() const {
    ms_t ms_= cumulative;
    if (last_start) {   // still running
        ms_ += cpu_ms()-last_start;
    }
    return (unsigned long)ms_;
}

void Profiler_ms::stop() /*throw(E_BUG)*/ {
    ms_t now= cpu_ms();
    assert( now>=last_start );

    if (last_start==0) {
        throw E_LOG_BUG( "Stopping a non-started profiler '%s'", name.c_str() );
    }
        
    cumulative += now-last_start;
    last_start= 0;
}
    
Profiler_ms::ms_t Profiler_ms::cpu_ms() {
    /* 
    * On Linux, CLOCKS_PER_SEC is 1000000, but the resolution seems to be
    * 10 ms. Using the wall clock approach, we get (at least seemingly) 1 ms
    * resolution.
    */
#if 0
    // This uses CPU time
    //
    clock_t ct= clock();
    fprintf( stderr, "clock tick: %d", (int)ct );
    return ct/(CLOCKS_PER_SEC/1000);  // On Linux, CLOCKS_PER_SEC = 1000000
#else
    // This uses wall clock time
    //
    return (ms_t)::now_ms();
#endif
}


/*---=== Latin1 <-> UTF8 conversion ===---
* 
* Newbase SQD format uses Latin1 8-bit encoding. We use UTF8 throughout.
*
* Ref. http://msdn.microsoft.com/en-us/goglobal/cc305145.aspx
*
* 0x00..0x7f    are (naturally) one-to-one
* 0x80..0x9f    need a lookup (below)
* 0xa0..0xff    are again one-to-one
*
* Note: We might expose a 'latin1_to_utf8()' function in scripting API
*       for creation of certain characters (i.e. synop fonts need this).
*/
static const int CONV[]= {
    0x20ac,     -1, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,     // 0x80..
    0x02c6, 0x2030, 0x0160, 0x2039, 0x0152,     -1, 0x017d,     -1,
        -1, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,     // 0x90..
    0x02dc, 0x2122, 0x0161, 0x203a, 0x0153,     -1, 0x017e, 0x0178
};

/*
* Returns: -1 if 'c' is not available in Unicode (or rather: not defined in Latin1).
*/
static int latin1_to_utf8( unsigned char c ) {
    unsigned uc;    // unicode

    if (c <= 0x7f) {
        return c;       // basic ASCII

    } else if (c <= 0x9f) {
        int tmp= CONV[c-0x80];
        if (tmp<0) { return tmp; }
        uc= tmp;
    } else {
        uc= c;     // one-to-one (but needs UTF8 conversion)
    }
    
    // UTF-8 encoding: http://en.wikipedia.org/wiki/UTF-8
    //
    if (uc <= 0x07ff) {
        unsigned y= (uc >> 6) & 0x1f;
        unsigned x= uc & 0x3f;
        return 0xc080 | (y<<8) | x;

    } else {
        // 0x0800 .. 0xffff (all lookup values are within this range)
        //
        unsigned z= (uc >> 12) & 0x0f;
        unsigned y= (uc >> 6) & 0x3f;
        unsigned x= uc & 0x3f;

        return 0xe08080 | (z<<16) | (y<<8) | x;
    }
}

/*
* Returns: 0..0xff for Latin1 encoded characters
*          -1 for Unicode characters not available in Latin1.
*/
#ifdef METQU
static int utf8_to_latin1( unsigned utf8 ) {

    unsigned char a= (utf8 & 0xff);
    unsigned char b= ((utf8>>8) & 0xff);
    unsigned char c= ((utf8>>16) & 0xff);
    unsigned char d= ((utf8>>24) & 0xff);

    unsigned uc= 0;

    if (utf8 <= 0x7f) {
        return utf8;    // plain ASCII
    }

    if (d) {
        // U+010000 .. U+10FFFF (not involved in Latin-1)

    } else if (c) {
        if ( ((c & 0xf0)==0xe0) && ((b & 0xc0)==0x80) && ((a & 0xc0)==0x80) ) {     // U+0800..U+FFFF
            unsigned z= c & 0x0f;
            unsigned y= b & 0x3f;
            unsigned x= a & 0x3f;
            uc= (z<<12) | (y<<6) | x;
        }

    } else if (b) {
        if ( ((b & 0xe0)==0xc0) && ((a & 0xc0)==0x80) ) {     // U+0080..U+07FF
            unsigned y= b & 0x1f;
            unsigned x= a & 0x3f;
            uc= (y<<6) | x;
        }
    }

    if (uc) {
        if ((uc >= 0xa0) && (uc <= 0xff)) {
            return uc;
        }
        
        for( unsigned c=0x80; c<=0x9f; c++ ) {
            if (CONV[c-0x80] == (int)uc) {
                return c;   // match
            }
        }
    }

    return -1;  // not in Latin1
}
#endif


/*
* Convert Windows Latin1 encoded string to UTF8 encoding
*
* Needed for reading of SQD data and conversion of 'FmiNames' location names (s.a. 'Hämeenlinna');
* Newbase and FmiNames use Latin1 internally, even in Linux.
*
* Note: The names are relatively short and occur seldom; speed is not a major 
*       consideration for this implementation.
*/
string_or_null latin1_to_utf8( const char *latin1 ) {
    if (!latin1) return nullptr;

    unsigned n= strlen(latin1);

    // At most, the lenght of 's' is triple as UTF-8 (and the terminating '\0')
    //
    char buf[n*3+1];
    char *r= buf;

    for( const unsigned char *p= (const unsigned char *)latin1; *p; p++ ) {
        unsigned u= latin1_to_utf8(*p);

        unsigned a= u & 0xff0000;
        unsigned b= u & 0x00ff00;
        unsigned c= u & 0x0000ff;

        if (a) { *r++= a>>16; }
        if (b) { *r++= b>>8; }
        if (c) { *r++= c; }
    }
    *r= '\0';

    return buf;     // makes a copy
}


/*
* Convert UTF8 encoding to Windows Latin1
*
* Needed for creation of SQD data
*/
#ifdef METQU
string_or_null utf8_to_latin1( const char *utf8 ) {
    if (!utf8) return nullptr;

    unsigned n= strlen(utf8);

    // The lenght of Latin1 string (in bytes) is same as UTF8, or less.
    //
    char buf[n+1];
    char *r= buf;

    for( const unsigned char *p= (const unsigned char *)utf8; *p; ) {
        // 0..0x7f: single byte encoding
        // 0xc2..0xdf: start of 2-byte sequence
        // 0xe0..0xef: start of 3-byte sequence
        //
        unsigned u;

        // Note: For multibyte encoding we check the tail bytes actually are nonzero.
        //      This protects from memory access errors with malformed strings (= we're
        //      tolerant to anything thrown at us, as long as it's zero terminated).
        //
        if (*p <= 0x7f) {
            u= *p++;
        } else if ((*p >= 0xc2) && (*p <= 0xdf) && p[1]) {
            u= (*p<<8) | p[1];
            p += 2;
        } else if ((*p >= 0xe0) && (*p <= 0xef) && p[1] && p[2]) {
            u= (*p<<16) | (p[1]<<8) | p[2];
            p += 3;
        } else {
BAD_UTF8:
            throw E_LOG_USAGE( "Bad UTF8 data (unable to convert to Latin-1): %s", utf8 );
        }

        int c= utf8_to_latin1(u);
        if (c<0) goto BAD_UTF8;

        *r++= c;
    }
    *r= '\0';

    return buf;     // makes a copy
}
#endif


/*
*/
NA_Level one_level( lua_State *L, int idx, const char *lt_name ) {
    assert(lt_name);

    bool true_only= (strcmp( lt_name, "ground" )==0 || strcmp( lt_name, "sounding" )==0);     // approve 'true' only

#ifdef CONFIG_FLIGHT_LEVELS_API
    bool is_flight= strcmp( lt_name, "flight" )==0;     // convert to pressure level
#endif

#ifdef CONFIG_FLIGHT_LEVELS_API
    NA_Level::Type lt= is_flight ? NA_Level::PRESSURE_LEVEL : NA_Level::lt_enum(lt_name);
#else
    NA_Level::Type lt= NA_Level::lt_enum(lt_name);
#endif

    if (!lt) {
        throw E_LOG_USAGE( "Bad level key: %s", lt_name );
    }

    switch( lua_type(L,idx) ) {
        case LUA_TBOOLEAN:
            if (lua_toboolean(L,idx)) {
                // 'flight=true' will cause a 'hpa=true' (matters only if CONFIG_FLIGHT_LEVELS_API defined)
                //
                return NA_Level( lt );
            } 
            break;
        
        case LUA_TNUMBER: 
            if (!true_only) {
                double lv= lua_tonumber(L,idx);

#ifdef CONFIG_FLIGHT_LEVELS_API
                if (is_flight) {
                    // Call 'fl_hpa' function in Lua
                    //

#ifdef USE_NEWBASE_FL
					// 24-Aug-2011 PKi: Get flightlevel pressure from newbase
	
					lv = CalcFlightLevelPressure(100 * lv);
#else

                L_START
                    L_GROW(2);
                    
                    lua_getglobal( L, "fl_hpa" );
                    lua_pushnumber( L, lv );
                    lua_call( L, 1 /*args*/, 1 /*retvals*/ );
                    
                    lv= lua_tonumber(L,-1);
                    lua_pop(L,1);

                L_END(0)    // checks the stack remains in balance
#endif	// ifdef USE_NEWBASE_FL
                }
#endif
                return NA_Level( lt, lv );
            } 
            break;
            
        default: break;
    }
    throw E_LOG_USAGE( "Bad value '%s=%s'", lt_name, L_string_or_typename(idx) );
}


/*
* Adds specified levels to 'vec'.
*
* 'hpa=true|number|{number,...}' (or 'ground', 'hybrid', 'height').
*
* If the value in 'idx' is not as expected, 'E_USAGE' is thrown.
*/
void one_or_many_levels( lua_State *L, int idx, const char *lt_name, vector<NA_Level> &vec ) {

    bool true_only= (strcmp(lt_name,"ground")==0 || strcmp(lt_name,"sounding")==0);     // approve 'true' only

    if (!lua_istable(L,idx) || true_only) {
        vec.push_back( one_level( L, idx, lt_name ) );    // may throw E_USAGE

    } else {
        unsigned idx_abs= L_ABS(idx);
        unsigned n= lua_objlen(L,idx);
        for( unsigned i=1; i<=n; i++ ) {
            lua_pushinteger(L,i);
            lua_gettable(L,idx_abs);
            
            if (lua_isnumber(L,-1)) {
                vec.push_back( one_level( L, -1, lt_name ) );
                lua_pop(L,1);
            } else {
                throw E_LOG_USAGE( "Bad '%s' value: %s", lt_name, L_string_or_typename(-1) );
            }
        }
    }
}
