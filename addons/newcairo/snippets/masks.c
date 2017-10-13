/*
* Speed test for determining fastest way to make many smoothened circles.
*/

#include <stdio.h>
//#include <math.h>
#define M_PI 3.14

#include <cairo.h>

#include <stdlib.h>

static const unsigned WIDTH=600;
static const unsigned HEIGHT=400; 

#define CODE_PATH 2
    //
    // 1: Draw 'CIRCLES' circles (0.14 sec for 100 on Macbook)
    // 2: Draw using mask (48 secs for 2400 circles - 50/sec)
    // 3: Draw using mask surface (0.33 secs for 2400 circles - 7270/sec)

#if CODE_PATH==1
  static const unsigned CIRCLES=100;
#endif

#define RND(v) (rand() % (v))
#define RND_1() ((rand() % 1000) / 1000.0)

/*
*/
static void draw( cairo_t *cr ) {

#if CODE_PATH==1
    // Make 'CIRCLES' colorful circles using Cairo arc
    //
    for( unsigned i=0; i<CIRCLES; i++ ) {
        unsigned r= RND(90);
        unsigned cx= r+RND(WIDTH-2*r);
        unsigned cy= r+RND(HEIGHT-2*r);

        cairo_arc( cr, cx,cy, r, 0, M_PI*2.0 );
        cairo_set_source_rgba( cr, RND_1(), RND_1(), RND_1(), RND_1() );
        cairo_fill( cr );
        
        fprintf( stderr, "%d / %d        \r", i, CIRCLES );
    }
#elif CODE_PATH==2
    // Make circles using Cairo mask
    //
    const unsigned STEP=10;
    cairo_pattern_t *pat= cairo_pattern_create_radial( 0,0, STEP/2-2, 0,0, STEP/2+2 );
        //
        cairo_pattern_add_color_stop_rgba( pat, 0, 0,0,0,1 );
        cairo_pattern_add_color_stop_rgba( pat, 1, 0,0,0,0 );

    for( unsigned y=STEP/2; y<HEIGHT; y+=STEP ) {
        for( unsigned x=STEP/2; x<WIDTH; x+=STEP ) {
            cairo_set_source_rgba( cr, RND_1(), RND_1(), RND_1(), RND_1() );

            cairo_save( cr );
            {
                cairo_translate( cr, x,y );
                cairo_mask( cr, pat );
            }
            cairo_restore( cr );

fprintf( stderr, "%d %d     \r", x, y );
        }
    }
    cairo_pattern_destroy(pat);

#elif CODE_PATH==3
    // Make circles using Cairo mask surface
    //
    const unsigned STEP=10;
    cairo_pattern_t *pat= cairo_pattern_create_radial( 0,0, STEP/2-2, 0,0, STEP/2+2 );
        //
        cairo_pattern_add_color_stop_rgba( pat, 0, 0,0,0,1 );
        cairo_pattern_add_color_stop_rgba( pat, 1, 0,0,0,0 );

    // Luodaan muistisurfaasi, johon maski stamplataan
    //
    cairo_surface_t *surf2= cairo_surface_create_similar( cairo_get_target(cr),
                                CAIRO_CONTENT_ALPHA, STEP, STEP );

    cairo_t *cr2= cairo_create(surf2);
    {
        cairo_translate( cr2, STEP/2, STEP/2 );
        cairo_mask( cr2, pat );
    }
    cairo_destroy(cr2);
    cairo_pattern_destroy(pat);

    for( unsigned y=STEP/2; y<HEIGHT; y+=STEP ) {
        for( unsigned x=STEP/2; x<WIDTH; x+=STEP ) {
            cairo_set_source_rgba( cr, RND_1(), RND_1(), RND_1(), RND_1() );

            // 'cairo_mask_surface' saa x,y parametrit joten translatea ja save/restorea ei tarvita
            //
            cairo_mask_surface( cr, surf2, x,y );

fprintf( stderr, "%d %d     \r", x, y );
        }
    }
    
    cairo_surface_destroy(surf2);
#else
# error "Wrong CODE_PATH"
#endif
fprintf( stderr, "\n" );
}

/*
*/
int main( int argc, const char *argv[] ) {
    (void)argc;
    (void)argv;
    
    const char *fn= "out.png";

	cairo_surface_t *cs= cairo_image_surface_create( CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT );

    cairo_t *cr= cairo_create(cs);
    {
        draw(cr);
    }
    cairo_destroy(cr);

    cairo_surface_write_to_png(cs, fn);
	cairo_surface_destroy(cs);

    return 0;
}
