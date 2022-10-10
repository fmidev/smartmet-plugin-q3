/*
* MATRIX.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef MATRIX_HPP
#define MATRIX_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== Matrix ===---
*/
class Matrix;

struct Matrix_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo matrix"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef Matrix CAST_T;

  private:
    static int index( lua_State *L );
};

class Matrix : public LuaNew<Matrix_bind> {
  public:
    operator cairo_matrix_t *() { return &cm; }

    Matrix() :cm() {}
	//~Matrix();

    static int translate( lua_State *L );
    static int scale( lua_State *L );
    static int rotate( lua_State *L );
    static int invert( lua_State *L );
    static int multiply( lua_State *L );
    static int transform_distance( lua_State *L );
    static int transform_point( lua_State *L );

  private:
    // data members
    //
    cairo_matrix_t cm;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
    }
#endif 
};

#endif
    // MATRIX_HPP
    
