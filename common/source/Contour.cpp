/*
* CONTOUR.CPP                             Copyright (c) 2010, Ilmatieteen laitos
*
* Calculation of iso lines (contours) for matrix data.
*/
#include "Tools.h"
#include "Proto.h"
#include "Matrix.h"
#include "Contour.h"
#include "Vector.h"

#include <string.h>

// 21-Nov-2011 PKi: For drawing, filling and labeling contours
//
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include "cairo.h"
#include "Labelizer.h"
#undef assert_invariant
#include "Context.hpp"
#include "TronHints.h"
extern LuaNew_ID Context_bind::ID;

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "Smoothener.h"

using namespace std;

// Closed areas are automatically re-expanded after smoothening, to compensate for 
// areal loss (which is calculated). If the need for expansion is below this limit, 
// no expansion will be done.
//
#define AREA_EXPANSION_LIMIT 1.01

LuaNew_ID ContourBind::ID;
LuaNew_ID EdgePointBind::ID;


/*---=== Helpers ===---*/



/*---=== ContourBind ===---*/

/*
* [edgepoint_ud]= __index( contour_ud, int )      -- 1..N (otherwise returning nil)
*
* We use 1..N Lua indices to make the contour look like a Lua array. However, 'offset'
* has been set such that the first point seen by Lua has some guarantees to it (to make
* Lua coding simpler). See 'set_offset()' and 'INVARIANT()'.
*/
int ContourBind::__index( lua_State *L ) {
    const Contour &my= *Contour::instance(L,1);

    if (lua_type(L,2) == LUA_TSTRING) {
        const char *s= lua_tostring(L,2);
        (void)s;

    } else {
        int k= lua_tointeger(L,2);  // 0 if it's not a number
        if (k>0) {
            unsigned n_= my.points.size();
            
            assert( my.offset != ((unsigned)-1) );     // 'set_offset()' must have been called

            if ((unsigned)k <= n_) {
                new(L) EdgePoint( my.points[ (my.offset+(k-1)) %n_ ] );  // copy constructor to Lua userdata
                return 1;
            } else {
                return 0;   // no more points
            }
        }
    }

    luaL_error( L, "Bad index (for contour): %s", L_string_or_typename(2) );
    return 0;   // never
}


/*
* uint= __len( contour_ud )
*/
int ContourBind::__len( lua_State *L ) {
    const Contour &my= *Contour::instance(L,1);

    lua_pushinteger( L, my.points.size() );
    return 1;
}

/*
* Set up a metatable.
*/
void ContourBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,__index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__len");
    lua_pushcfunction(L,__len);
    lua_settable(L,-3);
}


/*---=== EdgePointBind ===---*/

/*
* ...= __index( contour_ud, "x"|"y"|"edge" )      -- n: 1..N
*/
int EdgePointBind::__index( lua_State *L ) {
    const EdgePoint &my= *EdgePoint::instance(L,1);

    const char *s= lua_tostring(L,2);
    if (s) {
        if (strcmp(s,"x")==0) {
            lua_pushnumber( L, my.x );
            return 1;
        }
        if (strcmp(s,"y")==0) {
            lua_pushnumber( L, my.y );
            return 1;
        }
        if (strcmp(s,"edge")==0) {
            lua_pushboolean( L, my.edge );
            return 1;
        }
    }
    luaL_error( L, "Bad index (for EdgePoint): %s", L_string_or_typename(2) );
    return 0;   // never
}

/*
* str= __tostring()
*
* This is only needed for debugging (makes 'DUMP()' show the point internals).
*/
int EdgePointBind::__tostring( lua_State *L ) {
    const EdgePoint &my= *EdgePoint::instance(L,1);

    lua_pushfstring( L, "{ x=%f, y=%f, edge=%s }", my.x, my.y, my.edge ? "true":"false" );
    return 1;
}

/*
* Set up a metatable.
*/
void EdgePointBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,__index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__tostring");
    lua_pushcfunction(L,__tostring);
    lua_settable(L,-3);
}


/*---=== Contour ===---*/

/*
* Returns 'true' if the coordinates 'x,y' are on the curve.
*
* TBD: This function is only required for Tron contouring, because we don't get 'edge' 
*      info directly from Tron. This approach is HIGHLY UNEFFICIENT.
*/
#ifdef USE_TRON
bool Contour::at_edge( double x, double y ) const {
    unsigned n= points.size();
    for( unsigned i=0; i<n; i++ ) {
        const Point &p1= points[i];
        const Point &p2= points[(i+1)%n];
    
        // Let's think of 'p[i]' as the origo, and check next point and (x,y) relative to it.
        //
        Point v= p2-p1;             // the edge
        Point v2= Point(x,y)-p1;    // along that edge?

        // Beware of null vectors
        //
        if (p2==p1) {
            return Point(x,y)==p1;  // at edge if exactly at that point
        }

        // Cross product tells whether (x,y) is left, right or (almost) on the line
        //
        double cp= v.cross_z(v2);
        if (fabs(cp) < 1.0e-5) {    // close enough
            // Dot product tells if it's within our length (0.0 .. v.norm())
            //
            double d= v.dot(v2);  // projection on the edge section

            if ((d>=0.0) && (d*d<=v.norm_pow2())) {      // faster to multiply than to 'sqrt' (at least not slower)
                return true;    // at edge
            }
        }
    }
    return false;
}
#endif


/*
* { deg_num, ... }= bind.calc_slants( { p1_ud, ... } )
*
* Calculate the slants for labels if they are to be placed in each point.
*
* Note: This function is run once for each part to be labelled, and results used for
*       considering how good the particular position is (how horizontal it is). This
*       happens even if all labels are to be plotted horizontal.
*
* Note: If the part is not closed, we won't be needing the first and last point's
*       slants. However, for shortness of code we calculate them anyways.
*/
int Contour::calc_slants( lua_State *L ) {
    proto( L, "{ EdgePoint, ... }" );

    const int table_idx= 1;
    L_GROW(3);

    // Read the points to a C++ vector
    //
    vector<Point> points;
    EdgePoint *e;			// 04-Nov-2011 PKi

    lua_pushnil(L);     // first key
    while (lua_next(L, table_idx)) {
        // [-2]: key (1..len)
        // [-1]: value (EdgePoint)

    	// 04-Nov-2011 PKi: Check the value to detect end of part's contours; other keys/values follow.
    	//
    	//                  This function is currently not in use.

        if (! (e = EdgePoint::instance(L,-1)))
            break;
        
        points.push_back( *e );
        lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
    }

    unsigned len= points.size();

    lua_newtable(L);    // table to be returned

    // On each iteration, we handle two vectors (one from previous point to us and one from
    // us to the next point).
    //
    Point va= points[0] - points[len-1];

    for( unsigned i=0; i<len; i++ ) {
        Point vb= points[(i+1)%len] - points[i];
        
        // Slant of the sum vector is what we want (but with 0 to East == horizontal
        // and growing clock-wise)
        //
        Vector va_vb( va.x+vb.x, va.y+vb.y, false /*cartesian*/ );
        double deg= fmod( va_vb.getDeg() +270.0, 360.0 );

        lua_pushinteger( L, i+1 );
        lua_pushnumber( L, deg );
        lua_settable( L, -3 );

        va= vb;
    }
    
    return 1;
}


/*
* [smoothenedmatrix,] tronhints, [contour_ud [, ...]]= contour( matrix, lo_val, hi_val [, smooth_length, smooth_degree, smoothenedmatrix, tronhints ] )
*
* Calculate contours of constant value.
*/
int Contour::contour( lua_State *L ) {
    proto( L, "Matrix, number, [ number ], [ number ], [ number ], [ Matrix ], [ TronHints ]");

    const Matrix &my= *Matrix::instance( L, 1 );

    /*
    * Here's invariants for the contours we should get:
    *   - All curves are closed
    *   - Higher values are to the right of the curves (they travel around 'hills' clockwise)
    *   - At edges (material edge or near missing data) curves are marked as "edge"
    *    
    * This also guarantees any curves of a certain level value will always be alike
    * (that contours and fills have exactly the same paths).
    */

    // TBD: We could have a 'ContourCollector' object here functioning as a a callback.
    //      Would reduce the time used in copying stuff - pushing a contour directly
    //      to Lua stack and letting the adapter fill stuff to it.
    //
    //      The adapter could also encapsulate 'my' into it, keeping access to the matrix
    //      here (only accessed via 'ContourCollector').
    //
    // 29-May-2012 PKi: If smoothening has already been done, the latter input matrix is just used and returned;
    //					otherwise the source/former matrix is pushed and it is smoothened if requested.
    //
    //					Note: if old style call (not using lua side contourcollector) from user script,
    //					the source matrix is pushed (it's used to get the contours) but not returned.
    //
    // 05-Jun-2012 PKi: Using tron hints if the 'TronHints' arg is passed in. If it is nil, TronHints
    //					object is pushed onto stack; otherwise it is just used and returned. If the 'TronHints'
    //					arg is omitted, nil is pushed and returned to keep the number of return values preceeding
    //					the contours constant
    //
    unsigned argc= lua_gettop(L);					// 5 or 6 args expected, 2 args if old style call
    unsigned coffs= ((argc > 3) ? 2 : 0);			// Max number of pushed values in front of contours
    unsigned tos= argc + ((argc > 3) ? 0 : 1);		// Top of stack; first return value

    float lo_val= lua_tonumber( L,2 ),hi_val= ((argc >= 3) ? lua_tonumber( L,3 ) : NAN);

    ContourCollector cc( L );

    MemMatrix * mm;
    Matrix * mys = ((argc >= 6) ? Matrix::instance( L, 6 ) : NULL);

    if (mys) {
    	// Use and return the smoothened matrix
    	//
    	mm = (MemMatrix *) mys;
    	tos--;
    }
    else
    	// Push and use the source matrix, data will be smoothened if requested
    	//
    	mm = new(L) MemMatrix(my);

    ContourMatrix cm(mm);

    // 13-Dec-2011 PKi: Smoothing length and degree for tron. No smoothing by default.
    //                  Default degree is 2
	//
    // 29-May-2012 PKi: Use zero smoothing length if the data has been smoothened already
    //
	int smooth_length = (mys ? 0 : ((argc >= 4) ? (int) lua_tonumber( L,4 ) : 0));
	int smooth_degree = ((argc >= 5) ? (int) lua_tonumber( L,5 ) : 2);

#ifdef USE_TRON
    // 05-Jun-2012 PKi: To minimize the changes needed for using tron hints the 'TronHints' c++ type is known by
	//					Contour_Tron module only; pass forward the Lua stack and the index to take the object from the stack.
	//
	//					If tron hints are to be used - if 'TronHints' object was passed in - top of stack will be adjusted down by 1 by the
	//				    call if the object is not nil (nothing pushed, the hints are just used and returned); if hints are not to be used, push
	//					nil onto the stack to keep the number of return values preceeding the contours constant
	//
	if ((argc > 3) && (argc < 7))
		lua_pushnil(L);

    tron_contour( cc, cm, lo_val, hi_val, smooth_length, smooth_degree, (argc == 7) ? L : NULL, argc, tos );    // calls back 'cc.push_contour' 0..N times (adds stuff to Lua stack)
#else
    luaL_error( L, "TRON contouring not compiled in - cannot calculate contours." );
    (void) val;
#endif

    // Prepare the pushed contours to be used from Lua, by setting their 'offset' fields
    // (provides some extra assurance which helps keep Lua side simple).
    //    
    // 29-May-2012 PKi: Set offsets only when old style call (not used by lua side contourcollector)
    //
    // 02-Mar-2015 PKi: Offsets must always be set for proper egde handling (must start stroking from the start of a strokable section; offset is needed by draw_path())
    //
	for( unsigned i=tos+1+coffs; i<= (unsigned)lua_gettop(L); i++ ) {
		Contour *c= Contour::instance( L, i );
		assert(c);

		c->set_offset();
	}

    return lua_gettop(L) - tos;    // number of return values
}


/*
* contour_ud= contour_smoothen_one( contour_ud, smooth_num [, q3smoothening] )
*
* Create a new, smoothened contour.
*  
* factor:   How much to move a point towards the window average:
*               0.0 (no smoothening) .. 1.0 (maximum smoothening)
*
* window_size: How many points are considered for the average (3/5/7)
*           Note: near edges, we automatically tune this lower.
* 
* Ref. <http://www.sli.unimelb.edu.au/gisweb/LGmodule/LGSmoothing.htm>
*
* Algorithm:
*   For each sliding window of 'window_size' (3/5/7) points:
*   - take the average of their x,y
*   - move middle point a fraction towards the average
*   - repeat..
*   - first and last "half window" (1/2/3) points are not moved,
*     if the contour is open ended
*
* Note:
*   - smoothened line is deterministic; certain data with certain parameters
*     always creates the same smoothened version. THIS IS IMPORTANT since
*     we need to get exactly same results on multiple rounds, to i.e. have
*     neighbouring filled areas (with same smoothening factor) match.
*   - smoothened line is not dependent on the direction travelled.
*   - smoothened contour has the same number of points as the original
*     contour. They have simply been slightly moved.
*
* 03-Mar-2015 PKi: Applying the given factor instead of maximum smoothening.
* 				   Added support for q2 smoothening too.
*
*/
int Contour::contour_smoothen_one( lua_State *L ) {
    const unsigned window_size= 5;      // currently fixed

    proto( L, "Contour,number,[bool]" );

    const Contour &old= * Contour::instance(L,1);

    double factor= lua_tonumber( L, 2 );
    if (factor<=0.0) {
        luaL_error( L, "Bad smoothening factor: %f", factor );
    }

    if ((window_size<3) || (window_size%2==0)) {
        luaL_error( L, "Bad window size (must be odd integer > 1): %d", window_size );
    }

    // Make a copy of the contour - we change the copy in-place.
    //
    Contour &c2= * new(L) Contour(old);

	// 03-Mar-2015 PKi: Support for q2 smoothening

	bool q3smoothening = ((lua_gettop(L) < 3) || lua_toboolean(L,3));

	if (q3smoothening) {
		// Smoothening itself
		//
	    if (factor>1.0)
	    	factor= 1.0;

		unsigned n= c2.points.size();
		int half_window= (window_size-1)/2;        // 1/2/3
		unsigned i=0;

		for( vector<EdgePoint>::iterator it= c2.points.begin();
			it != c2.points.end();
			++it, ++i ) {

			// If 'half_window-1' points left and round of '*it' are not edge points, smoothen.
			// If there are edge points, try with a shortened range.
			//
			for( int hf= half_window; hf>=1; hf-- ) {
				Vectors::Point avg;
				bool try_smaller_window= false;
				for( int j= -hf; j<=hf; j++ ) {
					const EdgePoint &p= old.points[ (i+j+n)%n ];

					// First or last point are allowed to be at edge but not others (there must be
					// no edge sections within our window, or we won't do the adjust).
					//
					if (p.edge && (abs(j) < half_window)) {
						try_smaller_window= true; break;   // do not move this point
					} else {
						avg += Vectors::Point( p.x, p.y );
					}
				}

				if (!try_smaller_window) {
					Vectors::Point p( it->x, it->y );
					p += Vectors::Vector( p, avg/(hf*2+1) ) * factor;   // move the point
					*it= EdgePoint( p.x, p.y, false );   // smoothened
					break;
				}
			}
		}

		// Provide strecthing to compensate for area loss due to smoothening.
		//
		// Note: For areas touching an edge (having "edge" sections) the correction
		//      is possibly little less than what would have been best (our code expects
		//      an even expansion, but the edge points won't be expanded)
		//
		double area_was= old.area();
		double area_now= c2.area();

		double factor_1d= sqrt(area_was) / sqrt(area_now);     // How much to expand (>1.0) to make areas (almost) like they were

		if (factor_1d > AREA_EXPANSION_LIMIT) {
			c2.stretch_out( factor_1d );
		}

        // Confirm compensation is good (it is, gives around 100% original area)
#if 0
		double area_compensated= c2.area();
		LOG_DEBUG( "Area compensation: %.3f -> %.3f (%.1f%%) -> %.3f (%.1f%%)", area_was, area_now, (area_now/area_was)*100.0, area_compensated, (area_compensated/area_was)*100.0 );
#endif
	}
	else {
		SmoothPath sp;

		for( vector<EdgePoint>::iterator it= c2.points.begin(); it != c2.points.end(); it++ )
			sp.add_point(Vectors::Point(it->x,it->y),it->edge);

		sp.done(factor,true);

		vector<PointAndEdge>::const_iterator spit = sp.begin();

		for( vector<EdgePoint>::iterator it= c2.points.begin(); ((it != c2.points.end()) && (spit != sp.end())); it++, spit++ )
			*it = EdgePoint(spit->x,spit->y,it->edge);
	}

    assert( c2.points.size() == old.points.size() );
    
    return 1;   // 'c2' pushed
}


/*
* Stretch a contour by 'factor' (>1.0) from its center point. The factor is a one-dimensional
* factor; each non-edge point is moved that much further away from the center.
*/
void Contour::stretch_out( double factor ) {
    unsigned n= points.size();

    // Get the center point
    //
    Point cp;
    for( vector<EdgePoint>::const_iterator it= points.begin(); it != points.end(); ++it ) {
        cp += *it;
    }
    cp /= n;

    for( unsigned i=0; i<n; i++ ) {
        const EdgePoint &p= points[i];
        if (!p.edge) {
            points[i]= EdgePoint( cp+ (p-cp) * factor, false );
        }
    }
}


/*
* Ref. <http://www.wikihow.com/Calculate-the-Area-of-a-Polygon>
*/
double Contour::area() const {

    // "Multiply the x coordinate of each vertex by the y coordinate of the next vertex. Add these."
    // "Multiply the y coordinate of each vertex by the x coordinate of the next vertex. Add these."
    //
    double sum1=0.0, sum2=0.0;
    unsigned n= points.size();

    for( unsigned i=0; i<n; ++i ) {
        const Point &p1= points[i];
        const Point &p2= points[(i+1)%n];
        sum1 += p1.x * p2.y;
        sum2 += p1.y * p2.x;
    }

    // Subtract the sums, divide by two and ignore the sign (indicates whether the contour went
    // clockwise or counter-clockwise).
    //
    return abs( (sum1-sum2)/2.0 );
}


/*
* Set the 'offset' field of particular contour so it can be easily indexed from Lua, with added
* assurance such that:
*
*   - if the contour is closed (no edges), any offset will do (==0)
*
*   - if there is at least one edge, '.edge' of the 'offset' point is 'true' and the next point's
*     'edge' is 'false'. In other words, the 'offset' point is the first point of a strokable
*     non-edge section.
*/
void Contour::set_offset() {

    unsigned n= points.size();
    bool has_edge= false;

    // Check all points until we find a suitable position (or figure that it's a closed contour)
    //
    for( unsigned i=0; i<n; i++ ) {

        if (points[i].edge && (! points[ (i+1)%n ].edge)) {
            offset= i;  // suitable (start of section at 'i')
            goto DONE;
        }
        has_edge |= points[i].edge;
    }
    
    // Note: For some cases we get contours that are all-edge. This may even be beneficial, i.e.
    //      to fill areas from "down to level X" or "level X to up". So let them pass.
    //
#if 1
    (void)has_edge;     // ignored
#else
    if (has_edge) {
        throw E_LOG_BUG0( "Contour is all edge." );     // should not be
    }
#endif

    offset= 0;  // anything is fine (closed contour or all edge)

    // Re-test the invariant, now that we've set 'offset'
    //
DONE:
#ifndef NDEBUG
    n = 0; // INVARIANT();
#endif
;
}


//
// 21-Nov-2011 PKi: Contour labeling code taken from Q2 CairoContours
//

/*
* Translate pixel size to Cairo coordinates
*
* Note that 'cairo_device_to_user_distance()' will give negative values
* if the translation matrix is negated (and it is, for PNG and SVG).
*/
static double pixel_to_data_dist( cairo_t *cr, double pixel_size ) {
	double dx,dy;
	dx= dy= pixel_size;
	cairo_device_to_user_distance( cr,&dx,&dy );

	return (fabs(dx)+fabs(dy))/2.0;
}

#define R(argb) ContourInfo_common::R(argb)
#define G(argb) ContourInfo_common::G(argb)
#define B(argb) ContourInfo_common::B(argb)
#define A(argb) ContourInfo_common::A(argb)

// Margins (in pixels) around label text
//
const float LABEL_MARGIN_X= 5;
const float LABEL_MARGIN_Y= 5;

/*
* Label positioning (used for both priting & clipping)
*
* Projections:
*   When called, a data (user) projection is in effect.
*   When returned, a 1:1 pixel projection (origin at center of text and
*   rotation handled)
*
* Caller needs to use 'cairo_save/restore()' to restore the original conditions.
*/
static
void label_pos( cairo_t *cr,
                const Vectors::Point &p,
                const string &s,
                double tilt_rad,
                const ContourInfo_Line::LabelParams &label_specs,
                cairo_text_extents_t &te ) {

    double x=p.x, y=p.y;
    cairo_user_to_device( cr, &x,&y );

    // NOTE: Most of the steps below need to be performed in EXACTLY THIS
    //      order. Be careful if changing anything here (actually; DON'T!)

    cairo_identity_matrix(cr);     // resets to identity matrix

    // Font size is in target pixels (and we're 1:1); needs to be done
    // before getting text extents.
    //
    assert( label_specs.font_size_ > 0.0 );
    cairo_set_font_size( cr, label_specs.font_size_ );

    // Get text sizes when 1:1 mapping, but before the panning
    //
    cairo_text_extents( cr, s.c_str(), &te );

    // Only now can we pan the origin to where the label will be.
    //
    cairo_translate(cr,x,y);

    if (tilt_rad!=0.0)
        cairo_rotate( cr, -tilt_rad );
}

/*
* Label printing
*/
static
void label( cairo_t *cr,
            const ContourInfo_Line::LabelParams &label_specs,
            const Vectors::Point &p, const string &s, double tilt_rad ) {

    cairo_save(cr);
        {
        cairo_text_extents_t te;
        label_pos( cr, p, s, tilt_rad, label_specs, te );

        // Box behind the label (if any)
        //
        float w= te.width + 2*LABEL_MARGIN_X;     // give some extra (a bigger bounding box than just the text;
        float h= te.height + 2*LABEL_MARGIN_Y;    // only matters if the font filling is non-transparent)

        cairo_rectangle( cr, -w/2.0, -h/2.0, w,h );

        ContourInfo_common::uint_ARGB col= label_specs.box_fill_color;
        cairo_set_source_rgba( cr, R(col), G(col), B(col), A(col) );

        cairo_fill_preserve(cr);

        col= label_specs.box_stroke_color;
        cairo_set_source_rgba( cr, R(col), G(col), B(col), A(col) );

        cairo_set_line_width( cr, label_specs.box_stroke_width );
        cairo_stroke(cr);

        // Actual text
        //
        col= label_specs.font_color;
        cairo_set_source_rgba( cr, R(col), G(col), B(col), A(col) );

        cairo_move_to( cr, -(te.width/2.0), te.height/2.0 );
        cairo_show_text( cr, s.c_str() );
        }
    cairo_restore(cr);
}

/*
* Clip the label positions so that subsequent Cairo output won't affect them
* (protects them from line drawing, but keeps the already applied fill patterns
* intact).
*/
static
void label_clip( cairo_t *cr, const Labelizer::TiltedRect &tr ) {

    // The rectangle may be tilted so we don't want to use 'cairo_rectangle'
    //
    const Vectors::Point &p0= tr.get_corner(0);
    cairo_move_to( cr, p0.x, p0.y );

    for( unsigned i=1; i<=4; i++ ) {
        const Vectors::Point &p= tr.get_corner(i%4);
        cairo_line_to( cr, p.x, p.y );
    }

    // Collect all clipping paths together; only then (caller does) 'cairo_clip()'
}

/*
* The drawing object is required already here, to store label styles per
* each level used. This is a slight hack, it would be easier to allow only
* one label type per _all_ labels. But this is how the current specification
* goes.
*/
class MyDrawer : public Labelizer::Drawer {
  private:
    cairo_t *cr;
    unsigned w,h;   // 'cr' can be used for getting canvas dimensions only if
                    // it's image surface (PNG); not for SVG.

  public:
    MyDrawer( cairo_t *cr_, unsigned w_, unsigned h_ ) : Labelizer::Drawer(), cr(cr_), w(w_), h(h_) {}
    ~MyDrawer() {}

    void label( const Vectors::Point &p, const std::string &label_text,
        const ContourInfo_Line::LabelParams &label_specs,
        double tilt_rad ) {

// DEBUG: show exactly where the label is meant to be
#if 0
        double x= p.x, y=p.y;
        cairo_stroke(cr);
        cairo_save(cr);
        {
            cairo_set_source_rgb( cr, 0,0,0 );
            cairo_arc( cr, x,y, pixel_to_data_dist(cr,2.5), 0,2*M_PI );
            cairo_stroke(cr);

            cairo_set_source_rgba( cr, 0.7,0,0, 0.4 );  // transparent red
            cairo_arc( cr, x,y, pixel_to_data_dist(cr,5), 0.0, tilt_rad );
            cairo_stroke(cr);
        }
        cairo_restore(cr);
#endif

    ::label( cr, label_specs, p, label_text, tilt_rad );
    }

    /*
    * Common color selection for all 'mark_..()' debugging functions.
    */
    static void set_color_by_name( cairo_t *cr, const char *str, double alpha=0.2 ) {

        if (str) {
            double r=0.0, g=0.0, b=0.0;

                 if (strcmp(str,"red")==0) r=1.0;
            else if (strcmp(str,"blue")==0) b=1.0;
            else if (strcmp(str,"yellow")==0) r=g=1.0;
            else if (strcmp(str,"magenta")==0) r=b=1.0;
            else if (strcmp(str,"gray")==0) { /*nothing*/; }

            cairo_set_source_rgba( cr, r,g,b, alpha );
        }
    }

    /*
    * Marks for debugging (optional)
    *
    * NOTE: This debugging feature (to get stuff on the actual output
    *       graphics instead of log) is ABSOLUTELY NECESSARY for any
    *       meaningful debugging. NEVER EVER REMOVE THIS!  --AKa 8-Jan-2008
    */
#if 0
    void mark_candidate( const Vectors::Point &p, double goodness, const char *str ) {
        double x= p.x, y=p.y;

        stringstream ss; ss<<goodness;

        cairo_save(cr);
        {
            set_color_by_name(cr,str,1.0 /*no alpha*/);

            const double SIZE= pixel_to_data_dist(cr,2.0);
            const double THICKNESS= pixel_to_data_dist(cr,2.0);
            cairo_set_line_width( cr, THICKNESS );

            cairo_arc( cr, x,y, SIZE, 0.0, 2*M_PI );
            cairo_stroke(cr);
# if 0
            const double FONT_SIZE= pixel_to_data_dist(cr,10.0);
            cairo_set_source_rgb( cr, r,g,b );  // no alpha for text

            cairo_move_to( cr, x+2*SIZE, y+2*SIZE );
            cairo_set_font_size( cr, FONT_SIZE );
            cairo_show_text( cr, ss.str().c_str() );

            //cairo_stroke(cr);
# endif
        }
        cairo_restore(cr);
    }
#endif

    /*
    * Also this function is worth every line in Gold. Enable to see label
    * rectangles and their bounding boxes. (debugging only)
    */
#if 0
    void mark_tr( const Labelizer::TiltedRect &tr, const char *str ) {

    	Vectors::Point p[4];
        for( unsigned i=0; i<4; i++ ) {
            p[i]= tr.get_corner(i);
        }

        cairo_save(cr);
        {
            set_color_by_name(cr,str);

            const double SIZE= pixel_to_data_dist(cr,3.0);
            cairo_set_line_width( cr, SIZE );

            const double SIZE2= pixel_to_data_dist(cr,2.0);
            Vectors::Point center= tr.get_center();
            cairo_arc( cr, center.x,center.y, SIZE2, 0.0, 2*M_PI );

            // The rectangle may be tilted so we don't want to use 'cairo_rectangle'
            //
            cairo_move_to( cr, p[0].x, p[0].y );

            for( unsigned i=0; i<4; i++ ) {
                const Vectors::Point &pp= p[(i+1)%4];
                cairo_line_to( cr, pp.x, pp.y );
            }
            cairo_stroke(cr);

            mark_bb( tr.get_bounding_box(), "blue" );
        }
        cairo_restore(cr);
    }
#endif


    /*
    * Show intersecting vectors
    */
#if 0
    void mark_vector( const Vectors::BointAndVector &v, const char *str ) {

        cairo_save(cr);
        {
            set_color_by_name(cr,str);

            const double SIZE= pixel_to_data_dist(cr,3.0);
            cairo_set_line_width( cr, SIZE );

            cairo_arc( cr, v.p.x, v.p.y, SIZE*4, 0, 2*M_PI );
            cairo_stroke(cr);

            cairo_move_to( cr, v.p.x, v.p.y );
            cairo_rel_line_to( cr, v.get_dx(), v.get_dy() );
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    }
#endif

    /*
    * Show a bounding box
    */
#if 0
    void mark_bb( const Vectors::BoundingBox &bb, const char *str ) {

        cairo_save(cr);
        {
            set_color_by_name(cr,str);

            const double SIZE= pixel_to_data_dist(cr,3.0);
            cairo_set_line_width( cr, SIZE );

            cairo_rectangle( cr, bb.lo.x, bb.lo.y, bb.width(), bb.height() );
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    }
#endif
};

/*
* Provide the size of a label, in data coordinates
*
* State:
*   'cr' is in the data coordinate system when this function is called.
*/
static Vectors::Vector label_size( cairo_t *cr, float font_size, const string &label ) {

    double w,h;

    cairo_save(cr);
    {
        cairo_identity_matrix(cr);
        cairo_set_font_size( cr, font_size );

        cairo_text_extents_t te;
        cairo_text_extents( cr, label.c_str(), &te );

        w= te.width + 2*LABEL_MARGIN_X;   // in pixels
        h= te.height + 2*LABEL_MARGIN_Y;
    }
    cairo_restore(cr);

    cairo_device_to_user_distance( cr, &w, &h );
        //
        // Now, they're in data coordinates

    if (h<0) h *= -1.0;     // compensate for negative Y mapping (SVG, PNG)

    return Vectors::Vector(w,h);
}

// 02-Dec-2011 PKi: Draw contour
//
void Contour::draw_path( cairo_t *cr, bool line ) const
{
	bool first = true;
	bool within_gap= false;    // if true, next operation is 'move_to()'
	bool had_gap= false;       // if false, last point gets closed to the first

    // 02-Mar-2015 PKi: Offset must be used for proper egde handling (must start stroking from the start of a strokable section)
    //
	unsigned n = points.size();
	assert( (offset != ((unsigned)-1)) && (offset < n) );	// 'set_offset()' must have been called
	vector<EdgePoint>::const_iterator eit = points.begin();
	advance(eit,offset);
	vector<EdgePoint>::const_iterator last = eit;

	for( ; (n > 0); eit++, n-- )
	{
		if (eit == points.end())	// Wrap around to the first point (__index(); points[ (my.offset+(k-1)) %n_ ] );
			eit = points.begin();	//

		if (first)
		{
			cairo_move_to( cr, eit->x, eit->y );
			first = false;
		}
		else if (line && last->edge && eit->edge)
		{
			within_gap= true;
			had_gap= true;
		}
		else
		{
			if (within_gap)
			{
				cairo_move_to( cr, last->x, last->y );   // start of new non-edge section
				within_gap= false;
			}

			cairo_line_to( cr, eit->x, eit->y );
		}

		last = eit;
	}

	if (! had_gap)
		cairo_close_path(cr);

    return;
}

// 21-Nov-2011 PKi: Storage for contours and stroke attributes
//
/*
* Had this first as a local within 'CairoContours()' function, but it did
* not get pass compiler (unsure why, cryptical errors). If it's here, it does.
*   --AKa 2-Feb-2009
*/
struct LineStore {
	ContourInfo_common::StrokeParams sp;
	const Contour *curves;

	LineStore( const ContourInfo_common::StrokeParams &sp_, const Contour *curves_ )
		: sp(sp_), curves(curves_) {}
};

/* 21-Nov-2011 PKi: Draw, fill and labelize contours
*
* drawcontours( cairocontext, gridsize_ud, { labelcfg }, { contourdefs }, { contourvalues }, { contour_ud } )
*/
int Contour::drawcontours( lua_State *L )
{
	try {
		proto( L, "CairoContext, MatrixPos, { } , { string }, { [ number ] }, { { Contour } }" );

		cairo_t *cr= NULL;
		MatrixPos *gs= NULL;

		const int cr_idx = 1,gs_idx = 2,lcfg_idx = 3,cdef_idx = 4,cval_idx = 5,contour_idx = 6;

		if (
			(lua_gettop(L) == contour_idx) && (lua_type(L,lcfg_idx) == LUA_TTABLE) &&
			(lua_type(L,cdef_idx) == LUA_TTABLE) && (lua_type(L,cval_idx) == LUA_TTABLE) &&
			(lua_type(L,contour_idx) == LUA_TTABLE)
		   )
		{
			Context *cx = Context::instance(L,cr_idx);

			// 09-Feb-2012 PKi: Context::instance() fails if not statically linked with newcairo;
			//                  just casting ...
			if (! cx) {
				void *ud = lua_touserdata(L,cr_idx);
				cx = ((Context *) (ud ? ud : NULL));
			}

			cr = (cx ? ((cairo_t *) *cx) : NULL);

			gs= MatrixPos::instance(L,gs_idx);
		}
		else
			luaL_error( L, "Invalid args; usage: drawcontours( cr, gridsize_ud, { contourdefs }, { contourvalues }, { contour_ud } )" );

		if ((! cr) || (! gs))
			luaL_error( L, "Invalid %s; usage: drawcontours( cr, gridsize_ud, { contourdefs }, { contourvalues }, { contour_ud } )",
						cr ? "gridsize" : "cairocontext"
					  );

		// Get contour descriptor strings
		//
		typedef boost::shared_ptr<const ContourInfo_common> spcc;
		vector<spcc> ccVec;

		typedef map<unsigned int,string> cdefmap;
		typedef pair<unsigned int,string> cdefpair;
		cdefmap contourdefs;

		unsigned int idx;
		string s;

		L_GROW(3);
		lua_pushnil(L);     // first key
		while (lua_next(L,cdef_idx)) {
			// [-2]: key
			// [-1]: value

			if (! lua_isnumber(L,-2))
				luaL_error( L, "arg %d: numeric contour descriptor index expected, got %s", cdef_idx, lua_typename(L, lua_type(L, -2)));

			idx = (unsigned int) lua_tonumber(L,-2);

			if (lua_isnil(L,-1) || (! lua_isstring(L,-1)))
				luaL_error( L, "arg %d: contour descriptor expected, got %s", cdef_idx, lua_typename(L, lua_type(L, -1)));

			s = lua_tostring(L,-1);

			contourdefs.insert(cdefpair(idx,s));

			lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
		}

		// Get contour values (labels)
		//
		vector<float> labelval;
		float v;
		unsigned int nv = 0;

		L_GROW(3);
		lua_pushnil(L);     // first key
		while (lua_next(L,cval_idx)) {
			// [-2]: key (1..len)
			// [-1]: value

			if (! lua_isnumber(L,-1))
				luaL_error( L, "arg %d: contour value expected, got %s", cval_idx, lua_typename(L, lua_type(L, -1)));
			else
			{
				v = lua_tonumber(L,-1);

				// 11-Jan-2012 PKi: Value is nan if no label (label color is 'none')
				//
				if (! isnan(v))
				    nv++;
			}

			labelval.push_back(v);

			lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
		}

		bool labelize = (nv > 0),clip_labels = false;

		// Configure Labelizer (strategy for positioning labels, label color and size etc.)
		//
		MyDrawer my_drawer( cr, 0, 0 );
		Labelizer::Config lCfg;
		Labelizer lzer( Labelizer::loadUserCfg(L,lcfg_idx,lCfg), my_drawer, gs->getX()-1, gs->getY()-1 );

		// If drawing contours, store them with stroke attributes for drawing and
	    // (if labeling) add contour paths and labels to Labelizer.
		//
	    // If filling, set fill attributes for each pair of lo-hi range contours.

		Contour *c = NULL,*pc = NULL;
	    vector<LineStore> line_store;

		cdefmap::const_iterator pit = contourdefs.end();
		const ContourInfo_common *cc = NULL;
		unsigned int nc,pnc = 0;
		bool first = true,fillOpen = false;
		bool labelizethis = labelize;

		ostringstream oss;
		oss.precision(1);
		string label;

		L_GROW(3);
		lua_pushnil(L);     // first key
		while (lua_next(L,contour_idx)) {
			// [-2]: key (1..len)
			// [-1]: value { Contour }

			if (! lua_isnumber(L,-2))
				luaL_error( L, "arg %d: numeric contour index expected, got %s", contour_idx, lua_typename(L, lua_type(L, -2)));

			nc = (unsigned int) lua_tonumber(L,-2);

			if (lua_type(L,-1) != LUA_TTABLE)
				luaL_error( L, "arg %d: table of contours expected, got %s", contour_idx, lua_typename(L, lua_type(L, -1)));

			if (labelize)
			{
				if (nc <= labelval.size())
				{
					v = labelval[nc-1];

					// 11-Jan-2012 PKi: If nan just do not labelize; go on to handle the contour
					//
//					if (isnan(v))
//					{
//						lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
//						continue;
//					}
					if ((labelizethis = (! isnan(v))))
					{
						oss.str("");
						oss.clear();
						oss << fixed << boost::format(string("%.") + boost::lexical_cast<string>(min(max(lCfg.decimals,0),6)) + "f") % v;
						label = oss.str();
					}
					else
						label.clear();
				}
				else
				{
					// No label; should not happen

					lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
					continue;
				}
			}

			// Get contour's descriptor string (having largest key <= contour's index)
			//
			// If contour index >= max. contour descriptor index, use the last descriptor without searching

			cdefmap::const_iterator cit = contourdefs.end();
			cit--;

			if (nc < cit->first)
			{
				// If contour index >= previous index, start searching from prev. iterator

				pnc++;

				for ( cdefmap::const_iterator it= (cc && (nc >= pnc)) ? pit : contourdefs.begin(); it != contourdefs.end(); it++ )
					if (it->first > nc)
						break;
					else
						cit = it;
			}

			pnc = nc;

			if ((! cc) || (cit != pit))
			{
				// First contour or contour set changes; create ContourInfo_common
				//
				// 1 					step 					zero_level(not used) 	lo 				hi
				// 2 					Nlevels					level1					level2			levelN
				// stroke_color 		fill_color 				stroke_width 			smooth_factor
				// label_strategy 		label_font_height 		label_text_color
				// label_box_fill_color label_box_stroke_color	label_box_stroke_width
				//
				// If previous contour set was for fill, complete it

				if (fillOpen) {
					cairo_fill(cr);
					fillOpen = false;
				}

				cc = ContourInfo_common::create(cit->second);
				ccVec.push_back(spcc(cc));

				pit = cit;

				pc = NULL;	// To create new ContourInfo_Fill for next fill descriptor
			}

	        if (cc->line_type()) {
				const ContourInfo_Line &ci = static_cast<const ContourInfo_Line&>( *cc );

				// Default font size is height/30 for historical reasons
				//
				double font_size= ci.label.font_size_;

				if (font_size<=0.0)
				{
					cairo_surface_t *cs = cairo_get_target(cr);
					font_size= (cs ? (cairo_image_surface_get_height(cs) / 30.0) : 20.0);
				}

				Labelizer::FontSpec fs( label, label_size( cr, font_size, label ), &ci.label );

				L_GROW(3);
				lua_pushnil(L);     // first key
				while (lua_next(L, -2)) {
					c = Contour::instance(L,-1);

					if (! c)
						luaL_error( L, "arg %d: contour expected, got %s", contour_idx, lua_typename(L, lua_type(L, -1)));

					if (labelizethis)
					{
						bool fst = true;

						for( vector<EdgePoint>::iterator it = c->points.begin(); (it != c->points.end()); it++)
						{
							Vectors::Point p(it->x,it->y);

							if (fst)
							{
								lzer.add_point(p,fs);
								fst = false;
							}
							else
								lzer.add_point(p);
						}

						lzer.close();

						if ((ci.label.box_fill_color & 0xff000000) == 0xff000000) {
							// Full alpha: completely transparent (= "none")
							//
							clip_labels= true;
						}
					}

					line_store.push_back( LineStore(ci.stroke,c) );

					lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
				}
	        }
	        else {
				if (! pc) {
					const ContourInfo_Fill &ci= static_cast<const ContourInfo_Fill&>( *cc );

					// 24-Jan-2014 PKi: Set pattern transformation matrix's scale
					//
					double ux = 1.0, uy = 1.0;
					cairo_device_to_user_distance (cr, &ux, &uy);

					cairo_matrix_t matrix;
					cairo_pattern_get_matrix(ci.pattern,&matrix);
					cairo_matrix_init_scale(&matrix,ci.patternSize() * (0.125/ux),ci.patternSize() * (0.125/uy));
					cairo_pattern_set_matrix(ci.pattern,&matrix);

					cairo_set_source( cr, ci.pattern );

					if (first)
					{
						cairo_set_fill_rule( cr, CAIRO_FILL_RULE_EVEN_ODD );
						first = false;
					}
				}

				L_GROW(3);
				lua_pushnil(L);     // first key
				while (lua_next(L, -2)) {
					c = Contour::instance(L,-1);

					if (! c)
						luaL_error( L, "arg %d: contour expected, got %s", contour_idx, lua_typename(L, lua_type(L, -1)));

                    c->draw_path(cr, false);

        			lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
				}

				if (fillOpen || c->range()) {
					cairo_fill(cr);
					fillOpen = false;
				}
				else {
					pc = c;
					fillOpen = true;
				}
	        }

	        lua_pop(L,1);   // remove value, keep key (for 'lua_next()')
		}

		// If last contour set was for fill, complete it

		if (fillOpen)
			cairo_fill(cr);

		if (labelize)
		{
			vector<Labelizer::TiltedRect> label_trs= lzer.done(pixel_to_data_dist( cr, 100) / 100);

			/*
			* Possible clipping before drawing lines
			*/
			if (clip_labels) {

				// Clip label boxes (don't allow drawing curves on them)
				//
				for( vector<Labelizer::TiltedRect>::const_iterator it= label_trs.begin();
					it != label_trs.end();
					++it ) {
					label_clip( cr, *it );
				}

				// Paths need to be inverted to clip out the label areas. Also filling
				// the whole target area will do the same.
				//
				cairo_rectangle( cr, 0,0, gs->getX(),gs->getY() );

				cairo_clip(cr);     // clip them all at once
			}
		}

	    // Draw lines

	    const ContourInfo_common::StrokeParams *sp_last= 0;

	    for( vector<LineStore>::const_iterator it= line_store.begin();
	        it != line_store.end();
	        ++it ) {
	        const ContourInfo_common::StrokeParams &sp= it->sp;
	        const Contour *curves = it->curves;

	        if ((!sp_last) || (*sp_last != sp)) {
	            if (sp_last) {
	                cairo_stroke(cr);   // Output earlier lines
	            }

	            /*
	            * Prepare Cairo line type etc. (line width in device pixels)
	            */
	            cairo_set_line_width( cr, pixel_to_data_dist(cr, sp.width) );

	            ContourInfo_common::uint_ARGB col= sp.color;
	            cairo_set_source_rgba( cr, R(col), G(col), B(col), A(col) );

	            // 'specs.stroke.dash' is a vector of unsigned; Cairo uses double
	            //
	            unsigned dash_n= sp.dash.size();

	            if (dash_n) {
	                double *arr= new double[dash_n];
	                {
	                for( unsigned i=0; i<dash_n; i++ ) {
	                    unsigned a= sp.dash[i];
	                        //
	                        // 0: one pixel pause
	                        // >=1: that many pixels on/off

	                    arr[i]= a==0 ? 1:a;
	                }
	                cairo_set_dash( cr, arr, dash_n, 0.0 );
	                }
	                delete[] arr;
	            }
	            else if (cairo_get_dash_count(cr) > 0)
	            {
		            // 02-Jan-2012 PKi: No dash, disable dashing
		            //

	                cairo_set_dash( cr, NULL, 0, 0.0 );
	            }

	            sp_last= &it->sp;
	        }

	        // Add latest curve to Cairo path (not drawing, yet)
	        //

	        curves->draw_path(cr);
	    }
	    cairo_stroke(cr);   // Output last curve type

		// Draw the labels
		//
		if (labelize)
		{
			cairo_reset_clip(cr);
			lzer.draw_labels( my_drawer );
		}
    }
    catch (const exception &ex)
    {
    	// Invalid contour descriptor

	    luaL_error( L, ex.what() );
    }

    return 0;
}
