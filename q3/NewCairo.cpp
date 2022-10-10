/*
 * LUA51-NEWCAIRO.CPP                       Copyright 2010, Ilmatieteen laitos
 *
 * Top level bindings (not the objects) for 'newcairo.lua'.
 *
 * Define:
 *   SANDBOX to compile without direct access to local filesystem (usable i.e.
 * on servers).
 *
 * Todo:
 *   We could make error handling uniform, and maybe use
 * 'cairo_status_to_string()' to get the strings. Would there be any help from
 * doing that?
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include <cairo.h>
#include <string.h>

#include <unistd.h>
// mkstemp()

using namespace std;

// Tested on 1.8.8 (OS X) and 1.6.4 (Linux Centos)
//
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 6, 4)
#warning                                                                       \
    "Not guaranteed to work on < 1.6.4 - please upgrade (1.8.8 is the most recent)"
#endif

#if (defined __WIN32__) || (defined __WIN64__)
#define WIN32_DLLEXPORT __declspec(dllexport)
#else
#define WIN32_DLLEXPORT
#endif

// Newcairo objects
//
#include "Context.hpp"
#include "FontFace.hpp"
#include "Glyph.hpp"
#include "Matrix.hpp"
#include "Path.hpp"
#include "Pattern.hpp"
#include "ScaledFont.hpp"
#include "Surface.hpp"

#include "Converter.hpp"

// Lua side of binding precompiled
//
static unsigned char newcairo_chunk[] =
#include "newcairo.lch"

#include "Color.hpp"

    /*
     * NOTE: Who and when should do the 'FT_Library' initialization is not
     * clear. We need it for loading TrueType fonts (from a file) and just one
     * (at least *per thread*) such is enough.
     *
     *       We should clean this up by 'FT_Done_FreeType()' but currently, we
     * don't.
     *       --AKa 8-Sep-10
     */
    static FT_Library ft_lib;

/*---=== Helpers ===---
 */

#ifdef CAIRO_HAS_SVG_SURFACE
struct SvgVersionConverter : public Converter<cairo_svg_version_t> {
  SvgVersionConverter() : Converter<cairo_svg_version_t>("svg_version") {
    map(CAIRO_SVG_VERSION_1_1, "1.1");
    map(CAIRO_SVG_VERSION_1_2, "1.2");
  }
};
static SvgVersionConverter conv_svg_version;
#endif

/*---=== Top-level functions ===---*/

/*
 * image_surf= image_surface( w_pixels_num, h_pixels_num, [format_string],
 * [filename_str] )  (full) image_surf= image_surface( w_pixels_num,
 * h_pixels_num, [format_string] )                  (SANDBOX)
 *
 * format:   "argb32" (default) | "rgb24" | "a8" | "a1"
 *
 * Note: There should not be any need to use 'format' other than ARGB32. Having
 * the format where it is does not really matter; this is just an internal API
 * (to '.surface()').
 */
#ifdef CAIRO_HAS_IMAGE_SURFACE
static int image_surface(lua_State *L) {
#ifndef SANDBOX
  proto(L, "number,number,[string],[string]");
#else
  proto(L, "number,number,[string]");
#endif
  int w = lua_tointeger(L, 1);
  int h = lua_tointeger(L, 2);
  cairo_format_t format = conv_image_format(L, 3, CAIRO_FORMAT_ARGB32);
#ifndef SANDBOX
  const char *fn = lua_tostring(L, 4);
#endif

  cairo_surface_t *surf = cairo_image_surface_create(format, w, h);
  //
  // Always returns a valid pointer, but it will return a pointer to a "nil"
  // surface if an error such as out of memory occurs. Use
  // 'cairo_surface_status()' to check for this.

  assert(surf);
  cairo_status_t st = cairo_surface_status(surf);
  //
  // CAIRO_STATUS_SUCCESS             no error
  // CAIRO_STATUS_NO_MEMORY           out of memory
  // CAIRO_STATUS_INVALID_FORMAT      invalid format

  switch (st) {
  case CAIRO_STATUS_SUCCESS:
    break;
  case CAIRO_STATUS_NO_MEMORY:
    luaL_error(L, "out of memory");
  case CAIRO_STATUS_INVALID_FORMAT:
  default:
    luaL_error(L, "internal error: %d", (int)st);
  }

  // '~ImageSurface()' will take care of calling 'cairo_surface_destroy()'
  //
#ifndef SANDBOX
  new (L) ImageSurface(surf, fn);
#else
  new (L) ImageSurface(surf);
#endif
  return 1;
}
#endif

/*
 * image_surf= image_surface_from_png( filename_str )
 */
#if (!defined SANDBOX) && (defined CAIRO_HAS_IMAGE_SURFACE) &&                 \
    (defined CAIRO_HAS_PNG_FUNCTIONS)
static int image_surface_from_png(lua_State *L) {
  proto(L, "string");
  const char *fn = lua_tostring(L, 1);

  cairo_surface_t *surf = cairo_image_surface_create_from_png(fn);
  //
  // Always returns a non-nullptr pointer. If there is an error, it points to a
  // 'nil' surface.

  assert(surf);
  cairo_status_t st = cairo_surface_status(surf);
  //
  // CAIRO_STATUS_SUCCESS (0)         no error
  // CAIRO_STATUS_NO_MEMORY           out of memory
  // CAIRO_STATUS_INVALID_FORMAT      invalid format

  if (st)
    switch (st) {
    case CAIRO_STATUS_NO_MEMORY:
      throw out_of_memory();
    case CAIRO_STATUS_INVALID_FORMAT:
      luaL_error(L, "Unknown format for: %s", fn);
    default:
      luaL_error(L, "unexpected error: %d", (int)st);
    }

  // 'ImageSurface()' will take care of calling 'cairo_surface_destroy()'
  //
  new (L) ImageSurface(surf);
  return 1;
}
#endif

/*
 * pdf_surf= pdf_surface( w_points_num, h_points_num, [filename_str] )   -- full
 * pdf_surf= pdf_surface( w_points_num, h_points_num )                   --
 * SANDBOX
 */
#ifdef CAIRO_HAS_PDF_SURFACE
static int pdf_surface(lua_State *L) {
#ifndef SANDBOX
  proto(L, "number,number,[string]");
#else
  proto(L, "number,number");
#endif
  double w = lua_tonumber(L, 1);
  double h = lua_tonumber(L, 2);
#ifndef SANDBOX
  const char *fn = lua_tostring(L, 3);
#endif

  try {
#ifndef SANDBOX
    new (L) PdfSurface(w, h, fn);
#else
    new (L) PdfSurface(w, h);
#endif
  } catch (const exception &exc) {
    LuaNew_base::nuke(L, -1); // the pushed memory never became a valid object
    luaL_error(L, "%s", exc.what());
  }
  return 1;
}
#endif

/*
 * svg_surf= svg_surface( w_points_num, h_points_num, [version_required_str],
 * [filename_str] )   -- full svg_surf= svg_surface( w_points_num, h_points_num,
 * [version_required_str] )                   -- SANDBOX
 */
#ifdef CAIRO_HAS_SVG_SURFACE
static int svg_surface(lua_State *L) {
#ifndef SANDBOX
  proto(L, "number,number,[string],[string]");
#else
  proto(L, "number,number,[string]");
#endif
  lua_Number w = lua_tonumber(L, 1);
  lua_Number h = lua_tonumber(L, 2);
  cairo_svg_version_t v_e =
      conv_svg_version(L, 3, CAIRO_SVG_VERSION_1_2 /*latest*/);
#ifndef SANDBOX
  const char *fn = lua_tostring(L, 4);
#endif

  try {
#ifndef SANDBOX
    new (L) SvgSurface(w, h, v_e, fn);
#else
    new (L) SvgSurface(w, h, v_e);
#endif
  } catch (const exception &exc) {
    LuaNew_base::nuke(L, -1); // the pushed memory never became a valid object
    luaL_error(L, "%s", exc.what());
  }
  return 1;
}
#endif

/*---=== Pattern ===---*/

/*
 * pattern_ud= pattern_rgb( r_num, g_num, b_num )
 * pattern_ud= pattern_rgb( rgb_str )
 */
static int pattern_rgb(lua_State *L) {
  proto(L, lua_type(L, 1) == LUA_TNUMBER ? "number,number,number" : "string");
  Color col(L, 1);

  // 'cairo_pattern_create_rgb()' always provides a non-nullptr pointer.
  // By pushing it to Lua already here, we guarantee getting automatic
  // '.destroy()' even if we were to abandon it.
  //
  Pattern *pat = new (L) Pattern(cairo_pattern_create_rgb(col.r, col.g, col.b));
  status_check(L, cairo_pattern_status(*pat));
  return 1;
}

/*
 * pattern_ud= pattern_rgba( r_num, g_num, b_num, a_num )
 * pattern_ud= pattern_rgba( rgb_str, a_num )
 */
static int pattern_rgba(lua_State *L) {
  proto(L, lua_type(L, 1) == LUA_TNUMBER ? "number,number,number,number"
                                         : "string,number");
  Color col(L, 1, true);

  Pattern *pat =
      new (L) Pattern(cairo_pattern_create_rgba(col.r, col.g, col.b, col.a));
  status_check(L, cairo_pattern_status(*pat));
  return 1;
}

/*
 * pattern_ud= pattern_for_surface( surf_ud )
 */
static int pattern_for_surface(lua_State *L) {
  proto(L, "CairoSurface");
  Surface &surf = Surface::instance_notnull(L, 1); // error if none

  Pattern *pat = new (L) Pattern(cairo_pattern_create_for_surface(surf));
  status_check(L, cairo_pattern_status(*pat));
  return 1;
}

/*
 * pattern_ud= pattern_linear( x0_num, y0_num, x1_num, y1_num )
 */
static int pattern_linear(lua_State *L) {
  proto(L, "number,number,number,number");
  double x0 = lua_tonumber(L, 1);
  double y0 = lua_tonumber(L, 2);
  double x1 = lua_tonumber(L, 3);
  double y1 = lua_tonumber(L, 4);

  Pattern *pat = new (L) Pattern(cairo_pattern_create_linear(x0, y0, x1, y1));
  status_check(L, cairo_pattern_status(*pat));
  return 1;
}

/*
 * pattern_ud= pattern_radial( cx0_num, cy0_num, r0_num, cx1_num, cy1_num,
 * r1_num )
 */
static int pattern_radial(lua_State *L) {
  proto(L, "number,number,number,number,number,number");
  double x0 = lua_tonumber(L, 1);
  double y0 = lua_tonumber(L, 2);
  double r0 = lua_tonumber(L, 3);
  double x1 = lua_tonumber(L, 4);
  double y1 = lua_tonumber(L, 5);
  double r1 = lua_tonumber(L, 6);

  Pattern *pat =
      new (L) Pattern(cairo_pattern_create_radial(x0, y0, r0, x1, y1, r1));
  status_check(L, cairo_pattern_status(*pat));
  return 1;
}

/*---=== Matrix ===---
 *
 * Note: We've reduced the '_init' away from the function call names.
 */

/*
 * matrix_ud= matrix( xx_num, yx_num, xy_num, yy_num, x0_num, y0_num )
 */
static int matrix(lua_State *L) {
  proto(L, "number,number,number,number,number,number");
  double xx = lua_tonumber(L, 1);
  double yx = lua_tonumber(L, 2);
  double xy = lua_tonumber(L, 3);
  double yy = lua_tonumber(L, 4);
  double x0 = lua_tonumber(L, 5);
  double y0 = lua_tonumber(L, 6);

  cairo_matrix_init(*new (L) Matrix(), xx, yx, xy, yy, x0, y0);
  return 1;
}

/*
 * matrix_ud= matrix_identity()
 */
static int matrix_identity(lua_State *L) {
  proto(L, "");
  cairo_matrix_init_identity(*new (L) Matrix());
  return 1;
}

/*
 * matrix_ud= matrix_translate( tx_num, ty_num )
 */
static int matrix_translate(lua_State *L) {
  proto(L, "number,number");
  double tx = lua_tonumber(L, 1);
  double ty = lua_tonumber(L, 2);

  cairo_matrix_init_translate(*new (L) Matrix(), tx, ty);
  return 1;
}

/*
 * matrix_ud= matrix_scale( sx_num, sy_num )
 */
static int matrix_scale(lua_State *L) {
  proto(L, "number,number");
  double sx = lua_tonumber(L, 1);
  double sy = lua_tonumber(L, 2);

  cairo_matrix_init_scale(*new (L) Matrix(), sx, sy);
  return 1;
}

/*
 * matrix_ud= matrix_rotate( rad_num )
 */
static int matrix_rotate(lua_State *L) {
  proto(L, "number");
  double rad = lua_tonumber(L, 1);

  cairo_matrix_init_rotate(*new (L) Matrix(), rad);
  return 1;
}

/*---=== Font face ===---*/

/*
 * font_ud= font( ttf_path_str [,index_uint] )
 */
#ifdef CAIRO_HAS_FT_FONT
static int font(lua_State *L) {
  proto(L, "string,[uint]");

  const char *path = lua_tostring(L, 1);
  unsigned face_index = lua_tointeger(L, 2); // 0 by default

  cairo_font_face_t *ff = FontFace::load_ttf(path, face_index, ft_lib);
  if (!ff) {
    luaL_error(L, "Unable to load %s", path);
  }

  new (L) FontFace(ff);
  return 1;
}
#endif
// CAIRO_HAS_FT_FONT

/*---=== Prototype checks ===---*/

/*
 * bool= is_surface( any )
 */
static int is_surface(lua_State *L) {
  lua_pushboolean(L, Surface::instance(L, 1) != nullptr);
  return 1;
}

/*
 * bool= is_context( any )
 */
static int is_context(lua_State *L) {
  lua_pushboolean(L, Context::instance(L, 1) != nullptr);
  return 1;
}

/*
 * bool= is_pattern( any )
 */
static int is_pattern(lua_State *L) {
  lua_pushboolean(L, Pattern::instance(L, 1) != nullptr);
  return 1;
}

/*
 * bool= is_matrix( any )
 */
static int is_matrix(lua_State *L) {
  lua_pushboolean(L, Matrix::instance(L, 1) != nullptr);
  return 1;
}

/*
 * bool= is_font( any )
 */
static int is_font(lua_State *L) {
  lua_pushboolean(L, FontFace::instance(L, 1) != nullptr);
  return 1;
}

/*---=== Initialization ===---*/

/*
 * Lua addon module entry point
 */
extern "C" int WIN32_DLLEXPORT luaopen_newcairo(lua_State *L) {
  int st;

  //---
  // Push the precompiled Lua level chunk
  //
  st = luaL_loadbuffer(L, (char *)newcairo_chunk, sizeof(newcairo_chunk),
                       nullptr /*from precompiled*/);
  if (st) {
    // Can only be LUA_ERRMEM (the script is precompiled so no syntax errors)
    //
    return 0; // don't return anything
  }

  //---
  // Claim access to FreeType
  //
  // Ref.
  // http://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_Init_FreeType
  //
  // NOTE: We should probably guard against doing this more than once.
  //
  FT_Error ft_err = FT_Init_FreeType(&ft_lib);
  if (ft_err) {
    luaL_error(L, "FT_Init_FreeType error %d", (int)ft_err);
  }

  //---
  // Module table
  //
  lua_newtable(L);

  L_START {
    // Set up classes
    //
    // Note: For derived classes, we need a slightly more elaborate syntax. It's
    // still
    //      essentially the same call.
    //
    Surface::create_mt(L);
    Context::create_mt(L);
    Pattern::create_mt(L);
    Matrix::create_mt(L);
    Path::create_mt(L);
    FontFace::create_mt(L);
    ScaledFont::create_mt(L);
    Glyph::create_mt(L);

    //---
    // Surface binding
    //
#ifdef CAIRO_HAS_IMAGE_SURFACE
    lua_pushcfunction(L, image_surface);
    lua_setfield(L, -2, "image_surface");
#if (!defined SANDBOX) && (defined CAIRO_HAS_PNG_FUNCTIONS)
    lua_pushcfunction(L, image_surface_from_png);
    lua_setfield(L, -2, "image_surface_from_png");
#endif
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
    lua_pushcfunction(L, pdf_surface);
    lua_setfield(L, -2, "pdf_surface");
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
    lua_pushcfunction(L, svg_surface);
    lua_setfield(L, -2, "svg_surface");
#endif

    //---
    // Context binding
    //

    //---
    // Pattern binding
    //
    lua_pushcfunction(L, pattern_rgb);
    lua_setfield(L, -2, "pattern_rgb");

    lua_pushcfunction(L, pattern_rgba);
    lua_setfield(L, -2, "pattern_rgba");

    lua_pushcfunction(L, pattern_for_surface);
    lua_setfield(L, -2, "pattern_for_surface");

    lua_pushcfunction(L, pattern_linear);
    lua_setfield(L, -2, "pattern_linear");

    lua_pushcfunction(L, pattern_radial);
    lua_setfield(L, -2, "pattern_radial");

    //---
    // Matrix binding
    //
    lua_pushcfunction(L, matrix);
    lua_setfield(L, -2, "matrix");

    lua_pushcfunction(L, matrix_identity);
    lua_setfield(L, -2, "matrix_identity");

    lua_pushcfunction(L, matrix_translate);
    lua_setfield(L, -2, "matrix_translate");

    lua_pushcfunction(L, matrix_scale);
    lua_setfield(L, -2, "matrix_scale");

    lua_pushcfunction(L, matrix_rotate);
    lua_setfield(L, -2, "matrix_rotate");

    //---
    // Font binding
    //
#ifdef CAIRO_HAS_FT_FONT
    lua_pushcfunction(L, font);
    lua_setfield(L, -2, "font");
#endif

    //---
    // Misc
    //
    lua_pushstring(L, cairo_version_string()); // x.y.z (of the runtime)
    lua_setfield(L, -2, "VERSION");

    //--
    // Initialize 'proto' system for both us (C++) and Lua.
    // Add our custom types to 'proto.*'
    //
    proto_init(L)
        .set("CairoSurface", is_surface)
        .set("CairoContext", is_context)
        .set("CairoPattern", is_pattern)
        .set("CairoMatrix", is_matrix)
        .set("CairoFont", is_font);
  }
  L_END(0);

  // [1]: newcairo_chunk
  // [2]: C++ level binding table

  lua_pushliteral(L, "newcairo"); // name for our module (matches the name of
                                  // the .so and this function)

#ifndef SANDBOX
  lua_pushboolean(L, true);
#else
  lua_pushnil(L);
#endif
  lua_call(L, 3 /*args*/, 0 /*results*/);

  return 0; // nothing (could return the 'newcairo' table; is it beneficial?)
}
