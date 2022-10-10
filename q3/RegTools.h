/*
 * REGTOOLS.H                       Copyright (c) 2010, Ilmatieteen laitos
 */
#ifndef REGTOOLS_H
#define REGTOOLS_H

#include "Tools.h"

/*---=== Output decimals and MIME control (via Lua stack registry) ===---
 */

struct RegTools { // just namespace
private:
  static void *REG_DECIMALS;
  static void *REG_JSONP;
#ifndef METQU
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  static void *REG_BINARY;
#endif
#else
  static void *REG_FILENAME;
#endif

public:
  static int get_Decimals(lua_State *L) { return get_uint(L, REG_DECIMALS); }
  static void set_Decimals(lua_State *L, unsigned decs) {
    set(L, REG_DECIMALS, decs);
  }

  static bool get_JSONP(lua_State *L) { return get_bool(L, REG_JSONP); }
  static void set_JSONP(lua_State *L, bool v) { set(L, REG_JSONP, v); }

#ifndef METQU
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  static bool get_Binary(lua_State *L) { return get_bool(L, REG_BINARY); }
  static void set_Binary(lua_State *L, bool v) { set(L, REG_BINARY, v); }
#endif
#else
  static string_or_null get_Filename(lua_State *L) {
    return get_str(L, REG_FILENAME);
  }
  static void set_Filename(lua_State *L, const char *s) {
    set(L, REG_FILENAME, s);
  }
#endif

private:
  static string_or_null get_str(lua_State *L, void *key);
  static void set(lua_State *L, void *key, const char *s);

  static int get_uint(lua_State *L, void *key);
  static void set(lua_State *L, void *key, unsigned v);

  static bool get_bool(lua_State *L, void *key);
  static void set(lua_State *L, void *key, bool v);
};

#endif
// REGTOOLS_H
