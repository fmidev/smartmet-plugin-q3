/*
* FONTFACE.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef FONTFACE_HPP
#define FONTFACE_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>
#ifdef CAIRO_HAS_FT_FONT
# include <cairo-ft.h>
#endif


/*---=== FontFace ===---
*/
class FontFace;

struct FontFace_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo font face"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef FontFace CAST_T;

  private:
    static int index( lua_State *L );
};

/*
*/
class FontFace : public LuaNew<FontFace_bind> {
  public:
    operator cairo_font_face_t *() { return ff; }

    FontFace( cairo_font_face_t *ff_ );
	~FontFace();

    //static int save( lua_State *L );

#ifdef CAIRO_HAS_FT_FONT
    static cairo_font_face_t *load_ttf( const char *fn, unsigned face_index, FT_Library &ft_lib );
#endif

  private:
    // data members
    //
    cairo_font_face_t *ff;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( ff != 0 );
    }
#endif 
};

#endif
    // FONTFACE_HPP
    
