/*
 * SCALEDFONT.CPP                               Copyright 2010, Ilmatieteen
 * laitos
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "ScaledFont.hpp"

#include "Converter.hpp"

#include <string.h>

using namespace std;

LuaNew_ID ScaledFont_bind::ID;

struct ScaledFontMethodNames : public MethodNames {
  ScaledFontMethodNames() {
    static volatile unsigned initialized; // = 0
    if (initialized++)
      throw runtime_error("There should be only one ScaledFontMethodNames");

    // map( "set_matrix",		        Glyph::set_matrix );

    // ...
  }
};
static struct ScaledFontMethodNames scaled_font_method_names;

/*
 * ...= __index( ScaledFont_ud, key_any )
 */
int ScaledFont_bind::index(lua_State *L) {
  // ScaledFont &my= *ScaledFont::instance(L,1);

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
    lua_CFunction f = scaled_font_method_names(s);
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
void ScaledFont_bind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Add our own methods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}

/*---=== ScaledFont ===---*/

/*
 */
ScaledFont::ScaledFont(cairo_scaled_font_t *sf_) : sf(sf_) { INVARIANT(); }

ScaledFont::~ScaledFont() {
  INVARIANT();
  cairo_scaled_font_destroy(sf);
}

/*
 * { x_bearing= num,
 *   y_bearing= num,
 *   width= num,
 *   height= num,
 *   x_advance= num,
 *   y_advance= num
 * }= scaled_font.text_extents( utf8_str )
 */
int ScaledFont::text_extents(lua_State *L) {
  ScaledFont &my = *ScaledFont::instance(L, lua_upvalueindex(1));
  const char *s = lua_tostring(L, 1);

  cairo_text_extents_t ext;
  cairo_scaled_font_text_extents(my, s, &ext);

  lua_newtable(L);

  lua_pushliteral(L, "x_bearing");
  lua_pushnumber(L, ext.x_bearing);
  lua_pushliteral(L, "y_bearing");
  lua_pushnumber(L, ext.y_bearing);
  lua_pushliteral(L, "width");
  lua_pushnumber(L, ext.width);
  lua_pushliteral(L, "height");
  lua_pushnumber(L, ext.height);
  lua_pushliteral(L, "x_advance");
  lua_pushnumber(L, ext.x_advance);
  lua_pushliteral(L, "y_advance");
  lua_pushnumber(L, ext.y_advance);

  return 1;
}
