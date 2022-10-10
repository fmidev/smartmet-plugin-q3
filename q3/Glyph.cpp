/*
 * GLYPH.CPP                               Copyright 2010, Ilmatieteen laitos
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "Glyph.hpp"

#include "Converter.hpp"

#include <string.h>

using namespace std;

LuaNew_ID Glyph_bind::ID;

struct GlyphMethodNames : public MethodNames {
  GlyphMethodNames() {
    static volatile unsigned initialized; // = 0
    if (initialized++)
      throw runtime_error("There should be only one GlyphMethodNames");

    // map( "set_matrix",		        Glyph::set_matrix );

    // ...
  }
};
static struct GlyphMethodNames glyph_method_names;

/*
 * ...= __index( Glyph_ud, key_any )
 */
int Glyph_bind::index(lua_State *L) {
  // Glyph &my= *Glyph::instance(L,1);

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
    lua_CFunction f = glyph_method_names(s);
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
void Glyph_bind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Add our own methods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}
