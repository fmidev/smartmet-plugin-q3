/*
 * PATTERN.CPP                               Copyright 2010, Ilmatieteen laitos
 *
 * Ref: <http://www.cairographics.org/manual/cairo-pattern.html>
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "Context.hpp"
#include "Matrix.hpp"
#include "Pattern.hpp"

#include "Color.hpp"
#include "Converter.hpp"

#include <string.h>

using namespace std;

LuaNew_ID Pattern_bind::ID;

/*---=== Helpers ===---
 */

static const unsigned GRANT_SOLID = 0x01;
static const unsigned GRANT_SURFACE = 0x02;
static const unsigned GRANT_LINEAR = 0x04;
static const unsigned GRANT_RADIAL = 0x08;

static unsigned get_grant(/*const*/ cairo_pattern_t *p) {
  cairo_pattern_type_t pt = cairo_pattern_get_type(p);

  return (pt == CAIRO_PATTERN_TYPE_SOLID)     ? GRANT_SOLID
         : (pt == CAIRO_PATTERN_TYPE_SURFACE) ? GRANT_SURFACE
         : (pt == CAIRO_PATTERN_TYPE_LINEAR)  ? GRANT_LINEAR
         : (pt == CAIRO_PATTERN_TYPE_RADIAL)  ? GRANT_RADIAL
                                              : 0 /*should not occur*/;
}

/*---=== Converters ===---
 */
struct ExtendConverter : public Converter<cairo_extend_t> {
  ExtendConverter() : Converter<cairo_extend_t>("extend") {
    map(CAIRO_EXTEND_NONE, "none");
    map(CAIRO_EXTEND_REPEAT, "repeat");
    map(CAIRO_EXTEND_REFLECT, "reflect");
    map(CAIRO_EXTEND_PAD, "pad");
  }
};
static ExtendConverter conv_extend;

//--
struct FilterConverter : public Converter<cairo_filter_t> {
  FilterConverter() : Converter<cairo_filter_t>("filter") {
    map(CAIRO_FILTER_FAST, "fast");
    map(CAIRO_FILTER_GOOD, "good");
    map(CAIRO_FILTER_BEST, "best");
    map(CAIRO_FILTER_NEAREST, "nearest");
    map(CAIRO_FILTER_BILINEAR, "bilinear");
    map(CAIRO_FILTER_GAUSSIAN, "gaussian");
  }
};
static FilterConverter conv_filter;

//--
struct PatternTypeConverter : public Converter<cairo_pattern_type_t> {
  PatternTypeConverter() : Converter<cairo_pattern_type_t>("pattern_type") {
    map(CAIRO_PATTERN_TYPE_SOLID, "solid");
    map(CAIRO_PATTERN_TYPE_SURFACE, "surface");
    map(CAIRO_PATTERN_TYPE_LINEAR, "linear");
    map(CAIRO_PATTERN_TYPE_RADIAL, "radial");
  }
};
static PatternTypeConverter conv_pattern_type;

/*---=== Pattern ===---
 */

struct PatternMethodNames : public MethodNames {
  PatternMethodNames() {
    static volatile unsigned initialized; // = 0
    if (initialized++)
      throw runtime_error("There should be only one PatternMethodNames");

    // These functions only for LINEAR and RADIAL patterns:
    //
    map("add_color_stop_rgb", Pattern::add_color_stop_rgb,
        GRANT_LINEAR | GRANT_RADIAL);
    map("add_color_stop_rgba", Pattern::add_color_stop_rgba,
        GRANT_LINEAR | GRANT_RADIAL);
    map("get_color_stop_rgba", Pattern::get_color_stop_rgba,
        GRANT_LINEAR | GRANT_RADIAL);

    // Does not do anything for solid (but let it be in):
    //
    map("set_extend", Pattern::set_extend);
    map("set_filter", Pattern::set_filter);

    // All patterns:
    //
    map("set_matrix", Pattern::set_matrix);

    // ...
  }
};
static struct PatternMethodNames pattern_method_names;

/*
 * ...= __index( pattern_ud, key_any )
 */
int Pattern_bind::index(lua_State *L) {
  Pattern &my = *Pattern::instance(L, 1);

  L_GROW(2);

  const char *s = lua_tostring(L, 2);
  if (s) {
    // Data members
    //
    if (strcmp(s, "info") == 0) {
      lua_newtable(L);

      //---
      // .argb (argb_str)   (only if the pattern is a solid color pattern)
      {
        double r, g, b, a;
        cairo_status_t st = cairo_pattern_get_rgba(my, &r, &g, &b, &a);
        if (st == 0) {
          lua_pushliteral(L, "rgba");
          Color(r, g, b, a).push(L);
          lua_settable(L, -3);
        }
      }

      //---
      // .linear_points   ({ x0_num,y0_num, x1_num,y1_num })
      {
        double arr[4];
        cairo_status_t st =
            cairo_pattern_get_linear_points(my, arr, arr + 1, arr + 2, arr + 3);
        if (st == 0) {
          lua_pushliteral(L, "linear_points");
          lua_newtable(L);
          for (unsigned i = 0; i < 4; i++) {
            lua_pushinteger(L, i + 1);
            lua_pushnumber(L, arr[i]);
            lua_settable(L, -3);
          }
          lua_settable(L, -3);
        }
      }

      //---
      // .radial_circles   ({ x0_num,y0_num,r0_num, x1_num,y1_num,r1_num })
      {
        double arr[6];
        cairo_status_t st = cairo_pattern_get_radial_circles(
            my, arr, arr + 1, arr + 2, arr + 3, arr + 4, arr + 5);
        if (st == 0) {
          lua_pushliteral(L, "radial_circles");
          lua_newtable(L);
          for (unsigned i = 0; i < 6; i++) {
            lua_pushinteger(L, i + 1);
            lua_pushnumber(L, arr[i]);
            lua_settable(L, -3);
          }
          lua_settable(L, -3);
        }
      }

      //---
      // .extend      ("none"|"repeat"|"reflect"|"pad")
      //
      lua_pushliteral(L, "extend");
      lua_pushstring(L, conv_extend(cairo_pattern_get_extend(my)));
      lua_settable(L, -3);

      //---
      // .filter      (string)
      //
      lua_pushliteral(L, "filter");
      lua_pushstring(L, conv_filter(cairo_pattern_get_filter(my)));
      lua_settable(L, -3);

      //---
      // .matrix      (matrix)
      {
        lua_pushliteral(L, "matrix");
        cairo_pattern_get_matrix(my, *new (L)
                                         Matrix()); // push matrix, then fill it
        lua_settable(L, -3);
      }

      // .type    (string)
      //
      lua_pushliteral(L, "type");
      lua_pushstring(L, conv_pattern_type(cairo_pattern_get_type(my)));
      lua_settable(L, -3);

      //---
      //...

      status_check_ok(L, cairo_pattern_status(my));
      return 1;
    }

    // Function members
    //
    lua_CFunction f = pattern_method_names(s, get_grant(my));
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
void Pattern_bind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Add our own methods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}

/*
 */
Pattern::Pattern(cairo_pattern_t *cp_) : cp(cp_) { INVARIANT(); }

/*
 */
Pattern::~Pattern() {
  INVARIANT();
  cairo_pattern_destroy(cp);
}

/*
 * pat= pat.add_color_stop_rgb( offset_num, r_num,g_num,b_num )
 * pat= pat.add_color_stop_rgb( offset_num, rgb_str )
 */
int Pattern::add_color_stop_rgb(lua_State *L) {
  proto(L, lua_type(L, 2) == LUA_TNUMBER ? "number,number,number,number"
                                         : "number,string");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));

  double offset = lua_tonumber(L, 1);
  Color col(L, 2);

  cairo_pattern_add_color_stop_rgb(my, offset, col.r, col.g, col.b);

  // "If the pattern is not a gradient pattern, (eg. a linear or radial
  // pattern), then the pattern will be put into an error status with a status
  // of CAIRO_STATUS_PATTERN_TYPE_MISMATCH."
  //
  status_check(L, cairo_pattern_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chaining
  return 1;
}

/*
 * pat= pat.add_color_stop_rgba( offset_num, r_num,g_num,b_num, a_num )
 * pat= pat.add_color_stop_rgba( offset_num, rgb_str, a_num )
 */
int Pattern::add_color_stop_rgba(lua_State *L) {
  proto(L, lua_type(L, 2) == LUA_TNUMBER ? "number,number,number,number,number"
                                         : "number,string,number");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));

  double offset = lua_tonumber(L, 1);
  Color col(L, 2, true);

  cairo_pattern_add_color_stop_rgba(my, offset, col.r, col.g, col.b, col.a);

  // "If the pattern is not a gradient pattern, (eg. a linear or radial
  // pattern), then the pattern will be put into an error status with a status
  // of CAIRO_STATUS_PATTERN_TYPE_MISMATCH."
  //
  status_check(L, cairo_pattern_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chaining
  return 1;
}

/*
 * [offset_num, r_num, g_num, b_num, a_num]= get_color_stop_rgba( index_uint )
 *
 * Returns:
 *       color stop values for 'index' (1..n), or nothing if there is no such
 *       index being used.
 *
 * NOTE: Indexing is 1-based (like in Lua) whereas Cairo C API has 0-based
 *       indexing. This does not matter so much, since we're anyways doing
 *       things differently (i.e. not needing
 * 'cairo_pattern_get_color_stop_count()').
 */
int Pattern::get_color_stop_rgba(lua_State *L) {
  proto(L, "uint");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));

  unsigned index = lua_tointeger(L, 1);

  if (index == 0) {
    luaL_error(L, "Start indexing with 1");
  }

  double offset, r, g, b, a;

  cairo_status_t st =
      cairo_pattern_get_color_stop_rgba(my, index - 1, &offset, &r, &g, &b, &a);
  //
  // CAIRO_STATUS_INVALID_INDEX
  // CAIRO_STATUS_PATTERN_TYPE_MISMATCH

  if (st == CAIRO_STATUS_INVALID_INDEX) {
    return 0; // no such index (push nothing)
  } else {
    status_check(L, st);
  }

  lua_pushnumber(L, offset);
  lua_pushnumber(L, r);
  lua_pushnumber(L, g);
  lua_pushnumber(L, b);
  lua_pushnumber(L, a);
  return 5;
}

/*
 * pat= pat.set_extend( str )
 */
int Pattern::set_extend(lua_State *L) {
  proto(L, "string");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));

  cairo_pattern_set_extend(my, conv_extend(L, 1));
  status_check_ok(L, cairo_pattern_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chaining
  return 1;
}

/*
 * pat= pat.set_filter( str )
 */
int Pattern::set_filter(lua_State *L) {
  proto(L, "string");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));

  cairo_pattern_set_filter(my, conv_filter(L, 1));
  status_check_ok(L, cairo_pattern_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chaining
  return 1;
}

/*
 * pat= pat.set_matrix( matrix )
 */
int Pattern::set_matrix(lua_State *L) {
  proto(L, "CairoMatrix");
  Pattern &my = *Pattern::instance(L, lua_upvalueindex(1));
  Matrix *matrix = Matrix::instance(L, 1);
  if (!matrix) {
    luaL_error(L, "Wanted Matrix, got %s", L_typename(1));
  }

  cairo_pattern_set_matrix(my, *matrix);
  status_check_ok(L, cairo_pattern_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chaining
  return 1;
}
