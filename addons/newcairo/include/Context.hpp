/*
* CONTEXT.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== Context ===---
*/
class Context;

struct Context_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo context"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef Context CAST_T;

  private:
    static int index( lua_State *L );
};

/*
* All Cairo drawing happens via the context, which points to a particular surface.
*/
class Context : public LuaNew<Context_bind> {
  public:
    operator cairo_t *() { return cr; }

    Context( cairo_surface_t *cs );
	~Context();

    static int save( lua_State *L );
    static int restore( lua_State *L );
    static int push_group( lua_State *L );
    static int pop_group_to_source( lua_State *L );
    static int set_source_rgb( lua_State *L );
    static int set_source_rgba( lua_State *L );
    static int set_source( lua_State *L );
    static int set_source_surface( lua_State *L );
    static int get_source( lua_State *L );
    static int get_target( lua_State *L );
    static int set_antialias( lua_State *L );
    static int set_dash( lua_State *L );
    static int get_dash( lua_State *L );
    static int set_fill_rule( lua_State *L );
    static int set_line_cap( lua_State *L );
    static int set_line_join( lua_State *L );
    static int set_line_width( lua_State *L );
    static int set_miter_limit( lua_State *L );
    static int set_operator( lua_State *L );
    static int set_tolerance( lua_State *L );
    static int clip( lua_State *L );
    static int clip_preserve( lua_State *L );
    static int reset_clip( lua_State *L );
    static int clip_extents( lua_State *L );
    static int fill( lua_State *L );
    static int fill_preserve( lua_State *L );
    static int fill_extents( lua_State *L );
    static int in_fill( lua_State *L );
    static int mask( lua_State *L );
    static int mask_surface( lua_State *L );
    static int paint( lua_State *L );
    static int paint_with_alpha( lua_State *L );
    static int stroke( lua_State *L );
    static int stroke_preserve( lua_State *L );
    static int stroke_extents( lua_State *L );
    static int in_stroke( lua_State *L );
    static int copy_page( lua_State *L );
    static int show_page( lua_State *L );
    //
    static int translate( lua_State *L );
    static int scale( lua_State *L );
    static int rotate( lua_State *L );
    static int transform( lua_State *L );
    static int set_matrix( lua_State *L );
    static int get_matrix( lua_State *L );
    static int identity_matrix( lua_State *L );
    static int user_to_device( lua_State *L );
    static int user_to_device_distance( lua_State *L );
    static int device_to_user( lua_State *L );
    static int device_to_user_distance( lua_State *L );
    //
    static int copy_path( lua_State *L );
    static int copy_path_flat( lua_State *L );
    static int append_path( lua_State *L );
    static int has_current_point( lua_State *L );
    static int get_current_point( lua_State *L );
    static int new_path( lua_State *L );
    static int new_sub_path( lua_State *L );
    static int close_path( lua_State *L );
    static int arc( lua_State *L );
    static int arc_negative( lua_State *L );
    static int circle( lua_State *L );
    static int curve_to( lua_State *L );
    static int line_to( lua_State *L );
    static int move_to( lua_State *L );
    static int rectangle( lua_State *L );
    static int glyph_path( lua_State *L );
    static int text_path( lua_State *L );
    static int rel_curve_to( lua_State *L );
    static int rel_line_to( lua_State *L );
    static int rel_move_to( lua_State *L );
    static int path_extents( lua_State *L );
    //
    static int select_font_face( lua_State *L );
    static int set_font_size( lua_State *L );
    static int set_font_matrix( lua_State *L );
    static int get_font_matrix( lua_State *L );
    static int set_font_options( lua_State *L );
    static int get_font_options( lua_State *L );
    static int set_font_face( lua_State *L );
    static int get_font_face( lua_State *L );
    static int set_scaled_font( lua_State *L );
    static int get_scaled_font( lua_State *L );
    static int show_text( lua_State *L );
    static int show_glyphs( lua_State *L );
    static int font_extents( lua_State *L );
    static int text_extents( lua_State *L );

  private:
    // data members
    //
    cairo_t *cr;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( cr != 0 );
    }
#endif 
};

#endif
    // CONTEXT_HPP
    
