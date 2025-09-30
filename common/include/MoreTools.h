/*
* MORETOOLS.H                       Copyright (c) 2009-10, Ilmatieteen laitos
*
* Tools that were not wanted to be placed in 'Tools.h' because of possible cyclic
* header reads etc. Anyways, they are here.
*
* Revised:  21-Oct-2010 AKa
*/
#ifndef MORETOOLS_H
#define MORETOOLS_H

#include "Tools.h"
#include "JDay.h"

#include "NA_Level.h"
#include "ApiParam.h"

#include <cmath>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <algorithm>
    // std::unique
#include <string>

#ifdef METQU
# include "NA_Param.h"
#endif


/*---=== Helper classes ===---*/

/*
* Template specifier for 'std::string'
*/
struct T_String {
    typedef std::string ValueT;

    static std::string create( lua_State *L, int idx ) {
        const char *s= lua_tostring(L,idx);
        if (!s) {
            throw E_LOG_USAGE( "Expecting string, got: %s", L_typename(idx) );
        }
        return s;
    }

    static const std::string &toString( const std::string &s ) noexcept {
        return s;
    }
};


/*
* Template specifier for 'NA_Level'
*/
struct T_Level {
    typedef NA_Level ValueT;

    // Note: conversion from string needed for MQD header handling.
    //
#ifdef MQD_ENABLED
    static NA_Level create( lua_State *L, int idx ) {
        const char *s= lua_tostring(L,idx);
        if (!s) {
            throw E_LOG_USAGE( "Bad type for 'levels' (expecting string): %s", L_typename(idx) );
        }
        return NA_Level(s);     // may throw 'E_USAGE' for a bad name
    }
#endif

    static std::string toString( const NA_Level &p ) noexcept {
        return p.toString();
    }
};

/*
* Template specifier for 'ApiParam' (used in the scripting API)
*/
struct T_ApiParam {
    typedef ApiParam ValueT;

    static ApiParam create( lua_State *L, int idx ) {
        const char *s= lua_tostring(L,idx);
        if (!s) {
            throw E_LOG_USAGE( "Bad type for 'params' (expecting string): %s", L_typename(idx) );
        }
        return ApiParam(s);
    }
};

/*
* Template specifier for 'NA_Param'
*/
struct T_NativeParam {
    typedef NA_Param ValueT;

#ifdef METQU
    static NA_Param create( lua_State *L, int idx ) {
        const char *s= lua_tostring(L,idx);
        if (!s) {
            throw E_LOG_USAGE( "Bad type for 'params' (expecting string): %s", L_typename(idx) );
        }
# ifndef NDEBUG
        if( strchr(s,':') ) {
            throw E_LOG_BUG( "Shouldn't have ':' in '%s'", s );
        }
# endif
        return NA_Param(s);
    }
#endif

    static std::string toString( const NA_Param &p ) noexcept {
        return p.toString(false /*prefer native names*/);
    }
};

/*
* Template specifier for 'JDay'
*/
struct T_JDay {
    typedef JDay ValueT;

    static JDay create( lua_State *L, int idx ) {
        JDay jday( L, idx );
        if (!jday) {
            throw E_LOG_USAGE( "Bad value for 'times': %s", L_string_or_typename(idx) );
        }
        return jday;
    }

    // Output time in "YYYYMMDDMMHHSS" format
    //
    static std::string toString( const JDay &t ) noexcept {
        return t.toString();
    }
};


/*---=== Vector tools ===---*/

/*
* Read a 'any|{any,...}' parameter to a C++ vector of levels, params or times.
*
* Throws:
*       E_USAGE if the provided values are not valid for this type.
*/
template<typename T>
std::vector<typename T::ValueT> vector_of_( lua_State *L, int idx, bool allow_single_without_braces ) {

    idx= L_ABS(idx);    // make it an absolute index
    std::vector<typename T::ValueT> ret;
    bool got_it= true;

    unsigned tos= lua_gettop(L);
    try {
        if (lua_istable(L,idx)) {
            // Iterate 1..n indices, in order
            //
            bool done= false;
            for( unsigned i=1; !done; i++ ) {
                lua_pushinteger( L,i );
                lua_gettable( L, idx );   // replaces key (1..n) with value or 'nil'

                if (lua_isnil(L,-1)) {
                    done= true;
                } else {
                    ret.push_back( T::create(L,-1) );   // can throw E_USAGE
                }
                lua_pop(L,1);
            }
        } else if (allow_single_without_braces) {
            ret.push_back( T::create(L,idx) );  // can throw E_USAGE
        } else {
            got_it= false;
        }
    } 
    catch( const E_USAGE &e ) {
        // Note: don't make Lua error here, since upper levels may have other ideas (i.e. return nil+err_str).
        //
        lua_settop(L,tos);  // revert the stack to how it was
        throw(e);
    }
    catch( const std::exception &e ) {
        lua_settop(L,tos);
        throw E_LOG_BUG( "Unexpected exception: %s", e.what() );
    }
    
    // Throw E_USAGE if it wasn't a table and using tables was insisted upon.
    //
    if (!got_it) {
        throw E_LOG_USAGE( "Expected a table, got: %s", L_typename(idx) );
    }

    return ret;
};


inline std::vector<JDay> vector_of_times( lua_State *L, int idx ) {
    return vector_of_<T_JDay>( L, idx, true );
}

inline std::vector<ApiParam> vector_of_apiparams( lua_State *L, int idx ) {
    return vector_of_<T_ApiParam>( L, idx, true );
}

/*
* Note: THIS IS NEEDED FOR MQD HEADER HANDLING ONLY. 
*/
#ifdef MQD_ENABLED
inline std::vector<NA_Param> vector_of_params( lua_State *L, int idx ) {
    return vector_of_<T_NativeParam>( L, idx, true );
}
#endif

/*
* Note: THIS IS NEEDED FOR MQD HEADER HANDLING ONLY. 
*/
#ifdef MQD_ENABLED
inline std::vector<NA_Level> vector_of_levels( lua_State *L, int idx ) {
    return vector_of_<T_Level>( L, idx, false /* require {} */ );
}
#endif

/*
* Note: THIS IS NEEDED FOR MQD HEADER HANDLING ONLY. 
*/
#ifdef MQD_ENABLED
inline std::vector<std::string> vector_of_strings( lua_State *L, int idx ) {
    return vector_of_<T_String>( L, idx, false /* require {} */ );
}
#endif


/*---=== Pair tools ===---*/

/*
* Read a '{any,any}' parameter to a C++ pair.
*
* Throws: E_USAGE if the entries aren't as expected.
*/
template<typename T>
std::pair<typename T::ValueT,typename T::ValueT> pair_of_( lua_State *L, int idx ) {

    idx= L_ABS(idx);    // make it an absolute index

    std::pair<typename T::ValueT,typename T::ValueT> ret;

    if (!lua_istable(L,idx)) {
        throw E_LOG_USAGE( "Expected table, got: %s", L_typename(idx) );
    }

    unsigned tos= lua_gettop(L);
    try {
        for( unsigned i=1; i<=2; i++ ) {
            lua_pushinteger( L,i );
            lua_gettable( L, idx );   // replaces key (1..2) with value or 'nil'

            if (i==1) ret.first= T::create(L,-1);   // can throw E_USAGE
            else ret.second= T::create(L,-1);       // -''-

            lua_pop(L,1);
        }
    } 
    catch( const E_USAGE &e ) {
        lua_settop(L,tos);              // restore stack as it was
        throw(e);
    }
    catch( const std::exception &e ) {
        lua_settop(L,tos);
        throw E_LOG_BUG( "Unexpected exception: %s", e.what() );
    }
    return ret;
}


/*---=== Output tools ===---*/

/*
* Make a string output of vector of times, levels or params (for MQD output; see 'MQD_Data.cpp').
*/
#ifdef MQD_ENABLED
template<typename T>
void output_vector( const std::vector<typename T::ValueT> &vec, std::ostream &os ) noexcept {

    os << "{ ";

    bool first= true;
    for( typename std::vector<typename T::ValueT>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        if (first) first=false;
        else os << ", ";

        os << T::toString(*it);
    }
    assert(!first);     // They are never empty
    os << " }";
}

inline std::ostream & operator << ( std::ostream &os, const std::vector<JDay> &vec ) noexcept {
    output_vector<T_JDay>( vec, os );
    return os;
}

inline std::ostream & operator << ( std::ostream &os, const std::vector<NA_Level> &vec ) noexcept {
    output_vector<T_Level>( vec, os );
    return os;
}

# ifdef METQU
inline std::ostream & operator << ( std::ostream &os, const std::vector<NA_Param> &vec ) noexcept {
    output_vector<T_NativeParam>( vec, os );
    return os;
}
# endif

# ifdef METQU
inline std::ostream & operator << ( std::ostream &os, const std::vector<std::string> &vec ) noexcept {
    output_vector<T_String>( vec, os );
    return os;
}
# endif
#endif
    // MQD_ENABLED


/*---=== Sorting ===---
*
* C++ sorting API is a bit awkward. We can do better.
*/
template<typename T>
void sort_descending( std::vector<T> &vec, bool unique_only=false ) {
    struct my_sort {
        static bool func(const T &a, const T &b) { return a>b; }
    };
    sort( vec.begin(), vec.end(), my_sort::func );

    if (unique_only) {
        vec.erase( std::unique(vec.begin(), vec.end()), vec.end() );
    }
}


/*---=== Level reading ===---
*/

NA_Level one_level( lua_State *L, int idx, const char *lt_name );
void one_or_many_levels( lua_State *L, int idx, const char *lt_name, std::vector<NA_Level> &vec );


#endif
    // MORETOOLS_H
