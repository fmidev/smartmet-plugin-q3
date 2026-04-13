/*
 * FONTFACE.CPP                               Copyright 2010, Ilmatieteen laitos
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "FontFace.hpp"

#include "Converter.hpp"

#include <string.h>

using namespace std;

LuaNew_ID FontFace_bind::ID;

struct FontFaceMethodNames : public MethodNames {
  FontFaceMethodNames() {
    static volatile unsigned initialized; // = 0
    unsigned was = initialized;
    initialized = initialized + 1;
    if (was)
      throw runtime_error("There should be only one FontFaceMethodNames");

    // map( "set_matrix",		        Glyph::set_matrix );

    // ...
  }
};
static struct FontFaceMethodNames font_face_method_names;

/*
 * ...= __index( FontFace_ud, key_any )
 */
int FontFace_bind::index(lua_State *L) {
  // FontFace &my= *FontFace::instance(L,1);

  L_GROW(2);

  const char *s = lua_tostring(L, 2);
  if (s) {
    // Data members
    //
    if (strcmp(s, "info") == 0) {
      lua_newtable(L);

      //---
      //...

      return 1;
    }

    // Function members
    //
    lua_CFunction f = font_face_method_names(s);
    if (f) {
      lua_pushvalue(L, 1); // copy of the instance
      lua_pushcclosure(L, f, 1);
      return 1;
    }
  }
  return 0; // nil
}

/*
 */
void FontFace_bind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Add our own methods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}

/*---=== FontFace ===---*/

/*
 */
FontFace::FontFace(cairo_font_face_t *ff_) : ff(ff_) { INVARIANT(); }

FontFace::~FontFace() {
  INVARIANT();
  cairo_font_face_destroy(ff);
}

/*
 * Load a TrueType font from a file.
 */
#ifdef CAIRO_HAS_FT_FONT
cairo_font_face_t *FontFace::load_ttf(const char *fn, unsigned face_index,
                                      FT_Library &ft_lib) {

  // Filename must have no slashes if running on server
  //
#ifdef SANDBOX
  if (strchr(fn, '/')) {
    return nullptr; // refused
  }

  // Prefix with where the server fonts are stored
  //
  char buf[FILENAME_MAX];
  snprintf(buf, sizeof(buf), CUSTOM_FONT_PATH "/%s", fn);
  fn = buf;
#endif

  // Get us the FreeType font
  //
  FT_Face ft_face;
  FT_Error ft_err = FT_New_Face(ft_lib, fn, face_index, &ft_face);
  if (ft_err) {
    return nullptr; // no such file (or something else)
  }

  // Tie 'ft_face' to Cairo (also its lifespan)
  //
  // Ref.
  // http://www.cairographics.org/manual/cairo-FreeType-Fonts.html#cairo-ft-font-face-create-for-ft-face
  //
  static const cairo_user_data_key_t key = {0};

  cairo_font_face_t *ff =
      cairo_ft_font_face_create_for_ft_face(ft_face, 0 /*options*/);
  int st = cairo_font_face_set_user_data(ff, &key, ft_face,
                                         [](void *p) { FT_Done_Face(static_cast<FT_Face>(p)); });
  if (st) {
    cairo_font_face_destroy(ff);
    FT_Done_Face(ft_face);
    return nullptr;
  }

  return ff;
}
#endif
