/*
* GLYPH.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef GLYPH_HPP
#define GLYPH_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== Glyph ===---
*/
class Glyph;

struct Glyph_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo glyph"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef Glyph CAST_T;

  private:
    static int index( lua_State *L );
};

/*
*/
class Glyph : public LuaNew<Glyph_bind> {
  public:
    operator cairo_glyph_t *() { return glyph; }

    Glyph( cairo_glyph_t *glyph_ );
	~Glyph();

    //static int save( lua_State *L );

  private:
    // data members
    //
    cairo_glyph_t *glyph;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( glyph != 0 );
    }
#endif 
};

#endif
    // GLYPH_HPP
    
