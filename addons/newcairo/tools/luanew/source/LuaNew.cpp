/*
* LUANEW.CPP                 Copyright (c) 2008-2009, Ilmatieteen laitos
*/
#include "LuaNew.h"

#include <stdexcept>
#include <cstdarg>
#include <sstream>

#include <iostream>
    // cerr

using namespace std;

/*--- Helpers ---*/



/*--- LuaNew_base ---*/

// Needed by C++ for a pure virtual destructor
//
LuaNew_base::~LuaNew_base() {}


/*
* Takes in a certain type userdata from Lua parameters, including types that
* are derived from it.
*
* Checks that the parameter really is of the required type. Without such check,
* a script could make the C(++) side crash by feeding it wrong type (by mistake
* or by malicious purpose).
*
* Returns NULL for non-userdata or wrong kind of userdata
*/
LuaNew_base *LuaNew_base::instance( lua_State *L, int index, const LuaNew_ID &ID ) {
    void *ud= lua_touserdata(L,index);
    if (!ud) return 0;  // was not a userdata

    L_GROW(2);

    if (lua_getmetatable( L, index )) {
        //
        // [-1]: metatable of given userdata

        for( std::set<const LuaNew_ID *>::const_iterator it= ID.ids.begin();
            it != ID.ids.end();
            ++it ) {
            lua_pushlightuserdata( L, (void*) *it );
            lua_gettable( L, LUA_REGISTRYINDEX );
                //
                // [-1]: metatable of all userdata of 'it' kind
                // [-2]: metatable of given userdata
    
            if (lua_equal( L, -1,-2 )) {
                lua_pop(L,2);
                return (LuaNew_base*)ud;  // right kind :)
            }
            lua_pop(L,1);
        }
        lua_pop(L,1);
    }

    return 0;   // userdata of another kind
}


/*
* Allocate a C++ object by using the Lua userdata's memory. This is optimal,
* allowing us both to use the C++ destructor as well as Lua memory pool.
*
* 'ID' gives the metatable identity of the pushed type, so we can also
* attach the right metatable to the userdata.
*
* 'env_mode' defines, whether each instance is to be given its own environment
* table (i.e. to be used as a userdata specific cache). 
*   NULL:   No env.table
*   "":     Regular env. table
*   "k"|"v"|"kv":   Env. table with weak keys, values or both (see Lua manual)
*
* IMPORTANT NOTE: 
*       If the constructor were to be cancelled (an exception thrown within it)
*   the '::nuke()' method MUST BE CALLED before advancing. Otherwise Lua will
*   eventually try to destruct a non-existing object.
*
*   Or rather, just restrain from throwing exceptions within 'new(L)' allocated
*   constructors (..but it's not always in your power). You've been warned!
*/
void *LuaNew_base::push( lua_State *L, size_t bytes, const LuaNew_ID &ID, const char *name, const char *env_mode ) {
    static const runtime_error nomem_exc( "out of memory" );    // so throwing it won't cause allocs :)
    L_GROW(2);

    void *ud= lua_newuserdata( L, bytes );
    if (!ud) { throw nomem_exc; }

//cerr << ">>> allocated: " << (void*)ud << " " << name << endl;

    // [-1]: memory allocated for the C++ object

    lua_pushlightuserdata( L, (void*) &ID );
    lua_gettable( L, LUA_REGISTRYINDEX );
    if (!lua_istable(L,-1)) {
        lua_pop(L,2);   // clear 'nil' and allocated userdata

        ostringstream os;
        os << "Metatable not set up for: " << (name ? name:"unknown");
        string tmp= os.str();
        throw runtime_error( tmp.c_str() );
    }
    
    // This connects to GC - from now on Lua will call this object's destructor
    // when there's no more references to the object on the Lua side.
    //
    lua_setmetatable( L, -2 );

    // Create an environment table for it.
    //
    if (env_mode) {
        lua_newtable(L);
        
        if (*env_mode) {
            // Note: We're using the table itself as its metatable; this should not
            //      cause problems, right (strings are not garbage collected).
            //
            // TBD: It would actually be enough to just make a single metatable for 
            //      the environments, and apply it here to all the created env.tables.
            //
            L_GROW(2);
            lua_pushliteral(L,"__mode");
            lua_pushstring(L,env_mode);
            lua_settable(L,-3);
            
            lua_pushvalue(L,-1);    // 2nd ref to the env.table
            lua_setmetatable(L,-2);
        }

        // [-1]: env.table 
        // [-2]: userdata
        
        lua_setuservalue( L, -2 );
    }
    
    return ud;      // proceed to C++ constructor
}

/*
* Nuke the object after a constructor has thrown an exception; otherwise the 
* destructor will (eventually, at GC) get called for a non-valid C++ object.
*/
void LuaNew_base::nuke( lua_State *L, int idx ) {
    L_ASSERT( lua_type(L,idx) == LUA_TUSERDATA );
    int idx_abs= L_ABS(idx);

    L_GROW(1);
    lua_pushnil(L);
    lua_setmetatable( L, idx_abs );     // now '__gc()' won't be called
    
    // Also remove the dummy userdata from the Lua stack
    //
    lua_remove( L, idx_abs );
}


/*
* Keep 'index_ref' Lua object alive (out of GC) until 'index_this' (a LuaNew-derived
* userdata) has been destroyed.
*
* This functionality is provided using the Lua userdata environment table 1..N indices.
* The application can use empty indices, or indices other than 1..N, but NOT TOUCH the
* ones we've added.
*
* Returns: 
*   the index used for keeping 'index_ref' alive. This can be used for reclaiming
*   a copy of that value via 'push_alive()'.
*/
unsigned /*key*/ LuaNew_base::keep_alive( lua_State *L, int index_this, int index_ref ) {

    int index_ref_abs= L_ABS(index_ref);

    int index_this_abs= L_ABS(index_this);
    L_GROW(3);

    lua_getuservalue( L, index_this_abs );
    if (lua_isnil(L,-1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L,-1);    // 2nd ref
        lua_setuservalue( L, index_this_abs );
    }
    L_ASSERT( lua_istable(L,-1) );
    
    size_t n= lua_objlen( L, -1 );  // current number of array members (0..N)
    lua_pushinteger( L, ++n );             // key
    lua_pushvalue( L, index_ref_abs );     // value

    lua_rawset( L, -3 );    // now tied in the registry table
    lua_pop(L,1);
    
    return n;
}

/*
* Push the Lua value kept alive for 'index_this' onto Lua stack, or return
* 'false' if none.
*/
bool LuaNew_base::push_alive( lua_State *L, int index_this, unsigned key ) {
    int index_this_abs= L_ABS(index_this);
    L_GROW(2);

L_START
    lua_getuservalue( L, index_this_abs );
    if (lua_isnil(L,-1)) {
        lua_pop(L,1);
L_MID(0)
        return false;   // no environment table
    }
    L_ASSERT( lua_istable(L,-1) );

    lua_pushinteger( L, key );
    lua_rawget( L, -2 );
    lua_remove( L, -2 );    // remove env.table

    // [-1]: nil or any value
L_END(1)

    if (lua_isnil(L,-1)) {
        lua_pop(L,1);
        return false;   // 'env_table[key]' was empty
    }
    return true;    // value at [-1]
}

/*
* All objects derived from 'LuaNew' and created using the custom 'new'
* will call here during their garbage collection (well, unless they've
* been 'nuke()'d)".
*
* Upvalue: 1: name of the class (for debugging)
*/
int LuaNew_base::__gc( lua_State *L ) {
    LuaNew_base *obj= (LuaNew_base*)lua_touserdata(L,1);
    assert(obj);

    delete obj;     // will call the destructor of the particular derived class

    // References via 'keep_alive' are automatically cleared.
    
    return 0;
}


/*
* Creates a new metatable and binds the class to Lua.
* Sets '__gc' method to bind to C++ delete.
* Sets '__type' to the class's name (used by customized 'type()' function).
*/
void LuaNew_base::create_mt( lua_State *L, f_setup setup_f, const LuaNew_ID &_id, const char *name ) {
    assert(name);

    L_GROW(3);
    
    lua_newtable(L);

    // Fill the table with '__index' etc.
    //
    (*setup_f)( L );     // [-1]: metatable
    
    // Add '__gc' method (overwrites any placed by setup func(s))
    //
    lua_pushliteral( L, "__gc" );

    lua_pushstring( L, name );
    lua_pushcclosure( L, __gc, 1 /*upvalues*/ );
    lua_settable( L, -3 );

    if (name) {
        lua_pushliteral( L, "__type" );
        lua_pushstring( L, name );
        lua_settable( L, -3 );
    }

    // Tie to registry
    //
    lua_pushlightuserdata( L, (void*) &_id );
    lua_insert(L,-2);
    lua_settable( L, LUA_REGISTRYINDEX );
}


/*
* Like 'lua_typename(L,lua_type(L,index))' but digs out the userdata subtypes of
* LuaNew-derived userdata objects.
*/
const char *LuaNew_base::typename_( lua_State *L, int index ) {
    int index_abs= L_ABS(index);
    int tt= lua_type(L,index);
    const char *normal_typename= lua_typename(L,tt);

    L_GROW(3);

    // Check 'metatable.__type' first. This allows i.e. tables to have a subtype (not only userdata)
    //
    if (lua_getmetatable(L,index_abs)) {
        //
        // [-1]: metatable
        
        lua_pushliteral( L, "__type" );
        lua_gettable( L, -2 );
        
        switch( lua_type(L,-1) ) {
            case LUA_TSTRING: {
                const char *s= lua_pushfstring( L, "%s:%s", normal_typename, lua_tostring(L,-1) );

                // Leaving the type name on TOS would confuse the caller. Removing it might
                // make the C pointer invalid (not immediately, but in later GC). We need to
                // give a second reference somewhere.
                //
                keep_alive( L, index_abs, -1 );

                lua_pop(L,3);
                return s;
            }

            case LUA_TNIL:
                lua_pop(L,2);   // remove nil and metatable reference (no '.__type' field)
                break;
                
            default:
                luaL_error( L, "__type metaentry not a string" );
                break;  // never
        }
    }

    return normal_typename;
}


/*
* Dump out Lua stack contents (for debugging)
*/
void LuaNew_base::dump( lua_State *L, const char *file, int line ) {

    int top= lua_gettop(L);
    int i;

	fprintf( stderr, "\n  LUA STACK: [%s:%d]\n", file, line );

	if (top==0)
		fprintf( stderr, "\t(none)\n" );

	for( i=1; i<=top; i++ ) {
		// 'L_typename()' gives "userdata:xxx" kind of names (= more info than Lua itself)
		//
		fprintf( stderr, "\t[%d]= (%s) ", i, L_typename(i) );     

		// Print item contents here...
		//
		// Note: this requires 'tostring()' to be defined. If it is NOT,
		//       enable it for more debugging.
		//
    L_START
        L_GROW(2);

        lua_getglobal( L, "tostring" );
            //
            // [-1]: tostring function, or nil
        
        if (!lua_isfunction(L,-1)) {
             fprintf( stderr, "('tostring' not available)" );
         } else {
             lua_pushvalue( L, i );
             lua_call( L, 1 /*args*/, 1 /*retvals*/ );

             fputs( lua_tostring(L,-1), stderr );
         }
         lua_pop(L,1);
    L_END(0)
		fprintf( stderr, "\n" );
		}
	fprintf( stderr, "\n" );
}


/*
* To be used in functions that push 'nil, err_str' on errors.
*/
void _L_nilerr( lua_State *L, const char *s ) {
    L_ASSERT(s);

    L_GROW(2);
    lua_pushnil(L);
    lua_pushstring( L, s );
}

void _L_nilerr_fmt( lua_State *L, const char *fmt, ... ) {
    L_ASSERT(fmt);

    L_GROW(2);
    lua_pushnil(L);

    /*
    * By using 'lua_pushvfstring()' we don't need to make a buffer of arbitrary size
    * (and won't ever get truncations on the report).
    */
    va_list vl;
    va_start( vl, fmt );
    {
        lua_pushvfstring( L, fmt, vl );
    }
    va_end( vl );
}


