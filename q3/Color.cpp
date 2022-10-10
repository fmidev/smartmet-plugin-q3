/*
 * COLOR.CPP                               Copyright 2010, Ilmatieteen laitos
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "Color.hpp"

#include <stdlib.h>
// strtol (on Ubuntu 9.10)

/*
 * Read an RGB or RGBA entry from Lua stack
 *
 *   r_num, g_num, b_num [, a_num]
 *   rgb_str [, a_num]
 *
 * RGB string is of the form "[aa]rrggbb" (similar to HTML and SVG).
 */
Color::Color(lua_State *L, int index, bool alpha) : r(), g(), b(), a(0.0) {
  index = L_ABS(index);

  switch (lua_type(L, index)) {
  case LUA_TSTRING: {
    const char *s = lua_tostring(L, index++);
    char *endp;
    unsigned v = strtol(s, &endp, 16);
    if (*endp != '\0') {
      luaL_error(L, "Bad RGB string: %s", s);
    }

    r = ((v >> 16) & 0xff) / 255.0;
    g = ((v >> 8) & 0xff) / 255.0;
    b = ((v >> 0) & 0xff) / 255.0;
    a = ((v >> 24) & 0xff) / 255.0;
  } break;

  case LUA_TNUMBER:
    r = lua_tonumber(L, index++);
    g = lua_tonumber(L, index++);
    b = lua_tonumber(L, index++);
    break;

  default:
    luaL_error(L, "Bad RGB value: %s", L_typename(index));
  }

  // Is the caller interested of an alpha as well (this may _already_ have been
  // given in the string, but can be given as separate number as well).
  //
  if (alpha) {
    if (lua_isnumber(L, index)) {
      a = lua_tonumber(L, index);
    }
  }
}

/*
 */
int /*1*/ Color::push(lua_State *L) {
  lua_newtable(L);

  lua_pushliteral(L, "r");
  lua_pushnumber(L, r);
  lua_settable(L, -3);

  lua_pushliteral(L, "g");
  lua_pushnumber(L, g);
  lua_settable(L, -3);

  lua_pushliteral(L, "b");
  lua_pushnumber(L, b);
  lua_settable(L, -3);

  lua_pushliteral(L, "a");
  lua_pushnumber(L, a);
  lua_settable(L, -3);

  return 1;
}
