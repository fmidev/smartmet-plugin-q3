/*
* REGTOOLS.CPP                     Copyright (c) 2010, Ilmatieteen laitos
*/
#include "RegTools.h"

#include "LuaNew.h"

using namespace std;


/*---=== Matrix output decimals ===---
*
* We use the Lua stack's registry to store information about how many decimals should be used
* in matrix output and whether binary output is requested.
*/

/* Registry keys - each guaranteed unique.
*/
void *RegTools::REG_DECIMALS= (void*) &RegTools::REG_DECIMALS;
void *RegTools::REG_JSONP= (void*) &RegTools::REG_JSONP;

#ifndef METQU
# ifdef CONFIG_BINARY_OUTPUT_ENABLED
  void *RegTools::REG_BINARY= (void*) &RegTools::REG_BINARY;
# endif
#else
  void *RegTools::REG_FILENAME= (void*) &RegTools::REG_FILENAME;
#endif

/*
* Returns <0 if no such entry existed
*/
int RegTools::get_uint( lua_State *L, void *key ) {

    L_GROW(1);
    lua_pushlightuserdata( L, key );
    lua_gettable( L, LUA_REGISTRYINDEX );
        //
        // [-1]: int    

    int v= lua_isnumber(L,-1) ? lua_tointeger(L,-1) : -1;
    lua_pop(L,1);
    
    return v;
}

/*
*/
void RegTools::set( lua_State *L, void * key, unsigned v ) {

    L_GROW(2);
    lua_pushlightuserdata( L, key );
    lua_pushinteger( L, v );
    lua_settable( L, LUA_REGISTRYINDEX );
}

/*
*/
string_or_null RegTools::get_str( lua_State *L, void *key ) {
    L_GROW(1);
    lua_pushlightuserdata( L, key );
    lua_gettable( L, LUA_REGISTRYINDEX );
        //
        // [-1]: string|nul

    string_or_null s( lua_tostring(L,-1) );     // makes a copy
    lua_pop(L,1);
    
    return s;
}

/*
*/
void RegTools::set( lua_State *L, void *key, const char *s ) {

    L_GROW(2);
    lua_pushlightuserdata( L, key );
    lua_pushstring( L, s );
    lua_settable( L, LUA_REGISTRYINDEX );
}

/*
*/
bool RegTools::get_bool( lua_State *L, void *key ) {

    L_GROW(1);
    lua_pushlightuserdata( L, key );
    lua_gettable( L, LUA_REGISTRYINDEX );
        //
        // [-1]: bool    

    L_ASSERT( lua_isboolean(L,-1) );

    bool ret= lua_toboolean(L,-1);
    lua_pop(L,1);
    
    return ret;
}

/*
*/
void RegTools::set( lua_State *L, void * key, bool v ) {

    L_GROW(2);
    lua_pushlightuserdata( L, key );
    lua_pushboolean( L, v );
    lua_settable( L, LUA_REGISTRYINDEX );
}
