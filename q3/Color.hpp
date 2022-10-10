/*
* COLOR.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef COLOR_HPP
#define COLOR_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"


/*---=== Color ===---
*/
struct Color {
    Color( lua_State *L, int index, bool alpha=false );

    Color( double r_, double g_, double b_, double a_=0.0 )
        : r(r_), g(g_), b(b_), a(a_) {}

    int /*1*/ push( lua_State *L );

    double r,g,b,a;
};

#endif
    // COLOR_HPP
    
