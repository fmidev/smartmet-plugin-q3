/*
* MATRIX.CPP                               Copyright 2010, Ilmatieteen laitos
*
* Ref: <http://www.cairographics.org/manual/cairo-matrix.html#cairo-matrix-t>
*/
#include "LuaNew.h"
#include "Invariant.h"
#include "Proto.h"

#include "Matrix.hpp"

#include "Converter.hpp"
//#include "Color.hpp"

#include <string.h>

using namespace std;

LuaNew_ID Matrix_bind::ID;


/*---=== Helpers ===---
*/


/*---=== Matrix ===---
*/

struct MatrixMethodNames : public MethodNames {
    MatrixMethodNames() {
        static volatile unsigned initialized;    // = 0
        if (initialized++) throw runtime_error( "There should be only one MatrixMethodNames" );

        map( "translate",			Matrix::translate );
        map( "scale",				Matrix::translate );
        map( "rotate",				Matrix::translate );
        map( "invert",		        Matrix::translate );
        map( "multiply",		  	Matrix::translate );
        map( "transform_distance",	Matrix::translate );
        map( "transform_point",     Matrix::translate );

        // ...
    }
};
static struct MatrixMethodNames matrix_method_names;

/*
* ...= __index( pattern_ud, key_any )
*/
int Matrix_bind::index( lua_State *L ) {
    Matrix &my= *Matrix::instance(L,1);

    L_GROW(2);

    const char *s= lua_tostring(L,2);
    if (s) {
        // Data members
        //
        if ((*s=='x') || (*s=='y')) {
            cairo_matrix_t tmp= *my;

            if (strcmp(s,"xx")==0) {
                lua_pushnumber(L,tmp.xx);
                return 1;
            }
            if (strcmp(s,"yx")==0) {
                lua_pushnumber(L,tmp.yx);
                return 1;
            }
            if (strcmp(s,"xy")==0) {
                lua_pushnumber(L,tmp.xy);
                return 1;
            }
            if (strcmp(s,"yy")==0) {
                lua_pushnumber(L,tmp.yy);
                return 1;
            }
            if (strcmp(s,"x0")==0) {
                lua_pushnumber(L,tmp.x0);
                return 1;
            }
            if (strcmp(s,"y0")==0) {
                lua_pushnumber(L,tmp.y0);
                return 1;
            }
        }

        if (strcmp(s,"info")==0) {
            lua_newtable(L);
            
            //---
            // .argb (argb_str)   (only if the pattern is a solid color pattern)
#if 0
            {
            double r,g,b,a;
            cairo_status_t st= cairo_pattern_get_rgba(my, &r,&g,&b,&a);
            if (st==0) {
                lua_pushliteral(L,"rgba");
                push_rgba_string( L, r,g,b,a );
                lua_settable(L,-3);
            }
            }
#endif

            //---
            //...

            return 1;
        }

        // Function members
        //
        lua_CFunction f= matrix_method_names(s);
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
void Matrix_bind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Add our own methods
    //
    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);
}


/*
* matrix= matrix.translate( tx_num, ty_num )
*/
int Matrix::translate( lua_State *L ) {
    proto( L, "number,number" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    double tx= lua_tonumber(L,1);
    double ty= lua_tonumber(L,2);

    cairo_matrix_translate( my, tx,ty );
    
    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* matrix= matrix.scale( sx_num, sy_num )
*/
int Matrix::scale( lua_State *L ) {
    proto( L, "number,number" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    double sx= lua_tonumber(L,1);
    double sy= lua_tonumber(L,2);

    cairo_matrix_scale( my, sx,sy );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* matrix= matrix.rotate( rad_num )
*/
int Matrix::rotate( lua_State *L ) {
    proto( L, "number" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    double rad= lua_tonumber(L,1);

    cairo_matrix_rotate( my, rad );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* matrix= matrix.invert()
*/
int Matrix::invert( lua_State *L ) {
    proto( L, "" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );

    cairo_matrix_invert( my );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* matrix= matrix.multiply( b_matrix )
*
* Note: Unlike in Cairo C API, we use the same matrix for the first param, and the
*       destination.
*/
int Matrix::multiply( lua_State *L ) {
    proto( L, "CairoMatrix" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    Matrix &mb= Matrix::instance_notnull( L, 1 );

    cairo_matrix_t ma= *my;     // just in case (if 'cairo_matrix_multiply' would corrupt 
                                // when destination and source are the same)
    cairo_matrix_multiply( my, &ma, mb );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* dx', dy'= matrix.transform_distance( dx_num, dy_num )
*/
int Matrix::transform_distance( lua_State *L ) {
    proto( L, "number,number" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    double dx= lua_tonumber(L,1);
    double dy= lua_tonumber(L,2);

    cairo_matrix_transform_distance( my, &dx, &dy );

    lua_pushnumber( L, dx );
    lua_pushnumber( L, dy );
    return 2;
}

/*
* x', y'= matrix.transform_point( x_num, y_num )
*/
int Matrix::transform_point( lua_State *L ) {
    proto( L, "number,number" );
    Matrix &my= *Matrix::instance( L, lua_upvalueindex(1) );
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);

    cairo_matrix_transform_point( my, &x, &y );

    lua_pushnumber( L, x );
    lua_pushnumber( L, y );
    return 2;
}


