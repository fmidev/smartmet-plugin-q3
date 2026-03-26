/*
* LUANEW.H                   Copyright (c) 2008-2009, Ilmatieteen laitos
*
* Tools for tying C++ classes to Lua.
*
* Reference:
*   <http://www.parashift.com/c++-faq-lite/multiple-inheritance.html>
*   (about use of "dreaded diamond" and virtual inheritance)
*
* WARNING:
*   Do NOT use the Lua stack in constructor of an object derived from 'LuaObject'.
*   Doing so will trigger a "pure virtual" function call (via 'LuaObject::__gc')
*   if a garbage collection cycle is initiated by accessing the Lua stack.
*   -- AKa 23-Nov-2009
*/
#ifndef LUANEW_H
#define LUANEW_H

#include <cassert>

#include <set>
#include <string>

extern "C" {
  #include "luajit-2.1/lua.h"
  #include "luajit-2.1/lualib.h"
  #include "luajit-2.1/lauxlib.h"
}

#define L_typename(n) LuaNew_base::typename_( L, n )

/*
* Usage:
*<<
* class XXX_ID : public LuaNew_base::ID {
*   public:
*     XXX_ID() : LuaNew_base::ID("XXX") {}
*
*     static void setup( lua_State *L ) {
*       // Fill in metatable at [-1]
*       ...
*     }
*   private:
*     ...
* };
*
* class XXX : public LuaNew< XXX_ID > {
*   ...
* };
*
* XXX::create_mt( L );                  // set up Lua/C++ binding
* XXX *p= new(L) XXX(...);              // push 'XXX' onto the Lua stack
* XXX *p= XXX::instance( L, index );    // get a parameter
*   // released automatically by Lua GC, leading to 'XXX' destructor
*<<
*
*   'XXX::setup()' is called to set up the class's metatable. The metatable
*   is at [-1] (top of stack). The function can either set it up all by
*   itself or call a base class's 'setup()' to prepare some of it first.
*
*   'XXX::instance()' is called similar to 'lua_toxxx()' to get a parameter
*   given on Lua stack. If the value is not userdata, or not of the right class,
*   NULL is returned (just as 'lua_toxxx()' does).
*/

/*
* Each Lua bound class (to have a metatable of its own) will have ONE
* such (static or global) object.
*/
class LuaNew_ID {
  private:
    std::set<const LuaNew_ID *> ids;   // own id and derived id's
    friend class LuaNew_base;

  public:
    LuaNew_ID() : ids() { ids.insert(this); /* the class itself */ }

    void me2( const LuaNew_ID &id2 ) {
        ids.insert( &id2 );    // adds a derived class to our knowledge
    }
};

/*
* Functions of 'LuaNew' which don't have to be templated are placed here.
*/
class LuaNew_base {
  protected:
    LuaNew_base() {}          // not for direct use, only deriving
    virtual ~LuaNew_base() = 0;  // gateway from Lua GC -> C++ destructor of derived class

  protected:
    typedef void (*f_setup)( lua_State *L );
    static void create_mt( lua_State *L, f_setup, const LuaNew_ID &_id, const char *name );

    static LuaNew_base *instance( lua_State *L, int index, const LuaNew_ID &_id );

    static void *push( lua_State *L, size_t size, const LuaNew_ID &_id, const char *name, const char *env_mode );

  public:
    /*
    * Call this (with 'idx'==-1) if an exception happened in a constructor
    * that was allocated by 'new(L)'. Detaches the C++ destructor call from
    * the object's GC (otherwise, horrifying things await!).
    */
    static void nuke(lua_State *L, int idx );

    static unsigned /*key*/ keep_alive( lua_State *L, int index_this, int index_ref );
    static bool push_alive( lua_State *L, int index_this, unsigned key );

    static const char *typename_( lua_State *L, int index );

    static void dump( lua_State *L, const char *file, int line );

    /*
    * This dummy 'delete' matches the custom 'new' of derived classes.
    * We don't need to do anything; Lua will release the memory once we're out.
    *
    * Seems this needs to be 'public' (Ubuntu 9.04, gcc 4.3.3)  AKa 25-May-2009
    */
  public:
    static void operator delete(void *) {}

  private:
    static int __gc( lua_State *L );
};


/*
* Interface required for actual use.
*
* Note: 'public virtual' makes us have only a single 'LuaNew', even if
*       classes ended up deriving us multiple times (i.e. a class which is-a 
*       'LuaNew' is derived from a class which also is-a 'LuaNew'.
*       i.e. 'Matrix' and 'SubMatrix' need this in FMI Q3 code).
*
* 06-Feb-2012 PKi: References to BIND::ID replaced by call to BIND_T::id(), thus avoiding
*		     libfmi-q3.so build dependency to newcairo library (was made 01-Feb-2012)
*		     which in turn avoids rpm build problem (does not handle the dependency currently).
*
*/
template< class BIND_T > class LuaNew : public virtual LuaNew_base {
  private:
    typedef typename BIND_T::CAST_T *CAST_PTR;
    typedef typename BIND_T::CAST_T &CAST_REF;

  public:
    /*
    * 'operator new()' returns 'void *' (not 'CAST_PTR') by C++ convention, 
    * since the object is not yet constructed.
    */
    static void *operator new(size_t size, lua_State *L) {
        return LuaNew_base::push( L, size, BIND_T::id(), BIND_T::name(), BIND_T::env_mode() );
    }

    static void create_mt( lua_State *L ) {
        LuaNew_base::create_mt( L, BIND_T::setup, BIND_T::id(), BIND_T::name() );
    }

    static CAST_PTR instance( lua_State *L, int index ) {
        return (CAST_PTR) (void*) LuaNew_base::instance( L, index, BIND_T::id() );
    }

    static CAST_REF instance_notnull( lua_State *L, int index ) {
        CAST_PTR p= (CAST_PTR) (void*) LuaNew_base::instance( L, index, BIND_T::id() );
        if (!p) {
            luaL_error( L, "Expecting '%s', got %s", BIND_T::name(), L_typename(index) );
        }
        return *p;
    }

    // 
    // bool= is( any )    
    //
    // Used by the 'proto' system to check if 'any' is of this kind.
    //
    static int is( lua_State *L ) {
        lua_pushboolean( L, instance(L,1) != 0 );
        return 1;
    }
    
    static const char *name() {
        return BIND_T::name();
    }
};


/*--- Misc tools ---*/

#define L_DUMP() LuaNew_base::dump( L, __FILE__, __LINE__ )

#define L_string_or_typename(n) \
    ( lua_isstring(L,(n)) ? lua_tostring(L,(n)) : LuaNew_base::typename_(L,(n)) )

#define L_GROW(n) \
    { if (!lua_checkstack(L,n)) { throw std::runtime_error("Out of Lua stack"); } }

#define L_ASSERT(cond) \
    { if (!(cond)) { \
        luaL_error( L, "Assert failed at %s:%d: %s", __FILE__, __LINE__, #cond ); \
    } }

#define L_ABS(n) \
	( ((n) >= 0 || (n) <= -10000 /*LUA_REGISTRYINDEX*/) ? \
	   (n) /*absolute or special index*/ : (lua_gettop(L)+(n)+1) )

#define L_START \
    { int __tos= lua_gettop(L);

#define L_MID(change) \
    { int __tos_now= lua_gettop(L); \
        if (__tos_now != __tos+change) { \
            LuaNew_base::dump( L, __FILE__, __LINE__ ); \
            luaL_error( L, "Stack assert failed at %s:%d: has %d items instead of %d", \
                                __FILE__, __LINE__, __tos_now, __tos+change ); \
        } \
    }

#define L_END(change) L_MID(change); }

/*
* Giving the value 2 means it can be used directly as 'return L_nilerr(reason)'
*/
void _L_nilerr( lua_State *L, const char *s );
#define L_nilerr( str ) (_L_nilerr( L, str ), 2)

void _L_nilerr_fmt( lua_State *L, const char *fmt, ... );
#define L_nilerr_fmt( fmt, ... ) (_L_nilerr_fmt( L, fmt, __VA_ARGS__ ), 2)

#endif
    // LUANEW_H
