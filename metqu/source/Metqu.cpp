/*
 * METQU.CPP                            Copyright (c) 2008-2010, Ilmatieteen laitos
 * 
 * Metqu command line tool
 *
 * For usage, see the self-printing help message
*/
#ifndef METQU
# error "Not for server mode"
#endif

#include "Session.h"
#include "LogTools.h"
#include "Matrix.h"

#include <iostream>

using namespace std;

#ifdef UNIX
# define WIN32_DLLEXPORT
#else
# define WIN32_DLLEXPORT __declspec(dllexport)
#endif


/*
* = results( ... )
*
* Output results to stdout. This is called implicitly from 'metqu' launch script,
* for the values the user script has returned (if any).
*/
static int results( lua_State *L ) {

    unsigned n= lua_gettop(L);
    for( unsigned i=1; i<=n; i++ ) {
        Session::result_( L, cout, i );
    }
    return 0;
}


/*
* Lua addon module entry point
*/
extern "C" int WIN32_DLLEXPORT luaopen_metqu( lua_State *L )
{
    // TBD: Could also allow the script to change logging, or switch on/off.
    //      Either via a Lua global we'll act upon here or by exposing the
    //      logging to the scripts.
    //
    Logger::init( new StderrLogger() );

    lua_pushcfunction( L, Session::init );
    lua_call( L, 0 /*args*/, 0 /*retvals*/ );   // may initiate an error

    lua_pushliteral( L, "_results" );
    lua_pushcfunction( L, results );
    lua_settable( L, LUA_GLOBALSINDEX );

    return 0;
}
