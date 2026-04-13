/*
 * SURFACE.CPP                               Copyright 2010, Ilmatieteen laitos
 *
 * Ref: <http://www.cairographics.org/manual/cairo-image-surface.html>
 */
#include "Invariant.h"
#include "LuaNew.h"
#include "Proto.h"

#include "Context.hpp"
#include "Surface.hpp"

#include "Converter.hpp"

#include <cairo.h>
#include <string.h>

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

using namespace std;

LuaNew_ID Surface_bind::ID;

/*---=== Helpers ===---
 *
 */
static const unsigned GRANT_IMAGE = 0x01;
static const unsigned GRANT_PDF = 0x02;
static const unsigned GRANT_SVG = 0x04;

static unsigned get_grant(/*const*/ cairo_surface_t *surf) {
  cairo_surface_type_t st = cairo_surface_get_type(surf);
  switch (st) {
  case CAIRO_SURFACE_TYPE_IMAGE:
    return GRANT_IMAGE;
  case CAIRO_SURFACE_TYPE_PDF:
    return GRANT_PDF;
  case CAIRO_SURFACE_TYPE_SVG:
    return GRANT_SVG;
  default:
    break;
  }
  ostringstream os;
  os << "Internal error: unexpected surface type " << (int)st;
  throw runtime_error(os.str().c_str());
}

struct SurfaceTypeConverter : public Converter<cairo_surface_type_t> {
  SurfaceTypeConverter() : Converter<cairo_surface_type_t>("surface_type") {
#ifdef CAIRO_HAS_IMAGE_SURFACE
    map(CAIRO_SURFACE_TYPE_IMAGE, "image");
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
    map(CAIRO_SURFACE_TYPE_PDF, "pdf");
#endif
    // map(CAIRO_SURFACE_TYPE_PS, "ps" );
    // map(CAIRO_SURFACE_TYPE_XLIB, "xlib" );
    // map(CAIRO_SURFACE_TYPE_XCB, "xcb" );
    // map(CAIRO_SURFACE_TYPE_GLITZ, "glitz" );
    // map(CAIRO_SURFACE_TYPE_QUARTZ, "quartz" );
    // map(CAIRO_SURFACE_TYPE_WIN32, "win32" );
    // map(CAIRO_SURFACE_TYPE_BEOS, "beos" );
    // map(CAIRO_SURFACE_TYPE_DIRECTFB, "directfb" );
#ifdef CAIRO_HAS_SVG_SURFACE
    map(CAIRO_SURFACE_TYPE_SVG, "svg");
#endif
    // map(CAIRO_SURFACE_TYPE_OS2, "os2" );
    // map(CAIRO_SURFACE_TYPE_WIN32_PRINTING, "printing" );
    // map(CAIRO_SURFACE_TYPE_QUARTZ_IMAGE, "quartz image" );
  }
};
static SurfaceTypeConverter conv_surface_type;

//---
FormatConverter::FormatConverter() : Converter<cairo_format_t>("format") {
  map(CAIRO_FORMAT_ARGB32, "argb32");
  map(CAIRO_FORMAT_RGB24, "rgb24");
  map(CAIRO_FORMAT_A8, "a8");
  map(CAIRO_FORMAT_A1, "a1");
  // map( CAIRO_FORMAT_RGB16_565, "rgb16_565" );
};
FormatConverter conv_image_format;

/*---=== Surface ===---
 *
 * This is the base class for actual surfaces s.a. 'ImageSurface' (bitmaps),
 * 'PdfSurface' and others.
 *
 * The surfaces don't really have common properties or methods, but by doing
 * this we can allow for object to any of them to be used via
 * 'Surface::instance' as a generic surface.
 */

/*
 * Map method names -> functions
 */
struct SurfaceMethodNames : public MethodNames {
  SurfaceMethodNames() {
    static volatile unsigned initialized; // = 0
    unsigned was = initialized;
    initialized = initialized + 1;
    if (was)
      throw runtime_error("There should be only one SurfaceMethodNames");

    // Methods for all surfaces
    //
    map("finish", Surface::finish);
    map("get_font_options", Surface::get_font_options);
    map("contents", Surface::contents);

    // Methods for Image surface only
    //
#ifndef SANDBOX
    map("write_to_png", ImageSurface::write_to_png, GRANT_IMAGE);
#endif

    // Methods for PDF and SVG
    //
    map("copy_page", PdfSurface::copy_page, GRANT_PDF | GRANT_SVG);
    map("show_page", PdfSurface::show_page, GRANT_PDF | GRANT_SVG);

    // Methods for PDF only
    //
    map("set_size", PdfSurface::set_size, GRANT_PDF);
  }
};
static struct SurfaceMethodNames surface_method_names;

/*
 * any|nil= __index( surf_ud, key )
 */
int Surface_bind::index(lua_State *L) {
  // const Surface &my= *Surface::instance(L,1);

  L_GROW(2);

  const char *s = lua_tostring(L, 2);
  if (s) {
    Surface &my = *Surface::instance(L, 1);

    // Data members
    //
    if (strcmp(s, "info") == 0) {
      cairo_content_t e = cairo_surface_get_content(my);
      //
      // CAIRO_CONTENT_COLOR
      // CAIRO_CONTENT_ALPHA
      // CAIRO_CONTENT_COLOR_ALPHA

      bool content_color = (e != CAIRO_CONTENT_ALPHA);
      bool content_alpha = (e != CAIRO_CONTENT_COLOR);

      lua_newtable(L);
      lua_pushliteral(L, "content_color");
      lua_pushboolean(L, content_color);
      lua_settable(L, -3);

      lua_pushliteral(L, "content_alpha");
      lua_pushboolean(L, content_alpha);
      lua_settable(L, -3);

      //--
      lua_pushliteral(L, "type");
      lua_pushstring(L, conv_surface_type(cairo_surface_get_type(my)));
      lua_settable(L, -3);

      // Features of >= 1.8.0
      //
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 8, 0)
      lua_pushliteral(L, "has_show_text_glyphs");
      lua_pushboolean(L, cairo_surface_has_show_text_glyphs(my));
      lua_settable(L, -3);
#endif
      return 1;
    }

    if (strcmp(s, "context") == 0) {
      new (L) Context(my);
      return 1;
    }

    // Data members ('ImageSurface')
    //
    if (cairo_surface_get_type(my) == CAIRO_SURFACE_TYPE_IMAGE) {
      if (strcmp(s, "width") == 0) {
        lua_pushinteger(L, cairo_image_surface_get_width(my));
        return 1;
      }
      if (strcmp(s, "height") == 0) {
        lua_pushinteger(L, cairo_image_surface_get_height(my));
        return 1;
      }
      if (strcmp(s, "format") == 0) {
        lua_pushstring(L,
                       conv_image_format(cairo_image_surface_get_format(my)));
        return 1;
      }
    }

    // Function members (for all surfaces)
    //
    lua_CFunction f = surface_method_names(s, get_grant(my));
    if (f) {
      lua_pushvalue(L, 1); // copy of the 'Surface' instance
      lua_pushcclosure(L, f, 1);
      return 1;
    }
  }

  luaL_error(L, "No such property: %s", s);
  return 0; // never
}

/*
 */
void Surface_bind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Add our own methods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}

/*---=== Surface ===---*/

/*
 * It's very good we open the file already in constructor; get error messages
 * early and be consistant with how the user thinks things go. 'ImageSurface'
 * will write to it only at GC, whereas PDF and SVG surfaces will write through-
 * out the drawing.
 */
#ifndef SANDBOX
Surface::Surface(cairo_surface_t *surf_, const char *fn)
    : surf(surf_), finished(false), io()
#else
Surface::Surface(cairo_surface_t *surf_)
    : surf(surf_), finished(false), io(new stringstream())
#endif
{
  if (surf_) {
    cairo_status_t st = cairo_surface_status(surf_);
    if (st) {
      ostringstream os;
      os << "Cairo surface at error in initialization: %d"
         << cairo_status_to_string(st);
      throw runtime_error(os.str().c_str());
    }
  }

  // Open file output if filename given.
  //
#ifndef SANDBOX
  if (fn) {
    io = new fstream(fn, ios::in | ios::out | ios::binary | ios::trunc);
    if (!*io) {
      ostringstream os;
      os << "Unable to open: " << fn;
      throw runtime_error(os.str().c_str());
    }
  } else {
    io = new stringstream();
  }
#endif
}

Surface::~Surface() {
  INVARIANT();

  // This may still cause writing to 'io'
  //
  cairo_surface_destroy(surf);

  delete io;
}

/*
 * void= surf.finish()
 *
 * Used for explicitly closing a surface. This will write contents on disk (if
 * not SANDBOX and filename given) and mark any drawing contexts to give
 * CAIRO_STATUS_SURFACE_FINISHED error if continued to be used.
 *
 * Use this for making sure a surface has reaced the disk (otherwise it will
 * happen at GC, in an unknown time in the future - latest at Lua state
 * cleanup).
 */
int Surface::finish(lua_State *L) {
  proto(L, "");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));
  if (!my.finished) {
    cairo_surface_flush(my);
    my.image_to_out(); // copy internal bitmap to 'io' (nothing for PDF, SVG)

    // Drawing contexts won't work any more after this call
    //
    cairo_surface_finish(my);

    my.finished = true; // don't do this again
  }
  return 0;
}

/*
 * font_options_tbl= surf.get_font_options()
 */
int Surface::get_font_options(lua_State *L) {
  proto(L, "");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));
  (void)my;

  luaL_error(L, "TBD: Providing font options out"); // use a table, or userdata?
  return 0;
}

/*
 * data_str, mime_str= surf.contents()
 */
int Surface::contents(lua_State *L) {
  proto(L, "");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));

  L_GROW(2);

  cairo_surface_type_t st = cairo_surface_get_type(my);
  const char *mime = (st == CAIRO_SURFACE_TYPE_IMAGE) ? "image/png" :
#ifdef CAIRO_HAS_PDF_SURFACE
                     (st == CAIRO_SURFACE_TYPE_PDF) ? "application/pdf"
                     :
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
                     (st == CAIRO_SURFACE_TYPE_SVG) ? "image/svg+xml"
                                                    :
#endif
                                                    nullptr;
  if (!mime) {
    luaL_error(L, "Internal error: unknown surface type");
  }

  // Copy internal image buffer to output stream (only image surface
  // does anything with this).
  //
  if (!my.finished) {
    my.image_to_out();

    // Note: SVG and PDF output require us to completely finish the drawing,
    //      before data is written to the stream ('cairo_surface_flush' is not
    //      enough). At least so it seems. This means there can be no further
    //      drawing after 'contents()' has been called. (Cairo 1.6.4)
    //
    //      For 'ImageSurface', just calling 'image_to_out()' would be
    //      sufficient.
    //
    cairo_surface_finish(my);
    my.finished = true;
  }

  my.io->seekg(0, ios::end);
  size_t len = my.io->tellg();

#if 1
  cerr << "Picture size: " << len << endl;
#endif

  // Get the whole contents in one go (files should be within reasonable size,
  // compared to available RAM)
  //
  {
    char *buf = new char[len];
    my.io->seekg(0, ios::beg);
    my.io->read(buf, len);

    lua_pushlstring(L, buf, len);
    delete[] buf;
  }

  lua_pushstring(L, mime);
  return 2;
}

/*
 * surf= pdf|svg_surf.copy_page()
 *
 * Note: NOT available for image surface.
 */
int Surface::copy_page(lua_State *L) {
  proto(L, "");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));

  cairo_surface_copy_page(my);
  status_check_ok(L, cairo_surface_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chain
  return 1;
}

/*
 * surf= pdf|svg_surf.show_page()
 *
 * Note: NOT available for image surface.
 */
int Surface::show_page(lua_State *L) {
  proto(L, "");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));

  cairo_surface_show_page(my);
  status_check_ok(L, cairo_surface_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chain
  return 1;
}

/*
 * surf= pdf_surf.set_size( w_points_num, h_points_num )
 *
 * Note: ONLY available for PDF service (by the ban level).
 */
int Surface::set_size(lua_State *L) {
  proto(L, "number,number");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));
  lua_Number w = lua_tonumber(L, 1);
  lua_Number h = lua_tonumber(L, 2);

  cairo_pdf_surface_set_size(my, w, h);
  status_check_ok(L, cairo_surface_status(my));

  lua_pushvalue(L, lua_upvalueindex(1)); // chain
  return 1;
}

/*
 * Collection callback for taking in data into 'fs' or 'ss' stream.
 */
cairo_status_t Surface::collect_f(void *v_this, const unsigned char *data,
                                  unsigned int len) {
  Surface &my = *static_cast<Surface *>(v_this);

  // We do need 'reinterpret_cast' here, see
  // <http://stackoverflow.com/questions/658913/c-style-cast-from-unsigned-char-to-const-char>
  //
  my.io->write(reinterpret_cast<const char *>(data), len);

  return CAIRO_STATUS_SUCCESS;
}

/*---=== ImageSurface ===---*/

/*
 */
ImageSurface::~ImageSurface() {
  if (!finished) {
    image_to_out();
  }
}

/*
 * Write the surface to a PNG stream (either memory stream or output file).
 */
/*virtual*/ void ImageSurface::image_to_out() /*const*/ {
  assert(!finished);

  cairo_status_t st =
      cairo_surface_write_to_png_stream(*this, collect_f, (void *)this);
  //
  // CAIRO_STATUS_SUCCESS (0)     ok
  // CAIRO_STATUS_NO_MEMORY       out of memory
  // (CAIRO_STATUS_SURFACE_TYPE_MISMATCH cannot happen)
  // CAIRO_STATUS_WRITE_ERROR     error while writing (if returned by
  // 'collect_f')

  if (st) {
    ostringstream os;
    os << "Cairo error writing to image stream: " << cairo_status_to_string(st);
    throw runtime_error(os.str().c_str());
  }
}

/*
 * void= image_surf.write_to_png( fn_str )
 *
 * Note: ONLY available for image service (by the ban level).
 */
#ifndef SANDBOX
int ImageSurface::write_to_png(lua_State *L) {
  proto(L, "string");
  Surface &my = *Surface::instance(L, lua_upvalueindex(1));
  const char *fn = lua_tostring(L, 1);

  cairo_status_t st = cairo_surface_write_to_png(my, fn);
  //
  // CAIRO_STATUS_SUCCESS (0)     ok
  // CAIRO_STATUS_NO_MEMORY       out of memory
  // (CAIRO_STATUS_SURFACE_TYPE_MISMATCH cannot happen)
  // CAIRO_STATUS_WRITE_ERROR     error while writing (if returned by
  // 'collect_f')

  if (st) {
    luaL_error(L, "Cairo error writing to '%s': %s", fn,
               cairo_status_to_string(st));
  }

  return 0;
}
#endif

/*---=== PdfSurface ===---*/

#ifdef CAIRO_HAS_PDF_SURFACE

/*
 * Note: We must have a valid 'io' stream right from the moment we create the
 * 'cairo_surface_t'. Otherwise its header data will be written into a
 * non-initialized stream. In other words, delay creation of the
 * 'cairo_surface_t' until the body of the constructor.
 */
#ifndef SANDBOX
PdfSurface::PdfSurface(double w_points, double h_points, const char *fn)
    : Surface(0 /*coming soon*/, fn)
#else
PdfSurface::PdfSurface(double w_points, double h_points)
    : Surface(0 /*coming soon*/)
#endif
{
  // Now 'Surface' and its 'io' stream are initialized; we can provide 'this'
  // (or just 'io', does not matter) to the stream.
  //
  surf = cairo_pdf_surface_create_for_stream(collect_f, (void *)this, w_points,
                                             h_points);
  cairo_status_t st = cairo_surface_status(surf);
  if (st) {
    throw runtime_error(cairo_status_to_string(st));
  }

  INVARIANT();
}
#endif
// CAIRO_HAS_PDF_SURFACE

/*---=== SvgSurface ===---
 *
 * NOTE: The implementation is almost 1:1 with 'PdfSurface' but we've still
 * chosen not to make these two from an intermediate common class. They are
 * separate classes; they are just very, very alike (but not quite).
 */
#ifdef CAIRO_HAS_SVG_SURFACE

/*
 * 'ver_required' is CAIRO_SVG_VERSION_1_1 | CAIRO_SVG_VERSION_1_2 | (-1:
 * default version)
 */
#ifndef SANDBOX
SvgSurface::SvgSurface(double w_points, double h_points,
                       cairo_svg_version_t ver_required, const char *fn)
    : Surface(0 /*coming soon*/, fn)
#else
SvgSurface::SvgSurface(double w_points, double h_points,
                       cairo_svg_version_t ver_required)
    : Surface(0 /*coming soon*/)
#endif
{
  surf = cairo_svg_surface_create_for_stream(collect_f, (void *)this, w_points,
                                             h_points);

  if (((int)ver_required) >= 0) {
    cairo_svg_surface_restrict_to_version(surf, ver_required);
  }

  cairo_status_t st = cairo_surface_status(surf);
  if (st) {
    throw runtime_error(cairo_status_to_string(st));
  }

  INVARIANT();
}

#endif
// CAIRO_HAS_SVG_SURFACE
