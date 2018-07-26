/*
* SCALEDFONT.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef SCALEDFONT_HPP
#define SCALEDFONT_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== ScaledFont ===---
*/
class ScaledFont;

struct ScaledFont_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo scaled font"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef ScaledFont CAST_T;

  private:
    static int index( lua_State *L );
};

/*
*/
class ScaledFont : public LuaNew<ScaledFont_bind> {
  public:
    operator cairo_scaled_font_t *() { return sf; }

    ScaledFont( cairo_scaled_font_t *sf_ );
	~ScaledFont();

    static int text_extents( lua_State *L );

  private:
    // data members
    //
    cairo_scaled_font_t *sf;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( sf != 0 );
    }
#endif 
};

#endif
    // SCALEDFONT_HPP
    
