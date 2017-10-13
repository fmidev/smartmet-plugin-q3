/*
* WRAP.HPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Lua sandboxing for Q3 Plugin.
*
* NOTE: There's a similar object in Metqu sources (LuaWrap).
*/
#ifndef Q3_WRAP_HPP
#define Q3_WRAP_HPP

#include "Tools.h"
#include "Server.hpp"
#include "RequestResponse.hpp"

extern "C" {
# include "lua.h"
# include "lauxlib.h"
}

#include <map>
#include <string>


/*
* Takes care of Lua sandboxing for a single query.
*
* NOTE: If a copy of the '.L' member is made, guarantee its lifespan by
*       explicitly closing the wrapper (i.e. 'destoy()') at end of scope.
*/
class LuaWrapper {
  public:
    // Note: Some parameter is needed, otherwise 'operator lua_State *' does not work.
    //
    LuaWrapper( bool ignore );
    ~LuaWrapper() { INVARIANT(); destroy(); }

    int run( int args, RequestResponse &rr, double killtime_secs, int decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                , bool binary_q2
#endif
            );

    void destroy();     // explicit closing (used for speed measurement)

  // public for REGISTRY_KEY_HOOK
  //
    static void my_hook( lua_State *L, lua_Debug *ar );

    operator lua_State * () { return L; }

  private:
    LuaWrapper( const LuaWrapper & );       // no copy constructor
    void operator=( const LuaWrapper & );   // no assignment

    static int my_traceback( lua_State *L );

    static int run2_( lua_State *L );

    // data members
    //
    lua_State *L;

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        // if destroyed, 'L' can be NULL
        //
    #if 0
        if (L) {        
            // L[1] is the 'my_traceback' closure
            //        
            assert_invariant( lua_gettop(L)>=1 );
            assert_invariant( lua_isfunction(L,1) );
        }
    #endif
    }
#endif
};

#endif
    // Q3_WRAP_HPP
    
