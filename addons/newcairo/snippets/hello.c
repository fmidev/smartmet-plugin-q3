/*
*/

#include <stdio.h>

#include <cairo.h>


static void t1( cairo_t *cr ) {
    cairo_set_line_width(cr, 3);
    cairo_set_source_rgb(cr, 255, 0, 0);
    cairo_rectangle(cr, 25, 25, 50, 50);
    cairo_stroke(cr);
}



int main( int argc, const char *argv[] ) {
    (void)argc;
    (void)argv;
    
    const char *fn= "out.png";

	cairo_surface_t *cs= cairo_image_surface_create( CAIRO_FORMAT_ARGB32, 100, 100 );

    cairo_t *cr= cairo_create(cs);
    {
        t1(cr);
    }
    cairo_destroy(cr);

    cairo_surface_write_to_png(cs, fn);
	cairo_surface_destroy(cs);

    return 0;
}
