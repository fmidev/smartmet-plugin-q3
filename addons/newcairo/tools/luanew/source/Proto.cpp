/*
* PROTO.CPP                 Copyright (c) 2010, Ilmatieteen laitos
*/
#include "Proto.h"

extern "C" {
  #include "lualib.h"
  #include "lauxlib.h"
}

#include "assert.h"

static unsigned char proto_chunk[]=
#include "proto.lch"

static void *PROTO_REGISTRY_KEY= (void*) &PROTO_REGISTRY_KEY;     // unique

#define L_GROW(n)  lua_checkstack( L, (n) )

#define L_ASSERT(cond) \
    { if (!(cond)) { \
        luaL_error( L, "Assert failed at %s:%d: %s", __FILE__, __LINE__, #cond ); \
    } }


/*
* Provide the Lua state's "proto" function, via registry key.
*/
static void push_proto( lua_State *L ) {
    L_GROW(1);
    lua_pushlightuserdata( L, PROTO_REGISTRY_KEY );
    lua_gettable( L, LUA_REGISTRYINDEX );

    if (!lua_istable(L,-1)) {
        luaL_error( L, "'proto' not properly initialized" );
    }
}


/*
* Initialize the 'proto' system for both Lua and C(++) side.
*
* This sets up the "proto.*" table on the Lua side, ties it to 'PROTO_REGISTRY_KEY'
* and allows custom addon types to be introduced by:
*
*   proto_init(L).set( id, func ).set( id, func ), ...
*/
ProtoProxy proto_init( lua_State *L ) {

    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);

    if (lua_isnil(L,-1)) {
        lua_pop(L,1);
        luaL_error( L, "No globals: LUA_REGISTRYINDEX/LUA_RIDX_GLOBALS is nil" );
    }
    else {
      lua_pushliteral( L, "proto" );
      lua_gettable( L, -2 );
    }

    if (lua_isnil(L,-1)) {
        lua_pop(L,2);

        // Initialize the precompiled block to 'proto' global and C++ binding.
        //
        int st= luaL_loadbuffer( L, (char *) proto_chunk, sizeof(proto_chunk), nullptr /*from precompiled*/ );
        if (st) {
            luaL_error( L, lua_tostring(L,-1) );    // can only be LUA_ERRMEM (script is precompiled)
        }
        lua_call( L, 0 /*args*/, 0 /*results*/ );   // sets a global 'proto'

        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
        lua_pushliteral( L, "proto" );
        lua_gettable( L, -2 );
        lua_remove( L, -2 );
        
        assert( lua_istable(L,-1) );

    } else if (!lua_istable(L,-1)) {
        luaL_error( L, "Unexpected 'proto': %s", lua_typename( L, lua_type(L,-1) ) );
    }
    else
      lua_remove( L, -2 );
    
    // [-1]: proto table

    // Tie the table to 'PROTO_REGISTRY_KEY'
    //
    lua_pushlightuserdata( L, PROTO_REGISTRY_KEY );
    lua_insert( L, -2 );
    lua_settable( L, LUA_REGISTRYINDEX );

    return ProtoProxy(L);
}


/*
* Set a new prototype check
*/
ProtoProxy &ProtoProxy::set( const char *id, lua_CFunction f ) {

    L_GROW(3);

    push_proto(L);
    lua_pushstring( L, id );
    lua_pushcfunction( L, f );
    lua_settable( L, -3 );

    lua_pop(L,1);    
    return *this;   // allows chaining
}


/*
* Called to check values into a function (normally part of public API)
*
* [1..tos] are the parameters
*/
void proto( lua_State *L, const char *pt ) {
    unsigned params= lua_gettop(L);     // number of params (need to be duplicated)

    L_GROW(2+params);

    // Get the Lua proto function via registry
    //
    push_proto(L);

    lua_pushstring(L,pt);

    // Duplicate the [1..params] parameters
    //
    for( unsigned i=1; i<=params; i++ ) {
        lua_pushvalue( L, i );
    }

    lua_call( L, 1+params /*args*/, 0 /*retvals*/ );   
    
    L_ASSERT( (unsigned)lua_gettop(L) == params );
}

