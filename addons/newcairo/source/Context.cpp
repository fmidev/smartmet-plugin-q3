/*
* CONTEXT.CPP                               Copyright 2010, Ilmatieteen laitos
*
* Ref: <http://www.cairographics.org/manual/cairo-context.html>
*/
#include "LuaNew.h"
#include "Invariant.h"
#include "Proto.h"

#include "Context.hpp"
#include "Pattern.hpp"
#include "Surface.hpp"
#include "Matrix.hpp"
#include "Path.hpp"
#include "FontFace.hpp"
#include "ScaledFont.hpp"

#include "Converter.hpp"

#include <string.h>
#include <stdlib.h>
    // strtol (on Ubuntu 9.10)
#include <math.h>

using namespace std;

LuaNew_ID Context_bind::ID;


/*---=== Converters ===---
*/
struct OperatorConverter : public Converter<cairo_operator_t> {
    OperatorConverter() : Converter<cairo_operator_t>("operator") {
        map( CAIRO_OPERATOR_CLEAR, "clear" );
        map( CAIRO_OPERATOR_SOURCE, "source" );
        map( CAIRO_OPERATOR_OVER, "over" );
        map( CAIRO_OPERATOR_IN, "in" );
        map( CAIRO_OPERATOR_OUT, "out" );
        map( CAIRO_OPERATOR_ATOP, "atop" );
        map( CAIRO_OPERATOR_DEST, "dest" );
        map( CAIRO_OPERATOR_DEST_OVER, "dest_over" );
        map( CAIRO_OPERATOR_DEST_IN, "dest_in" );
        map( CAIRO_OPERATOR_DEST_OUT, "dest_out" );
        map( CAIRO_OPERATOR_DEST_ATOP, "dest_atop" );
        map( CAIRO_OPERATOR_XOR, "xor" );
        map( CAIRO_OPERATOR_ADD, "add" );
        map( CAIRO_OPERATOR_SATURATE, "saturate" );
    }
};
static OperatorConverter conv_operator;

//---
struct FillRuleConverter : public Converter<cairo_fill_rule_t> {
    FillRuleConverter() : Converter<cairo_fill_rule_t>("fill_rule") {
        map( CAIRO_FILL_RULE_WINDING, "winding" );
        map( CAIRO_FILL_RULE_EVEN_ODD, "even_odd" );
    }
};
static FillRuleConverter conv_fill_rule;

//---
struct LineCapConverter : public Converter<cairo_line_cap_t> {
    LineCapConverter() : Converter<cairo_line_cap_t>("line_cap") {
        map( CAIRO_LINE_CAP_BUTT, "butt" );
        map( CAIRO_LINE_CAP_ROUND, "round" );
        map( CAIRO_LINE_CAP_SQUARE, "square" );
    }
};
static LineCapConverter conv_line_cap;

//---
struct LineJoinConverter : public Converter<cairo_line_join_t> {
    LineJoinConverter() : Converter<cairo_line_join_t>("line_join") {
        map( CAIRO_LINE_JOIN_MITER, "miter" );
        map( CAIRO_LINE_JOIN_ROUND, "round" );
        map( CAIRO_LINE_JOIN_BEVEL, "bevel" );
    }
};
static LineJoinConverter conv_line_join;

//---
struct AntialiasConverter : public Converter<cairo_antialias_t> {
    AntialiasConverter() : Converter<cairo_antialias_t>("antialias") {
        map( CAIRO_ANTIALIAS_DEFAULT, "default" );
        map( CAIRO_ANTIALIAS_NONE, "none" );
        map( CAIRO_ANTIALIAS_GRAY, "gray" );
        map( CAIRO_ANTIALIAS_SUBPIXEL, "subpixel" );
    }
};
static AntialiasConverter conv_antialias;

//---
struct FontSlantConverter : public Converter<cairo_font_slant_t> {
    FontSlantConverter() : Converter<cairo_font_slant_t>("font_slant") {
        map( CAIRO_FONT_SLANT_NORMAL, "normal" );
        map( CAIRO_FONT_SLANT_ITALIC, "italic" );
        map( CAIRO_FONT_SLANT_OBLIQUE, "oblique" );
    }
};
static FontSlantConverter conv_font_slant;

//---
struct FontWeightConverter : public Converter<cairo_font_weight_t> {
    FontWeightConverter() : Converter<cairo_font_weight_t>("font_weight") {
        map( CAIRO_FONT_WEIGHT_NORMAL, "normal" );
        map( CAIRO_FONT_WEIGHT_BOLD, "bold" );
    }
};
static FontWeightConverter conv_font_weight;

//---
struct HintStyleConverter : public Converter<cairo_hint_style_t> {
    HintStyleConverter() : Converter<cairo_hint_style_t>("hint_style") {
        map( CAIRO_HINT_STYLE_DEFAULT, "default" );
        map( CAIRO_HINT_STYLE_NONE, "none" );
        map( CAIRO_HINT_STYLE_SLIGHT, "slight" );
        map( CAIRO_HINT_STYLE_MEDIUM, "medium" );
        map( CAIRO_HINT_STYLE_FULL, "full" );
    }
};
static HintStyleConverter conv_hint_style;

//---
struct HintMetricsConverter : public Converter<cairo_hint_metrics_t> {
    HintMetricsConverter() : Converter<cairo_hint_metrics_t>("hint_metrics") {
        map( CAIRO_HINT_METRICS_DEFAULT, "default" );
        map( CAIRO_HINT_METRICS_OFF, "off" );
        map( CAIRO_HINT_METRICS_ON, "on" );
    }
};
static HintMetricsConverter conv_hint_metrics;


/*---=== Helpers ===---
*/

/*
* Parse color strings to rgb values.
*
* i.e. "[#]0f0f00" -> 15/255, 15/255, 0
*
* Note: We could support other names s.a. "red", "teal" etc. here, but 
*       why bother. It's so easy to make variables in one's Lua script for
*       the colors one actually uses (s.a. 'local red="f00").
*/
static void parse_rgba( lua_State *L, int index, double &r, double &g, double &b, double *a_ptr ) {

    if (lua_type(L,1)==LUA_TSTRING) {
        const char *s= lua_tostring(L,index++);
        if (*s=='#') ++s;   // beginning hash is optional
        char *endp;
        long rgb= strtol( s, &endp, 16 );
        if (*endp) {
UNKNOWN_RGB:
            luaL_error( L, "Unknown color: %s", s );
        }

        if (endp-s == 6) {          // i.e. "aabbcc"
            r= ((rgb>>16) & 0xff) / 255.0;
            g= ((rgb>>8) & 0xff) / 255.0;
            b= ((rgb>>0) & 0xff) / 255.0;

        } else if (endp-s == 3) {   // i.e. "abc"
            unsigned rr= (rgb>>16) & 0x0f;
            unsigned gg= (rgb>>8) & 0x0f;
            unsigned bb= (rgb>>0) & 0x0f;

            r= (rr | (rr<<4)) / 255.0;            
            g= (gg | (gg<<4)) / 255.0;            
            b= (bb | (bb<<4)) / 255.0;            
        } else {
            goto UNKNOWN_RGB;
        }
    } else {
        r= lua_tonumber(L,index++);
        g= lua_tonumber(L,index++);
        b= lua_tonumber(L,index++);
    }

    if (a_ptr) {
        *a_ptr= lua_tonumber(L,index);
    }
}


/*---=== Context ===---
*/

/*
* Map method names -> functions
*
* Using C++ 'string' instead of 'const char *' for the key for
* ease of coding (no need to provide comparison method).
*/
struct ContextMethodNames : public MethodNames {
    ContextMethodNames() {
        static volatile unsigned initialized;    // = 0
        if (initialized++) throw runtime_error( "There should be only one ContextMethodNames" );

        map( "save", 			Context::save );
        map( "restore",     	Context::restore );
		map( "push_group",		Context::push_group );
		map( "pop_group_to_source", Context::pop_group_to_source );
		map( "set_source_rgb", 	Context::set_source_rgb );
		map( "set_source_rgba", Context::set_source_rgba );
		map( "set_source", 		Context::set_source );
		map( "set_source_surface", Context::set_source_surface );
		map( "get_source", 		Context::get_source );
#if 0
		map( "get_target", 		Context::get_target );
#endif
		map( "set_antialias", 	Context::set_antialias );
		map( "set_dash", 		Context::set_dash );
		map( "get_dash", 		Context::get_dash );
		map( "set_fill_rule", 	Context::set_fill_rule );
		map( "set_line_cap", 	Context::set_line_cap );
		map( "set_line_join", 	Context::set_line_join );
		map( "set_line_width", 	Context::set_line_width );
		map( "set_miter_limit", Context::set_miter_limit );
		map( "set_operator", 	Context::set_operator );
		map( "set_tolerance", 	Context::set_tolerance );
		map( "clip", 			Context::clip );
		map( "clip_preserve", 	Context::clip_preserve );
		map( "reset_clip", 		Context::reset_clip );
		map( "clip_extents", 	Context::clip_extents );
		map( "fill", 			Context::fill );
		map( "fill_preserve", 	Context::fill_preserve );
		map( "fill_extents", 	Context::fill_extents );
		map( "in_fill", 		Context::in_fill );
		map( "mask", 			Context::mask );
		map( "mask_surface", 	Context::mask_surface );
		map( "paint", 			Context::paint );
		map( "paint_with_alpha", Context::paint_with_alpha );
		map( "stroke", 			Context::stroke );
		map( "stroke_preserve", Context::stroke_preserve );
		map( "stroke_extents", 	Context::stroke_extents );
		map( "in_stroke", 		Context::in_stroke );
		map( "copy_page", 		Context::copy_page );
		map( "show_page", 		Context::show_page );
		
		// <http://www.cairographics.org/manual/cairo-transformations.html>
		//
		map( "translate",		Context::translate );
		map( "scale",			Context::scale );
		map( "rotate",			Context::rotate );
		map( "transform",	    Context::transform );
		map( "set_matrix",		Context::set_matrix );
		map( "get_matrix",		Context::get_matrix );
		map( "identity_matrix",	Context::identity_matrix );
		map( "user_to_device",	Context::user_to_device );
		map( "user_to_device_distance", Context::user_to_device_distance );
		map( "device_to_user",	Context::device_to_user );
		map( "device_to_user_distance", Context::device_to_user_distance );
		
		// <http://www.cairographics.org/manual/cairo-paths.html>
		//
		map( "copy_path", 		Context::copy_path );
		map( "copy_path_flat", 	Context::copy_path_flat );
		map( "append_path", 	Context::append_path );
		map( "has_current_point", Context::has_current_point );
		map( "get_current_point", Context::get_current_point );
		map( "new_path", 		Context::new_path );
		map( "new_sub_path", 	Context::new_sub_path );
		map( "close_path", 		Context::close_path );
		map( "arc", 			Context::arc );
		map( "arc_negative", 	Context::arc_negative );
		map( "circle", 			Context::circle );
		map( "curve_to", 		Context::curve_to );
		map( "line_to", 		Context::line_to );
		map( "move_to", 		Context::move_to );
		map( "rectangle", 		Context::rectangle );
		//map( "glyph_path", 		Context::glyph_path );
		map( "text_path", 		Context::text_path );
		map( "rel_curve_to", 	Context::rel_curve_to );
		map( "rel_line_to", 	Context::rel_line_to );
		map( "rel_move_to", 	Context::rel_move_to );
		map( "path_extents", 	Context::path_extents );

        map( "select_font_face", Context::select_font_face );
        map( "set_font_size", 	Context::set_font_size );
        map( "set_font_matrix", Context::set_font_matrix );
        map( "get_font_matrix", Context::get_font_matrix );
        map( "set_font_options", Context::set_font_options );
        map( "get_font_options", Context::get_font_options );
        map( "set_font_face", 	Context::set_font_face );
        map( "get_font_face", 	Context::get_font_face );
        map( "set_scaled_font", Context::set_scaled_font );
        map( "get_scaled_font", Context::get_scaled_font );
        map( "show_text", 		Context::show_text );
        //map( "show_glyphs", 	Context::show_glyphs );
        map( "font_extents", 	Context::font_extents );
        map( "text_extents", 	Context::text_extents );
    }
};
static struct ContextMethodNames context_method_names;


/*
* ...= __index( context_ud, key_any )
*/
int Context_bind::index( lua_State *L ) {

    L_GROW(2);

    const char *s= lua_tostring(L,2);
    if (s) {
        // Data members
        //
        if (strcmp(s,"info")==0) {
            Context &my= *Context::instance(L,1);
            lua_newtable(L);
            
            // .fill_rule ("winding"|"even_odd")
            //
            lua_pushliteral(L,"fill_rule");
            lua_pushstring(L, conv_fill_rule( cairo_get_fill_rule(my) ));
            lua_settable(L,-3);

            // .line_cap ("butt"|"round"|"square")
            //
            lua_pushliteral(L,"line_cap");
            lua_pushstring(L, conv_line_cap( cairo_get_line_cap(my) ));
            lua_settable(L,-3);

            // .line_join ("miter"|"round"|"bevel")
            //
            cairo_line_join_t e_lj= cairo_get_line_join(my);
            lua_pushliteral(L,"line_join");
            lua_pushstring(L, conv_line_join( e_lj ));
            lua_settable(L,-3);

            // .line_width (num)
            //
            lua_pushliteral(L,"line_width");
            lua_pushnumber( L, cairo_get_line_width(my) );
            lua_settable(L,-3);

            // .miter_limit (num)
            //
            // Note: This matters only when '.line_join' is "miter"; maybe
            //      we shouldn't even give it unless it is so.
            //
            if (e_lj==CAIRO_LINE_JOIN_MITER) {
                lua_pushliteral(L,"miter_limit");
                lua_pushnumber( L, cairo_get_miter_limit(my) );
                lua_settable(L,-3);
            }

            // .operator (string)
            //
            lua_pushliteral(L,"operator");
            lua_pushstring( L, conv_operator( cairo_get_operator(my) ) );
            lua_settable(L,-3);

            // .tolerance (num)
            //
            lua_pushliteral(L,"tolerance");
            lua_pushnumber( L, cairo_get_tolerance(my) );
            lua_settable(L,-3);

            //---
            //...

            status_check_ok( L, cairo_status(my) );
            return 1;
        }

        // Function members
        //
        lua_CFunction f= context_method_names(s);
        if (f) {
            lua_pushvalue( L,1 );   // copy of the context
            lua_pushcclosure( L, f, 1 );
            return 1;
        }
    }

    return 0;   // nil
}


/*
*/
void Context_bind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Add our own methods
    //
    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);
}

/*
*/
Context::Context( cairo_surface_t *cs ) 
    : cr( cairo_create(cs) ) {

    // Note: 'cairo_create()' references 'cs' internally so we don't have to
    //      keep the surface object alive for the lifespan of 'cr'.

    INVARIANT();
}


/*
*/
Context::~Context() {
    INVARIANT();
    cairo_destroy(cr);
}


/*
* cr= cr.save()
*/
int Context::save( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_save(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}

/*
* cr= cr.restore()
*/
int Context::restore( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_restore(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}

/*
* cr= cr.push_group( [content_color_bool], [content_alpha_bool] )
*/
int Context::push_group( lua_State *L ) {
    proto( L, "[bool], [bool]" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    // By default, content type is CAIRO_CONTENT_COLOR_ALPHA
    // (the baisc 'cairo_push_group()' would do that)
    //
    bool content_color= lua_isboolean(L,1) ? lua_toboolean(L,1) : true;
    bool content_alpha= lua_isboolean(L,2) ? lua_toboolean(L,2) : true;

    cairo_content_t v= (!content_alpha) ? CAIRO_CONTENT_COLOR :
                       (!content_color) ? CAIRO_CONTENT_ALPHA :
                                          CAIRO_CONTENT_COLOR_ALPHA;
    cairo_push_group_with_content(my, v);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    

}

/*
* cr= cr.pop_group_to_source()
*/
int Context::pop_group_to_source( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_pop_group_to_source(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    

}

/*
* cr= cr.set_source_rgb( r_num, g_num, b_num )
* cr= cr.set_source_rgb( rgb_str|color_str )
*
* 'r_num' etc.: 0.0 .. 1.0
* 'rgb_str':    "[#]rrggbb" in hex (like with SVG)
*               "red", "blue" etc. color names (like with SVG)
*/
int Context::set_source_rgb( lua_State *L ) {
    proto( L, lua_type(L,1)==LUA_TNUMBER ? "number,number,number" : "string" );

    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double r,g,b;
    parse_rgba( L, 1, r,g,b, nullptr );

    cairo_set_source_rgb( my, r,g,b );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}

/*
* cr= cr.set_source_rgba( r_num, g_num, b_num, a_num )
* cr= cr.set_source_rgba( rgb_str|color_str, a_num )
*
* 'r_num' etc.: 0.0 .. 1.0
* 'rgb_str':    "[#]rrggbb" in hex (like with SVG)
*               "red", "blue" etc. color names (like with SVG)
*/
int Context::set_source_rgba( lua_State *L ) {
    proto( L, lua_type(L,1)==LUA_TNUMBER ? "number,number,number,number" : "string,number" );

    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double r,g,b,a;
    parse_rgba( L,1, r,g,b, &a );

    cairo_set_source_rgba( my, r,g,b,a );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}


/*
* cr= cr.set_source( pattern )
*/
int Context::set_source( lua_State *L ) {
    proto( L, "CairoPattern" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Pattern &pat= Pattern::instance_notnull(L,1);

    cairo_set_source( my, pat );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}

/*
* cr= cr.set_source_surface( surface, x_num, y_num )
*/
int Context::set_source_surface( lua_State *L ) {
    proto( L, "CairoSurface,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Surface &surf= Surface::instance_notnull(L,1);
    double x= lua_tonumber(L,2);
    double y= lua_tonumber(L,3);
    
    cairo_set_source_surface( my, surf, x,y );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}

/*
* pattern= cr.get_source()
*/
int Context::get_source( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_pattern_t *cp= cairo_get_source(my);

    // "This object is owned by cairo. To keep a reference to it, you must call
    // cairo_pattern_reference()."
    //
    cairo_pattern_reference(cp);

    status_check_ok( L, cairo_status(my) );

    new(L) Pattern(cp);     // will call 'cairo_pattern_destroy()' at GC
    return 1;
}

/*
* surface= cr.get_target()
*
* TBD: Instead of doing it like this, we could tie 'cr' to its target in Lua, via
*      registry or something. Might be easier - handling memory with shared resources
*      ties up into problems rather fast.   AKa 15-Dec-2010
*/
#if 0
int Context::get_target( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_surface_t *cs= cairo_get_target(my);

    // "This object is owned by cairo. To keep a reference to it, you must call
    // cairo_surface_reference()."
    //
    cairo_surface_reference(cs);

    status_check_ok( L, cairo_status(my) );

    new(L) Surface(cs);     // will call 'cairo_surface_destroy()' at GC
    return 1;
}
#endif


/*
* cr= cr.set_antialias( antialias_str|bool )
*
* antialias_str:    "default"
*                   "none"  (same as false)
*                   "gray"  (same as true)
*                   "subpixel"
*/
int Context::set_antialias( lua_State *L ) {
    proto( L, "string|bool" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_antialias_t e;
    
    if (lua_isboolean(L,1)) {
        e= lua_toboolean(L,1) ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE;
    } else {
        e= conv_antialias(L,1);
    }

    cairo_set_antialias( my, e );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}


/*
* cr= cr.set_dash( [{ [num [, ...]] }] [, offset_num] )
*
* Note: '{}' (or nothing) disables dashing
*       '{num}' gives a symmetric dashing
*/
int Context::set_dash( lua_State *L ) {
    proto( L, "[{ [number], ... }], [number]" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    unsigned n;

    if (!lua_istable(L,1)) {
        if (!(lua_isnil(L,1) || lua_isnone(L,1))) {
            luaL_error( L, "Table expected, got %s", L_typename(1) );
        }
        n= 0;
    } else {
        n= lua_objlen(L,1);
    }

    double offset= lua_tonumber(L,2);   // 0.0 by default
    
    double *arr= new double[n];
    {
        for( unsigned i=0; i<n; i++ ) {
            lua_pushinteger(L,i+1);
            lua_gettable(L,1);
            
            if (lua_type(L,-1) != LUA_TNUMBER) {
                luaL_error( L, "Bad dash" );
            }
            arr[i]= lua_tonumber(L,-1);
            lua_pop(L,1);
        }
        cairo_set_dash( my, arr, n, offset );
    }
    delete[] arr;    

    // "If any value in dashes is negative, or if all values are 0, then 'cr'
    // will be put into an error state with a status of CAIRO_STATUS_INVALID_DASH."
    //
    status_check( L, cairo_status(my) );

    lua_pushvalue(L,lua_upvalueindex(1));     // return self for chaining
    return 1;    
}
                                  
                                  
/*
* { [num [, ...]] }, offset_num= cr.get_dash()
*/
int Context::get_dash( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    unsigned n= cairo_get_dash_count(my);

    double offset;
    double *arr= new double[n];
    {
        cairo_get_dash( my, arr, &offset );
        
        lua_newtable(L);
        for( unsigned i=0; i<n; i++ ) {
            lua_pushinteger( L, i+1 );
            lua_pushnumber( L, arr[i] );
            lua_settable( L, -3 );
        }
    }
    delete[] arr;
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber( L, offset );
    return 2;
}


/*
* cr= cr.set_fill_rule( fill_rule_str )
*
* fill_rule_str:    "winding"
*                   "even_odd"
*/
int Context::set_fill_rule( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_set_fill_rule( my, conv_fill_rule(L,1) );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
        

/*
* cr= cr.set_line_cap( line_cap_str )
*
* line_cap_str: "butt"|"round"|"square"
*/
int Context::set_line_cap( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_set_line_cap( my, conv_line_cap(L,1) );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.set_line_join( line_join_str )
*
* line_join_str: "miter"|"round"|"bevel"
*/
int Context::set_line_join( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_set_line_join( my, conv_line_join(L,1) );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.set_line_width( num )
*/
int Context::set_line_width( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double w= lua_tonumber(L,1);

    cairo_set_line_width( my, w );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.set_miter_limit( num )
*/
int Context::set_miter_limit( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v= lua_tonumber(L,1);

    cairo_set_miter_limit( my, v );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
        
                           

/*
* cr= cr.set_operator( operator_str )
*/
int Context::set_operator( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_operator_t op= conv_operator(L,1);
    
    cairo_set_operator( my, op );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.set_tolerance( num )
*/
int Context::set_tolerance( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_set_tolerance( my, lua_tonumber(L,2) );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.clip()
*
* Note: Chaining makes sense even for 'clip' (which empties the existing
*       path), just like it does for 'fill' and 'stroke'. Though, often,
*       'clip()' may be the last call of a chain.
*/
int Context::clip( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_clip(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.clip_preserve()
*/
int Context::clip_preserve( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_clip_preserve(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.reset_clip()
*/
int Context::reset_clip( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_reset_clip(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
  
/*
* x1_num, y1_num, x2_num, y2_num= cr.clip_extents()
*/
int Context::clip_extents( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v[4];

    cairo_clip_extents( my, &v[0], &v[1], &v[2], &v[3] );
    status_check_ok( L, cairo_status(my) );

    for( unsigned i=0; i<4; i++ ) {
        lua_pushnumber( L, v[i] );
    }
    return 4;
}
         
/*
* cr= cr.fill()
*/
int Context::fill( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_fill(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.fill_preserve()
*/
int Context::fill_preserve( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_fill_preserve(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
           
/*
* x1_num, y1_num, x2_num, y2_num= cr.fill_extents()
*/
int Context::fill_extents( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v[4];
    cairo_fill_extents( my, &v[0], &v[1], &v[2], &v[3] );
    status_check_ok( L, cairo_status(my) );

    for( unsigned i=0; i<4; i++ ) {
        lua_pushnumber( L, v[i] );
    }
    return 4;
}


/*
* bool= cr.in_fill( x_num, y_num )
*/
int Context::in_fill( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);
    
    bool v= cairo_in_fill(my,x,y);
    status_check_ok( L, cairo_status(my) );

    lua_pushboolean( L, v );
    return 1;
}

/*
* cr= cr.mask( pattern )
*/
int Context::mask( lua_State *L ) {
    proto( L, "CairoPattern" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Pattern &pat= Pattern::instance_notnull(L,1);

    cairo_mask(my, pat);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.mask_surface( surface [,x_num, y_num] )
*/
int Context::mask_surface( lua_State *L ) {
    proto( L, lua_gettop(L)==1 ? "CairoSurface" : "CairoSurface,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Surface &surf= Surface::instance_notnull(L,1);
    double x= lua_tonumber(L,2);    // 0.0 by default
    double y= lua_tonumber(L,3);

    cairo_mask_surface(my, surf, x,y);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.paint()
*/
int Context::paint( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_paint(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.paint_with_alpha( alpha_num )
*/
int Context::paint_with_alpha( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_paint_with_alpha(my, lua_tonumber(L,1));
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.stroke()
*/
int Context::stroke( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_stroke(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.stroke_preserve()
*/
int Context::stroke_preserve( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_stroke_preserve(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
                       
/*
* x1_num, y1_num, x2_num, y2_num= cr.stroke_extents()
*/
int Context::stroke_extents( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v[4];
    cairo_stroke_extents( my, &v[0], &v[1], &v[2], &v[3] );
    status_check_ok( L, cairo_status(my) );

    for( unsigned i=0; i<4; i++ ) {
        lua_pushnumber( L, v[i] );
    }
    return 4;
}


/*
* bool= cr.in_stroke( x_num, y_num )
*/
int Context::in_stroke( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);
    
    bool v= cairo_in_stroke(my,x,y);
    status_check_ok( L, cairo_status(my) );

    lua_pushboolean( L, v );
    return 1;
}

/*
* cr= cr.copy_page()
*/
int Context::copy_page( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_copy_page(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.show_page()
*/
int Context::show_page( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    cairo_show_page(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.translate( tx_num, ty_num )
*/
int Context::translate( lua_State *L ) {
    proto( L, "number, number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double tx= lua_tonumber(L,1);
    double ty= lua_tonumber(L,2);
    
    cairo_translate(my, tx,ty);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.scale( sx_num, sy_num )
* cr= cr.scale( s_num )
*/
int Context::scale( lua_State *L ) {
    proto( L, "number, [number]" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double sx= lua_tonumber(L,1);
    double sy= (lua_gettop(L)>1) ? lua_tonumber(L,2) : sx;

    if ((sx==0.0) || (sy==0.0)) {
        luaL_error( L, "Scale needs to be non-zero (%f, %f)", sx, sy );
    }

    cairo_scale(my, sx,sy);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.rotate( rad_num )
*/
int Context::rotate( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double rad= lua_tonumber(L,1);
    
    cairo_rotate(my, rad);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.transform( matrix )
*/
int Context::transform( lua_State *L ) {
    proto( L, "matrix" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Matrix &matrix= Matrix::instance_notnull(L,1);
    
    cairo_transform(my, matrix);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.set_matrix( matrix )
*/
int Context::set_matrix( lua_State *L ) {
    proto( L, "matrix" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Matrix &matrix= Matrix::instance_notnull(L,1);
    
    cairo_set_matrix(my, matrix);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* matrix= cr.get_matrix()
*/
int Context::get_matrix( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_get_matrix(my, *new(L) Matrix());     // pushes matrix
    status_check_ok( L, cairo_status(my) );
    return 1;
}


/*
* cr= cr.identity_matrix()
*/
int Context::identity_matrix( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_identity_matrix(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* x', y'= cr.user_to_device( x_num, y_num )
*/
int Context::user_to_device( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);
    
    cairo_user_to_device(my, &x,&y);
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber(L,x);
    lua_pushnumber(L,y);
    return 2;
}

/*
* dx', dy'= cr.user_to_device_distance( dx_num, dy_num )
*/
int Context::user_to_device_distance( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double dx= lua_tonumber(L,1);
    double dy= lua_tonumber(L,2);
    
    cairo_user_to_device_distance(my, &dx,&dy);
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber(L,dx);
    lua_pushnumber(L,dy);
    return 2;
}

/*
* x', y'= cr.device_to_user( x_num, y_num )
*/
int Context::device_to_user( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);
    
    cairo_device_to_user(my, &x,&y);
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber(L,x);
    lua_pushnumber(L,y);
    return 2;
}

/*
* dx', dy'= cr.device_to_user_distance( dx_num, dy_num )
*/
int Context::device_to_user_distance( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double dx= lua_tonumber(L,1);
    double dy= lua_tonumber(L,2);
    
    cairo_device_to_user_distance(my, &dx,&dy);
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber(L,dx);
    lua_pushnumber(L,dy);
    return 2;
}

/*
* path= cr.copy_path()
*/
int Context::copy_path( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    Path &path= *new(L) Path( cairo_copy_path(my) );
    status_check( L, path.status() );
    status_check_ok( L, cairo_status(my) );
    return 1;
}

/*
* path= cr.copy_path_flat()
*/
int Context::copy_path_flat( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    Path &path= *new(L) Path( cairo_copy_path_flat(my) );
    status_check( L, path.status() );
    status_check_ok( L, cairo_status(my) );
    return 1;
}


/*
* cr= cr.append_path( path )
*/
int Context::append_path( lua_State *L ) {
    proto( L, "path" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Path &path= Path::instance_notnull(L,1);

    cairo_append_path(my, path);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* bool= cr.has_current_point()
*/
int Context::has_current_point( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    bool v= cairo_has_current_point(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushboolean( L, v );
    return 1;
}

/*
* x_num, y_num= cr.get_current_point()
*/
int Context::get_current_point( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x,y;

    cairo_get_current_point(my, &x,&y);
    status_check_ok( L, cairo_status(my) );

    lua_pushnumber(L,x);
    lua_pushnumber(L,y);
    return 2;
}

/*
* cr= cr.new_path()
*/
int Context::new_path( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_new_path(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.new_sub_path()
*/
int Context::new_sub_path( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_new_sub_path(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.close_path()
*/
int Context::close_path( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    cairo_close_path(my);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.arc( xc_num, yc_num, r_num, angle1_num, angle2_num )
*
* 'angleX_num': in radians
*/
int Context::arc( lua_State *L ) {
    proto( L, "number,number,number,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double xc= lua_tonumber(L,1);
    double yc= lua_tonumber(L,2);
    double r= lua_tonumber(L,3);
    double angle1= lua_tonumber(L,4);
    double angle2= lua_tonumber(L,5);

    cairo_arc(my, xc,yc,r, angle1,angle2);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.arc_negative( xc_num, yc_num, r_num, angle1_num, angle2_num )
*
* 'angleX_num': in radians
*/
int Context::arc_negative( lua_State *L ) {
    proto( L, "number,number,number,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double xc= lua_tonumber(L,1);
    double yc= lua_tonumber(L,2);
    double r= lua_tonumber(L,3);
    double angle1= lua_tonumber(L,4);
    double angle2= lua_tonumber(L,5);

    cairo_arc_negative(my, xc,yc,r, angle1,angle2);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.circle( xc_num, yc_num, r_num )
*
* Note: Cairo API does not have 'circle' but we want it. And we do it like
*       'move_to()', meaning the earlier drawing and the circle point are
*       discontinued. This is for practical ease of use.    -- AKa 3-Mar-10
*/
int Context::circle( lua_State *L ) {
    proto( L, "number,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double xc= lua_tonumber(L,1);
    double yc= lua_tonumber(L,2);
    double r= lua_tonumber(L,3);

    cairo_new_sub_path(my);
    cairo_arc(my, xc,yc,r, 0,2*M_PI);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.curve_to( x1_num,y1_num, x2_num,y2_num, x3_num,y3_num )
*/
int Context::curve_to( lua_State *L ) {
    proto( L, "number,number, number,number, number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x1= lua_tonumber(L,1);
    double y1= lua_tonumber(L,2);
    double x2= lua_tonumber(L,3);
    double y2= lua_tonumber(L,4);
    double x3= lua_tonumber(L,5);
    double y3= lua_tonumber(L,6);

    cairo_curve_to(my, x1,y1, x2,y2, x3,y3);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.line_to( x_num, y_num )
*/
int Context::line_to( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);

    cairo_line_to(my, x,y);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.move_to( x_num, y_num )
*/
int Context::move_to( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);

    cairo_move_to(my, x,y);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.rectangle( x_num, y_num, w_num, h_num )
*/
int Context::rectangle( lua_State *L ) {
    proto( L, "number,number,number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double x= lua_tonumber(L,1);
    double y= lua_tonumber(L,2);
    double w= lua_tonumber(L,3);
    double h= lua_tonumber(L,4);

    cairo_rectangle(my, x,y, w,h);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.glyph_path( { glyph [, ...] } )
*/
/*** NOT ENABLED
int Context::glyph_path( lua_State *L ) {
    proto( L, "{ glyph, ... }" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    if (!lua_istable(L,1)) {
        luaL_error( L, "Expecting table, got %s", L_typename(1) );
    }

    unsigned n= lua_objlen(L,1);
    cairo_glyph_t *arr= new cairo_glyph_t[n];
    {
        for( unsigned i=0; i<n; i++ ) {
            lua_pushinteger(L,i+1);
            lua_gettable(L,1);

            if (!read_glyph(L,-1, arr[i])) {
                delete[] arr;
                luaL_error( L, "Expecting glyph, got %s", L_typename(-1) );
            }
        }
        cairo_glyph_path(my, arr, n);
    }
    delete[] arr;

    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
***/

/*
* cr= cr.text_path( utf8_str )
*/
int Context::text_path( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_text_path( my, lua_tostring(L,1) );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.rel_curve_to( dx1_num,dy1_num, dx2_num,dy2_num, dx3_num,dy3_num )
*/
int Context::rel_curve_to( lua_State *L ) {
    proto( L, "number,number, number,number, number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double dx1= lua_tonumber(L,1);
    double dy1= lua_tonumber(L,2);
    double dx2= lua_tonumber(L,3);
    double dy2= lua_tonumber(L,4);
    double dx3= lua_tonumber(L,5);
    double dy3= lua_tonumber(L,6);

    // "it is an error to call this function with no current point;
    // doing so will ...  CAIRO_STATUS_NO_CURRENT_POINT."
    //
    cairo_rel_curve_to(my, dx1,dy1, dx2,dy2, dx3,dy3);
    status_check( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.rel_line_to( dx_num, dy_num )
*/
int Context::rel_line_to( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double dx= lua_tonumber(L,1);
    double dy= lua_tonumber(L,2);

    // "it is an error to call this function with no current point;
    // doing so will ...  CAIRO_STATUS_NO_CURRENT_POINT."
    //
    cairo_rel_line_to(my, dx,dy);
    status_check( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.rel_move_to( dx_num, dy_num )
*/
int Context::rel_move_to( lua_State *L ) {
    proto( L, "number,number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double dx= lua_tonumber(L,1);
    double dy= lua_tonumber(L,2);

    // "it is an error to call this function with no current point;
    // doing so will ...  CAIRO_STATUS_NO_CURRENT_POINT."
    //
    cairo_rel_move_to(my, dx,dy);
    status_check( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* x1_num, y1_num, x2_num, y2_num= cr.path_extents()
*/
int Context::path_extents( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v[4];

    cairo_path_extents( my, &v[0], &v[1], &v[2], &v[3] );
    status_check_ok( L, cairo_status(my) );

    for( unsigned i=0; i<4; i++ ) {
        lua_pushnumber( L, v[i] );
    }
    return 4;
}

/*
* cr= cr.select_font_face( family_str [,"normal"|"italic"|"oblique"] [,"normal"|"bold"] )
*
* 'family_str': "Standard CSS2 family names are likely to work as expected
*               ("serif", "sans-serif", "cursive", "fantasy", "monospace")."
*
* Note: Use '.set_font_face()' for using local TTF fonts.
*/
int Context::select_font_face( lua_State *L ) {
    proto( L, "string, [string], [string]" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    const char *family= lua_tostring(L,1);
    cairo_font_slant_t slant= CAIRO_FONT_SLANT_NORMAL;
    cairo_font_weight_t weight= CAIRO_FONT_WEIGHT_NORMAL;

    for( unsigned i=2; i<=(unsigned)lua_gettop(L); i++ ) {
        if (!lua_isnil(L,i)) {
            if (!conv_font_slant.just_try(L,i,slant)) {
                if (!conv_font_weight.just_try(L,i,weight)) {
                    luaL_error( L, "Unexpected slant or weight: '%s'", L_string_or_typename(i) );
                }
            }
        }
    }
    
    cairo_select_font_face( my, family, slant, weight );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.set_font_size( size_num )
*/
int Context::set_font_size( lua_State *L ) {
    proto( L, "number" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    double v= lua_tonumber(L,1);
    
    cairo_set_font_size( my, v );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* cr= cr.set_font_matrix( matrix )
*/
int Context::set_font_matrix( lua_State *L ) {
    proto( L, "matrix" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    Matrix &mat= Matrix::instance_notnull(L,1);
    
    cairo_set_matrix( my, mat );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}



/*
* matrix= cr.get_font_matrix()
*/
int Context::get_font_matrix( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_get_matrix( my, *new(L) Matrix() );   // pushes matrix
    status_check_ok( L, cairo_status(my) );
    return 1;
}


/*
* cr= cr.set_font_options( {
*               [antialias= "default"|"none"|"gray"|"subpixel",]
*               [subpixel_order= "default"|"rgb"|"bgr"|"vrgb"|"vbgr",]  -- not implemented (not needed)
*               [hint_style= "default"|"none"|"slight"|"medium"|"full",]
*               [hint_metrics= "default"|"off"|"on",]
* } )
*/
int Context::set_font_options( lua_State *L ) {
    proto( L, "{ antialias=[string], subpixel_order=[string], hint_style=[string], hint_metrics=[string] }" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    if (!lua_istable(L,1)) {
        luaL_error( L, "Expected table, got %s", L_typename(1) );
    }

    const int t_idx= 1;
    cairo_font_options_t *opt= cairo_font_options_create();
    {
        lua_pushnil(L);     // first key
		while( lua_next(L, t_idx) ) {
			//
			// [-2]: key
			// [-1]: value

            const char *key= lua_tostring(L,-2);
            //const char *val= lua_tostring(L,-1);

            if (!key) continue;     // skip non-string keys (should not really be any)

            if (strcmp(key,"antialias")==0) {
                cairo_font_options_set_antialias( opt, conv_antialias(L,-1) );
            }
            else if (strcmp(key,"hint_style")==0) {
                cairo_font_options_set_hint_style( opt, conv_hint_style(L,-1) );
            }
            else if (strcmp(key,"hint_metrics")==0) {
                cairo_font_options_set_hint_metrics( opt, conv_hint_metrics(L,-1) );
            }
            else {
                luaL_error( L, "Unexpected key: %s", key );
            }

            // remove value, keep 'key' for next iteration
            //
    		lua_pop(L, 1);
		}
		cairo_set_font_options( my, opt );
    }
    cairo_font_options_destroy(opt);
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}


/*
* {
*   antialias= "default"|"none"|"gray"|"subpixel",
*   [subpixel_order= "default"|"rgb"|"bgr"|"vrgb"|"vbgr",]      -- not implemented
*   hint_style= "default"|"none"|"slight"|"medium"|"full",
*   hint_metrics= "default"|"off"|"on",
* }= cr.get_font_options()
*/
int Context::get_font_options( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    
    L_GROW(3);
    lua_newtable(L);

    cairo_font_options_t *opt= cairo_font_options_create();
    {
        cairo_get_font_options( my, opt );
        
        lua_pushliteral( L, "antialias" );
        lua_pushstring( L, conv_antialias( cairo_font_options_get_antialias(opt) ));
        lua_settable(L,-3);

        lua_pushliteral( L, "hint_style" );
        lua_pushstring( L, conv_hint_style( cairo_font_options_get_hint_style(opt) ));
        lua_settable(L,-3);

        lua_pushliteral( L, "hint_metrics" );
        lua_pushstring( L, conv_hint_metrics( cairo_font_options_get_hint_metrics(opt) ));
        lua_settable(L,-3);
    }
    cairo_font_options_destroy(opt);
    status_check_ok( L, cairo_status(my) );

    return 1;
}


/*
* cr= cr.set_font_face( font_face )
*/
int Context::set_font_face( lua_State *L ) {
    proto( L, "CairoFont" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    FontFace &font= FontFace::instance_notnull(L,1);
    
    cairo_set_font_face( my, font );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}



/*
* font_face= cr.get_font_matrix()
*/
int Context::get_font_face( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_font_face_t *ff= cairo_get_font_face(my);
        //
        // "This object is owned by cairo. To keep a reference to it, you must call 'cairo_font_face_reference()'.
        // This function never returns nullptr."
        
    cairo_font_face_reference(ff);

    status_check_ok( L, cairo_status(my) );
    
    new(L) FontFace(ff);
    return 1;
}

/*
* cr= cr.set_scaled_face( scaled_font )
*/
int Context::set_scaled_font( lua_State *L ) {
    proto( L, "scaled_font" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    ScaledFont &font= ScaledFont::instance_notnull(L,1);
    
    cairo_set_scaled_font( my, font );
    status_check_ok( L, cairo_status(my) );

    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}



/*
* scaled_font= cr.get_scaled_font()
*/
int Context::get_scaled_font( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_scaled_font_t *sf= cairo_get_scaled_font(my);
        //
        // "This object is owned by cairo. To keep a reference to it, you must call 'cairo_font_face_reference()'.
        // This function never returns nullptr."
        
    cairo_scaled_font_reference(sf);

    status_check_ok( L, cairo_status(my) );
    
    new(L) ScaledFont(sf);
    return 1;
}


/*
* cr= cr.show_text( utf8_str )
*/
int Context::show_text( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    const char *s= lua_tostring(L,1);
    
    cairo_show_text( my, s );
    status_check_ok( L, cairo_status(my) );
    
    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}

/*
* cr= cr.show_glyphs( { [glyph [, ...]] } )
*/
/*** NOT ENABLED
int Context::show_glyphs( lua_State *L ) {
    proto( L, "{ glyph, ... }" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    if (!lua_istable(L,1)) {
        luaL_error( L, "Expected table, got %s", L_typename(1) );
    }

    unsigned n= lua_objlen(L,1);
    cairo_glyph_t* arr= new cairo_glyph_t[n];
    {
        for( unsigned i=0; i<n; i++ ) {
            ... read glyph to arr
        }

        cairo_show_glyphs( *this, arr, n );
    }
    delete[] arr;
    status_check_ok( L, cairo_status(my) );
    
    lua_pushvalue( L, lua_upvalueindex(1) );    // chain
    return 1;
}
***/


/*
* { ascent= num,
*   descent= num,
*   height= num,
*   max_x_advance= num,
*   max_y_advance= num
* }= cr.font_extents()
*/
int Context::font_extents( lua_State *L ) {
    proto( L, "" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));

    cairo_font_extents_t ext;
    cairo_font_extents( my, &ext );

    status_check_ok( L, cairo_status(my) );

    lua_newtable(L);
    
    lua_pushliteral( L, "ascent" ); 
    lua_pushnumber( L, ext.ascent );
    lua_settable(L,-3);

    lua_pushliteral( L, "descent" ); 
    lua_pushnumber( L, ext.descent );
    lua_settable(L,-3);

    lua_pushliteral( L, "height" ); 
    lua_pushnumber( L, ext.height );
    lua_settable(L,-3);

    lua_pushliteral( L, "max_x_advance" ); 
    lua_pushnumber( L, ext.max_x_advance );
    lua_settable(L,-3);

    lua_pushliteral( L, "max_y_advance" ); 
    lua_pushnumber( L, ext.max_y_advance );
    lua_settable(L,-3);

    return 1;
}


/*
* { x_bearing= num,
*   y_bearing= num,
*   width= num,
*   height= num,
*   x_advance= num,
*   y_advance= num
* }= cr.text_extents( utf8_str )
*/
int Context::text_extents( lua_State *L ) {
    proto( L, "string" );
    Context &my= *Context::instance(L,lua_upvalueindex(1));
    const char *s= lua_tostring(L,1);

    cairo_text_extents_t ext;
    cairo_text_extents( my, s, &ext );

    status_check_ok( L, cairo_status(my) );

    lua_newtable(L);
    
    lua_pushliteral( L, "x_bearing" ); 
    lua_pushnumber( L, ext.x_bearing );
    lua_settable(L,-3);

    lua_pushliteral( L, "y_bearing" ); 
    lua_pushnumber( L, ext.y_bearing );
    lua_settable(L,-3);

    lua_pushliteral( L, "width" ); 
    lua_pushnumber( L, ext.width );
    lua_settable(L,-3);

    lua_pushliteral( L, "height" ); 
    lua_pushnumber( L, ext.height );
    lua_settable(L,-3);

    lua_pushliteral( L, "x_advance" ); 
    lua_pushnumber( L, ext.x_advance );
    lua_settable(L,-3);

    lua_pushliteral( L, "y_advance" ); 
    lua_pushnumber( L, ext.y_advance );
    lua_settable(L,-3);

    return 1;
}


