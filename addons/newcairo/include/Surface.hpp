/*
* SURFACE.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*
* Define:
*   SANDBOX      to disable direct filesystem access
*/
#ifndef SURFACE_HPP
#define SURFACE_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"
#include "Converter.hpp"

#include <cairo.h>

#include <ostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#ifdef CAIRO_HAS_PDF_SURFACE
# include <cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
# include <cairo-svg.h>
#endif

// Cairo 1.6.x has no CAIRO_HAS_IMAGE_SURFACE but the functions are always there
// Cairo 1.8.x has CAIRO_HAS_IMAGE_SURFACE and it's always enabled
//
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 8, 0)
# define CAIRO_HAS_IMAGE_SURFACE
#endif

struct FormatConverter : public Converter<cairo_format_t> {
    FormatConverter();
};
extern FormatConverter conv_image_format;


/*---=== Surface ===---
*
* Base class for various surfaces and the one bound to Lua. 
* Cannot be instantiated as such (= virtual base class).
*/
class Surface;

struct Surface_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo surface"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef Surface CAST_T;

  private:
    static int index( lua_State *L );
};

/*
*/
class Surface : public LuaNew<Surface_bind> {
  public:
    operator cairo_surface_t *() { return surf; }

    static int finish( lua_State *L );
    static int get_font_options( lua_State *L );
    static int contents( lua_State *L );
    static int copy_page( lua_State *L );   // PDF, SVG only
    static int show_page( lua_State *L );   // PDF, SVG only
    static int set_size( lua_State *L );    // PDF only

    static cairo_status_t collect_f( void *v_io, const unsigned char *data, unsigned int len );

#ifndef SANDBOX
    Surface( cairo_surface_t *surf_, const char *fn );
#else
    Surface( cairo_surface_t *surf_ );
#endif
	~Surface();

  protected:

    // Called to update 'out' from internal Cairo bitmap (either at 'finish' or
    // 'push_contents()').
    //
    virtual void image_to_out() /*const*/ {}     // by default do nothing (vectors)

  protected:
    // data members
    //
    cairo_surface_t *surf;
    bool finished;
    
    std::iostream *io;  // either 'fstream' (writing to file) or 'stringstream' (memory buffer)

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        // Normally 'surf' is non-nullptr but 'PdfSurface' and 'SvgSurface' constructors 
        // must have it initially nullptr, until they set it themselves.
        
        assert_invariant(io);
    }
#endif 
};


/*---=== ImageSurface ===---
*
* Memory buffer for an image
* 
* In non-SANDBOX mode, optionally output to a named PNG file at destructor.
*/
#ifdef CAIRO_HAS_IMAGE_SURFACE
# if (!defined CAIRO_HAS_PNG_FUNCTIONS)
#  error "Must have PNG support"
# endif

class ImageSurface : public Surface {
  public:
# ifndef SANDBOX
    ImageSurface( cairo_surface_t *surf_, const char *fn_=0 ) : Surface(surf_,fn_) { INVARIANT(); }
# else
    ImageSurface( cairo_surface_t *surf_ ) : Surface(surf_) { INVARIANT(); }
# endif
    ~ImageSurface();

#ifndef SANDBOX
    static int write_to_png( lua_State *L );
#endif

    /*virtual*/ void image_to_out() /*const*/;

  private:
# ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) {
    }
# endif 
};
#endif


/*---=== PdfSurface ===---
*/
#ifdef CAIRO_HAS_PDF_SURFACE

class PdfSurface : public Surface {
  public:
# ifndef SANDBOX
    PdfSurface( double w_points, double h_points, const char *fn );
# else
    PdfSurface( double w_points, double h_points );
# endif
    ~PdfSurface() {}
    
  private:
# ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) {
    }
# endif 
};
#endif


/*---=== SvgSurface ===---
*/
#ifdef CAIRO_HAS_SVG_SURFACE

class SvgSurface : public Surface {
  public:
# ifndef SANDBOX
    SvgSurface( double w_points, double h_points, cairo_svg_version_t ver_, const char *fn );
# else
    SvgSurface( double w_points, double h_points, cairo_svg_version_t ver_ );
# endif
    ~SvgSurface() {}

  private:
# ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) {
    }
# endif 
};
#endif

#endif
    // SURFACE_HPP
    
