/*
* PATTERN.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef PATTERN_HPP
#define PATTERN_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== Pattern ===---
*/
class Pattern;

struct Pattern_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo pattern"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef Pattern CAST_T;

  private:
    static int index( lua_State *L );
};

class Pattern : public LuaNew<Pattern_bind> {
  public:
    operator cairo_pattern_t *() { return cp; }
    operator const cairo_pattern_t *() const { return cp; }

    Pattern( cairo_pattern_t *cp_ );
	~Pattern();

    static int add_color_stop_rgb( lua_State *L );
    static int add_color_stop_rgba( lua_State *L );
    static int get_color_stop_rgba( lua_State *L );
    static int set_extend( lua_State *L );
    static int set_filter( lua_State *L );
    static int set_matrix( lua_State *L );

  private:
    // data members
    //
    cairo_pattern_t *cp;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( cp != 0 );
    }
#endif 
};

#endif
    // PATTERN_HPP
    
