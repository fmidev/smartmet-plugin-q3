/*
 * LUAWRAP.CPP                    Copyright (c) 2008-2010, Ilmatieteen laitos
*/

#include "LuaWrap.h"

#include "Tools.h"
#include "LogTools.h"

/*
*/
LuaWrap::LuaWrap( const luaL_Reg *libs, const char *chunk, size_t chunk_size )
    : L( luaL_newstate() ) { 
    if (!L) {
        throw E_LOG_OUT_OF_MEMORY();
    }

    for( const luaL_Reg *lib= libs; lib->func; ++lib ) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L,1);
    }

    // Define LOG (for debugging)
    //
    lua_pushcfunction( L, Logger::LOG_ );
    lua_setglobal( L, "LOG" );
    
    // Load in the precompiled chunk
    //
    int st= luaL_loadbuffer( L, chunk, chunk_size, nullptr /*from precompiled*/ );
    if (st!=0) {
        lua_close(L); L=0;
        throw E_LOG_OUT_OF_MEMORY();    // can not be anything else, since precompiled
    }
    
    INVARIANT();
}


/*
*/
LuaWrap::~LuaWrap() {
    INVARIANT();
    lua_close(L);
}

