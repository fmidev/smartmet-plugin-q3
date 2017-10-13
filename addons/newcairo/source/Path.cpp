/*
* PATH.CPP                               Copyright 2010, Ilmatieteen laitos
*
* Ref: <http://www.cairographics.org/manual/cairo-paths.html>
*/
#include "LuaNew.h"
#include "Invariant.h"
#include "Proto.h"

#include "Path.hpp"

#include "Converter.hpp"
//#include "Color.hpp"

//#include <string.h>

using namespace std;

LuaNew_ID Path_bind::ID;


/*---=== Helpers ===---
*/


/*---=== Converters ===---
*/

/*struct ExtendConverter : public Converter<cairo_extend_t> {
    ExtendConverter() {
        map( CAIRO_EXTEND_NONE, 	"none" );
        map( CAIRO_EXTEND_REPEAT, 	"repeat" );
        map( CAIRO_EXTEND_REFLECT, 	"reflect" );
        map( CAIRO_EXTEND_PAD, 		"pad" );
    }
};
static ExtendConverter conv_extend;
*/


/*---=== Path ===---
*/

struct PathMethodNames : public MethodNames {
    PathMethodNames() {
        static volatile unsigned initialized;    // = 0
        if (initialized++) throw runtime_error( "There should be only one PathMethodNames" );

        //map( "set_matrix",		        Path::set_matrix );
        
        // ...
    }
};
static struct PathMethodNames path_method_names;

/*
* ...= __index( path_ud, key_any )
*/
int Path_bind::index( lua_State *L ) {
    //Path &my= *Path::instance(L,1);

    L_GROW(2);

    const char *s= lua_tostring(L,2);
    if (s) {
        // Data members
        //
        if (strcmp(s,"info")==0) {
            lua_newtable(L);
            
            //---
            // .argb (argb_str)   (only if the pattern is a solid color pattern)
        /*
            {
            double r,g,b,a;
            cairo_status_t st= cairo_pattern_get_rgba(my, &r,&g,&b,&a);
            if (st==0) {
                lua_pushliteral(L,"rgba");
                Color(r,g,b,a).push(L);
                lua_settable(L,-3);
            }
            }
        */

            return 1;
        }

        // Function members
        //
        lua_CFunction f= path_method_names(s);
        if (f) {
            lua_pushvalue( L,1 );   // copy of the instance
            lua_pushcclosure( L, f, 1 );
            return 1;
        }
    }

    return 0;   // nil
}


/*
*/
void Path_bind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Add our own methods
    //
    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);
}


/*
*/
Path::Path( cairo_path_t *cp_ ) 
    : cp(cp_) { INVARIANT(); }

/*
*/
Path::~Path() {
    INVARIANT();
    
    // Pointer originally received from 'cairo_copy_path[_flat]()'
    //
    cairo_path_destroy(cp);
}


/*
* path= path.xxx( xxx )
*/
#if 0
int Path::set_matrix( lua_State *L ) {
    Path &my= *Path::instance( L, lua_upvalueindex(1) );
    Matrix *matrix= Matrix::instance(L,1);
    if (!matrix) {
        luaL_error( L, "Wanted Matrix, got %s", L_typename(1) );
    }

    cairo_pattern_set_matrix( my, *matrix );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chaining
    return 1;
}
#endif




                       
