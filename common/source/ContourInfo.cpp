/*
* ContourInfo.cpp
*
* Building a 'ContourInfo' struct out of contour specification strings,
* whose format dates back to earlier Q2Server usage. 
*
* Note:
*   Error checking of URL parameters is intensionally left rather loose.
*   The real way to do this kind of stuff should be by regular expressions,
*   in a scripting language. Plans for such are discussed, so it's not worth
*   brushing this too shiny.    -- AKa 7-Nov-2008
*/

#include "ContourInfo.h"

#include "newbase/NFmiStringTools.h"

// 29-Dec-2011 PKi: LogTools.h instead of QDLog.h
//
//#include "QDLog.h"
#include "LogTools.h"

#include <boost/lexical_cast.hpp>

#include <sstream>

#include <string.h>
#include <assert.h>
#include <math.h>

using namespace std;

/*
* Pass on to either 'ContourInfo_Line' or 'ContourInfo_Fill', based on the
* type of the 'conf' string (first character 1..4).
*
* Note: caller must 'delete' the pointer when used.
*/
const ContourInfo_common *ContourInfo_common::create( const std::string &conf, const ContourMatrix<float>& data ) {
    switch( conf.c_str()[0] ) {
        case '1': case '2':
            return ContourInfo_Line::create( conf, data );
        case '3': case '4':
            return ContourInfo_Fill::create( conf, data );
        default:
		  {
			LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
            throw runtime_error("Bad contour spec: "+conf);
		  }
    }
}


const ContourInfo_common *ContourInfo_common::create( const std::string &conf ) {
    ContourMatrix<float> data;
    return create( conf, data );
}


/*
*/
static int /*-1/0/+1*/ my_strincmp( const char *a, const char *b, size_t n ) {
    assert( a && b );
    for( size_t i=0; i<n; i++ ) {
        char la= tolower(a[i]);
        char lb= tolower(b[i]);

        if (!la) return lb ? -1:0;
        if (la<lb) return -1;
        if (la>lb) return +1;
    }
    return 0;   // 'n' first letters matched
}


/*
* Take out dash description within '[' ']' brackets (if any).
*
* 's' is a color string that may contain "[<on uint>,<off uint>,...]"
* 
* Returns: string except the dash part
*
* NAG: this kind of things are for regexps (oh, C++ did not have any...!)
*/
static string StripDash( const string &s, vector<unsigned> &dash )
{
    string::size_type i= s.find('[');
	string::size_type j= s.rfind(']');

	if ((i != string::npos) && (j != string::npos) && j>i) {
		string dash_s = string(s, i+1, j-i-1);
		
		// 'dash_s': <on_uint>,<off_uint>,...
		//
		const char *p= dash_s.c_str();
		
		while(true) {
            char *endptr;
            unsigned int v= strtol( p, &endptr, 10 );
            dash.push_back(v);
            
            if (*endptr=='\0') break;   // was last entry
            if (*endptr!=',')
			  {
				LOG_USAGE( "Bad dash string: '[%s]'", dash_s.c_str() );
                throw runtime_error("Bad dash string: '["+dash_s+"]'");
			  }

            p= endptr+1;
		}
		return s.substr(0, i);
    }
    return s;
}


/*
* Input: "rgba(R,G,B,A)"    color with opacity
*        "rgb(R,G,B[,A])[i,j]"  color with dash array
*        "<color name>"         color by name
*        "<color name>[i,j]"    color by name with dash array
*
* Picks the color, opacity/hatch value and possible dash string from the color
* descriptor.
*
* Sets: 'dash' to the dash array part ("" if none)
*
* Note: RGB and opaqueness values can be either "0"-"255" or "0%"-"100%".
*       At least opaqueness can also be decimal (s.a. "0.5").
*       No spaces allowed within the input string.
*/
static ContourInfo_common::uint_ARGB SetColor( const string &s, vector<unsigned> &dash )
{
	string s_no_dash = StripDash( s, dash );

    // Pre-known special values
    //
    if (s_no_dash=="none")  return 0xff000000;   // completely transparent
    if (s_no_dash=="red")   return ContourInfo_common::RED;
    if (s_no_dash=="green") return ContourInfo_common::GREEN;
    if (s_no_dash=="blue")  return ContourInfo_common::BLUE;
    if (s_no_dash=="yellow")  return ContourInfo_common::YELLOW;
    if (s_no_dash=="white")  return ContourInfo_common::WHITE;
    if (s_no_dash=="black")  return ContourInfo_common::BLACK;

    //---
    // Note: Q2 specs show "purple" as a preset color name, but there are 147
    //       named SVG colors (<http://www.december.com/html/spec/colorsvg.html>).
    // 
    // It was easy to support them all when crafting raw SVG. Surely we're not
    // trying to do that now, that we're using Cairo?
    //---
    
    // 's_no_dash' is supposed to be "rgb[a](...)". We don't really check
    // the prefix; just take stuff within paranthesis. If it's three, it's
    // RGB, if it's four it's RGBA.

    // AKa 17-Oct-2008: Heh, C++ stdlib has no case insensitive compare.
    //          Really, what to do with the language, anyways.
    //
    // MH: 4-Nov-2008: boost::algorithm defines iequals + many other
    //                 string specific algortihms

    //
    if (my_strincmp( s_no_dash.c_str(), "rgb", 3 ) != 0) {
	  LOG_USAGE( "Bad color string: %s", s.c_str() );
	  throw runtime_error("Bad color string: "+s);
    }

    string::size_type i = s_no_dash.find('(');
	string::size_type j = s_no_dash.rfind(')');

    if (i==string::npos || j==string::npos)
	  {
		LOG_USAGE( "bad color string: %s", s_no_dash.c_str() );
        throw runtime_error("Bad color string: "+s_no_dash);
	  }

    ContourInfo_common::uint_ARGB color= 0;

    string s2= s_no_dash.substr(i+1,j-i-1) + ",";
    string::size_type k=0;  // beginning of number at 's2'

    for (unsigned n=0; n<4; n++) {
        string::size_type k_was= k;
        if (k!=string::npos) {
            k= s2.find(',',k);     // next ','
            if (k!=string::npos) ++k;
        }

        if (k==string::npos) {
            if (n<3) color<<=8;     // tail is defaults (0)
        } else {
            string v= s2.substr(k_was,k-k_was-1);  // "0".."255" or "0%".."100%"
            
            char *endptr;
            double d= strtod( v.c_str(), &endptr );

            if (*endptr=='%') {
                d /= 100.0;
            } else if (*endptr) {
			  LOG_WARNING( "Bad rgb[a] value: %s (ignored)", v.c_str() );
			  throw runtime_error("Bad rgb[a] value: "+v+" (ignored)");
            } else if ((d>1) || (v=="1")) {   // 1.0 is taken as full color
                d /= 255.0;
            }

            if (d<0.0 || d>1.0) {
                LOG_WARNING( "Bad rgb[a] value: %s (out of range)", v.c_str() );
                d= d<0 ? 0.0:1.0;
            }
            unsigned vi= (unsigned)floor( d*255.0 + 0.5 );

            if (n<3) color= (color<<8) | vi;
            else color |= (255-vi)<<24;     // save A as transparency (255-opaqueness)
        }
    }
    return color;
}

static ContourInfo_common::uint_ARGB SetColor( const string &s ) {
    vector<unsigned> dash_dummy;
    return SetColor( s, dash_dummy );
}


/*
* Input: "rgbh(R,G,B[,A],h_index)"
*        other input delegated to 'SetColor'
*
* Based on Vili's request, one can give also alpha as part of 'rgbh()'.
* Traditionally it was not there.   --AKa 3-Feb-2009
*/
static ContourInfo_common::uint_ARGB SetColorAndHatch( const string &s_, int &hatch_index )
{
    string s_rgb;

	if (my_strincmp( s_.c_str(), "rgbh", 4) != 0) {
        hatch_index= -1;
        s_rgb= s_;

    } else {
        // Take the last number (1..405) apart, and leave the rest to 
        // 'SetColor()' as "rgb(...)" string.
        //
        string::size_type i = s_.rfind(',');
        string::size_type j = s_.rfind(')');

        if (i==string::npos || j==string::npos)
		  {
			LOG_USAGE( "Bad color string: %s", s_.c_str() );
            throw runtime_error("Bad color string: "+s_);
		  }

        string hatch= s_.substr(i+1,j-i-1);
        char *endptr;
        hatch_index= strtol( hatch.c_str(), &endptr, 10 );
                    
        if (*endptr)
		  {
			LOG_USAGE( "Bad rgbh value: %s", hatch.c_str() );
            throw runtime_error("Bad rgbh value: "+ hatch);
		  }

        s_rgb= string("rgb")+s_.substr(4 /*skip "rgbh"*/,i-4)+")";
    }
    return SetColor(s_rgb);
}


/*
* Common stroke params
*/
ContourInfo_common::StrokeParams::StrokeParams(
            const string &color_s,
            const string &width_s,
            const string &smooth_s ) {

    color= (color_s=="def" || color_s=="") ? BLACK : SetColor( color_s, dash );
    width= (width_s=="def" || width_s=="") ? 1 : NFmiStringTools::Convert<float>(width_s.c_str());
    smooth= (smooth_s=="def" || smooth_s=="") ? 0.0F : NFmiStringTools::Convert<float>(smooth_s.c_str());
    if (smooth<0.0F) smooth=0.0F;
}


/*
* Are two StrokeParams identical?
*/
bool ContourInfo_common::StrokeParams::operator!=( const ContourInfo_common::StrokeParams &o ) const {
    if ((color==o.color) && (width==o.width) && (smooth==o.smooth)) {
        unsigned dash_n= dash.size();
        if (dash_n == o.dash.size()) {
            for( unsigned i=0; i<dash.size(); i++ ) {
                if (dash[i] != o.dash[i])
                    return true;    // difference in dash pattern
            }
            return false;   // they are equal
        }
    }
    return true;    // different
}


/*
* Label params
*/
ContourInfo_Line::LabelParams::LabelParams(
            const string &strategy_s,
            const string &font_size_s,
            const string &font_color_s,
            const string &box_fill_color_s,
            const string &box_stroke_color_s,
            const string &box_stroke_width_s ) {

    strategy= ((strategy_s=="def" || strategy_s=="") ? 0 : NFmiStringTools::Convert<unsigned>(strategy_s.c_str()));
    font_size_= (font_size_s=="def" || font_size_s=="" || strategy_s=="") ? 0 : NFmiStringTools::Convert<float>(font_size_s.c_str());
    font_color= (font_color_s=="def" || font_color_s=="" || strategy_s=="") ? BLACK : SetColor( font_color_s );
    box_fill_color= (box_fill_color_s=="def" || box_fill_color_s=="" || strategy_s=="") ? YELLOW : SetColor( box_fill_color_s );
    box_stroke_color= (box_stroke_color_s=="def" || box_stroke_color_s=="" || strategy_s=="") ? BLUE : SetColor( box_stroke_color_s );
    box_stroke_width= (box_stroke_width_s=="def" || box_stroke_width_s=="" || strategy_s=="") ? 1 : NFmiStringTools::Convert<float>(box_stroke_width_s.c_str());
}


/*
* Fill/hatch params
*/
ContourInfo_Fill::FillParams::FillParams( const string &fill_color_s ) 
    : color(WHITE) 
    , hatch_width(0)    // no hatch pattern
    , hatch_offset(false)
    , hatch_type( (enum e_hatch_type) 0 )
{
    if (fill_color_s!="def") {
        int hatch_index;

        color= SetColorAndHatch( fill_color_s, hatch_index );
        if (hatch_index >= 0) {
            hatch_width= (float)( hatch_index<100 ? 1 : hatch_index/100 );
            hatch_offset= hatch_index/100 == 1;  // 100..105
            hatch_type= (enum e_hatch_type)( hatch_index%100 );
        }
    }
}


/*
* Find min/max levels based on 'data', aligned at 'step'
*/
static void find_limits( const ContourMatrix<float> &data, float step,
                        float *lo_ref, float *hi_ref ) {

    float min,max;

#ifdef USE_NFMIDATAMATRIX_FOR_CONTOURS
	min= max= data[0][0];
	for( unsigned int x= 0; x<data.NX(); x++ )
		for( unsigned int y=0; y<data.NY(); y++ )
		{
			float v= data[x][y];
			if (v<min) min=v;
			if (v>max) max=v;
		}
#else
    data.minmax( &min, &max );
#endif
            
    if (lo_ref) {
        // Round 'min' upwards to next 'step'
        //
        float mod= fmodf( fabsf(min), step );
        *lo_ref= (mod==0.0) ? min :
                 (min>0.0) ? min+(step-mod) : 
                            min+mod; 
    }
    if (hi_ref) {
        // Round 'max' downwards to next 'step'
        //
        float mod= fmodf( fabsf(max), step );
        *hi_ref= (mod==0.0) ? max :
                 (max>0.0) ? max-mod : 
                            max-(step-mod);
    }
}


/*
* Create an isoline Contour Info (types 1 & 2), from a descriptor string.
*
* The string format is backwards compatible with old Q2Server. However, some
* parts of it may not be implemented in the new Tron contouring.
*
* i.e. "1 5.0 0.0 900.0 1050.0 rgba(40,40,40,100) none 2  2.0 2 20 def none none 1"
*       ^-- function type
*         ^-- ...type specific
*                              ^-- color & opacity
*
* Note: 'data' is needed solely for finding min/max values of the data, if
*       "no limit" (32700) is given for the range.
*
*       'h' is the target output pixel height; it is needed solely for defining
*       the default font size (h/30).
*/
const ContourInfo_Line *ContourInfo_Line::create( const string &conf, const ContourMatrix<float>& data )
{
    vector<float> levels;
    stringstream ss(conf);

    int type;
	ss >> type;

    if (type==1) {
        // "1 step zero-level min max ..."
        //
        float step, zero_level, lo, hi;
        ss >> step >> zero_level >> lo >> hi;
        if (ss.bad())
		  {
			LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
            throw runtime_error("Bad contour spec: "+conf);
		  }

        if (step==0.0)
		  {
			LOG_USAGE0( "Bad step: 0" );
            throw runtime_error("Bad step: 0" );
		  }

        // 32700 is a special value for "no limit" = use -inf/+inf
        //
        if ((lo==32700) || (hi==32700)) {
            find_limits( data, step, (lo==32700) ? &lo:nullptr, (hi==32700) ? &hi:nullptr );
        }

        if (hi<lo)
		  {
			LOG_USAGE( "Bad contour spec (hi<lo): %s", conf.c_str() );
            throw runtime_error("Bad contour spec (hi<lo): "+conf);
		  }

        for( float v=lo; v<=hi; v+=step ) {
            levels.push_back(v);
        }
        (void)zero_level;   // not used

    } else if (type==2) {
        int count = 0;
        ss >> count;
        levels.resize(count);
        for (int i = 0; i<count; i++)
            ss >> levels[i];
        if (ss.bad())
		  {
			LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
            throw runtime_error("Bad contour spec: "+conf);
		  }
    } else {
	  LOG_USAGE( "Bad contour type %d (expected 1,2)", type );
	  throw runtime_error("Bad contour type "+boost::lexical_cast<string>(type)+" (expected 1,2)");
    }

    // i.e. "rgba(40,40,40,100) none 2 2.0 2 20 def none none 1"
    //
    string stroke_color, fill_color, stroke_width, smooth_factor, label_strategy,
           label_font_height, label_text_color, label_box_fill_color, label_box_stroke_color,
           label_box_stroke_width;
    
    ss >> stroke_color >> fill_color >> stroke_width >> smooth_factor
       >> label_strategy >> label_font_height >> label_text_color
       >> label_box_fill_color >> label_box_stroke_color
       >> label_box_stroke_width;
    if (ss.bad())
	  {
		LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
        throw runtime_error("Bad contour spec: "+conf);
	  }

    (void)fill_color;   // not used (by design)

    return new ContourInfo_Line( levels, 
                StrokeParams( stroke_color, stroke_width, smooth_factor ),
                LabelParams( label_strategy, label_font_height, label_text_color, 
                              label_box_fill_color, label_box_stroke_color,
                              label_box_stroke_width ) );
}


/*
* Prepare a Cairo surface pattern based on a hatch description.
*/
cairo_pattern_t *ContourInfo_Fill::FillParams::cairo_pattern_create(size_t size) const {

    double r=R(color), g=G(color), b=B(color), a=A(color);

    if ( hatch_width==0 ) {
        // Solid fill (possibly with transparency)
        //
		return cairo_pattern_create_rgba( r,g,b,a );
    }

    // For up/down patterns, we should keep the box size visually close to
    // the horizontal/vertical box size. Thus, scale by sqrt(2).
    //
    if ((hatch_type==HATCH_DOWN) || (hatch_type==HATCH_UP) ||
        (hatch_type==HATCH_DOWN_UP)) {
        size= (unsigned)( size*1.4 + 0.5 );
    }

    double mid_x= size/2.0;
    double mid_y= size/2.0;

    cairo_surface_t *surf= cairo_image_surface_create( CAIRO_FORMAT_ARGB32, size, size );

    cairo_t *cr= cairo_create( surf );

    cairo_set_source_rgba( cr, r,g,b,a );
    
    cairo_set_line_width( cr, hatch_width );
    cairo_set_line_cap( cr, CAIRO_LINE_CAP_SQUARE );

    if (hatch_offset) {
        double dx=0, dy=0;

        switch( hatch_type ) {
            case HATCH_HORIZONTAL:
            case HATCH_VERTICAL:
            case HATCH_HORIZONTAL_VERTICAL: 
                dx= dy= size/2.0; break;
            case HATCH_DOWN:
            case HATCH_UP:
            case HATCH_DOWN_UP:
                dx= dy= size/2.0; break;
        }
        cairo_translate( cr, dx, dy );
    }

    // Note: When offset==true, part of the line would be outside of the
    //       pattern canvas. We need to draw two lines to keep the pattern
    //       repeating.
    //
    switch( hatch_type ) {
        case HATCH_HORIZONTAL_VERTICAL:
        case HATCH_HORIZONTAL:
            cairo_move_to( cr, -((int)size), mid_y ); cairo_line_to( cr, size, mid_y );
            if (hatch_offset) {
                cairo_move_to( cr, -((int)size), mid_y-size ); cairo_line_to( cr, size, mid_y-size );
            }
            if (hatch_type==HATCH_HORIZONTAL) break;
            /* fall through */

        case HATCH_VERTICAL:
            cairo_move_to( cr, mid_x, -((int)size) ); cairo_line_to( cr, mid_x, size );
            if (hatch_offset) {
                cairo_move_to( cr, mid_x-size, -((int)size) ); cairo_line_to( cr, mid_x-size, size );
            }
            break;

        case HATCH_DOWN_UP:
        case HATCH_DOWN:
            cairo_move_to( cr, -((int)size), (int)size ); cairo_line_to( cr, size, -((int)size) );
            if (hatch_type==HATCH_DOWN) break;
            /* fall through */

        case HATCH_UP:
            cairo_move_to( cr, -((int)size), -((int)size) ); cairo_line_to( cr, size, size );
            break;
    }
    cairo_stroke(cr);
	cairo_destroy(cr);

    cairo_pattern_t *pat= cairo_pattern_create_for_surface(surf);
    cairo_pattern_set_extend( pat, CAIRO_EXTEND_REPEAT );

	cairo_surface_destroy(surf);

    return pat;
}


/*
* Create a section Contour Info (types 3 & 4), from a descriptor string.
*/
const ContourInfo_Fill *ContourInfo_Fill::create( const string &conf, const ContourMatrix<float>& data )
{
    vector< pair<float,float> > ranges;
    stringstream ss(conf);

    int type;
	ss >> type;

    if (type==3) {
        // "3 step zero-level min max ..."
        //
        float step, zero_level, lo, hi;
        ss >> step >> zero_level >> lo >> hi;
        if (ss.bad())
		  {
			LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
            throw runtime_error("Bad contour spec: "+conf);
		  }

        /*
		* 19-Mar-2015 PKi: 32700 goes now thru to tron (-inf, +inf)
        *
        if ((lo==32700) || (hi==32700)) {
            find_limits( data, step, lo==32700 ? &lo:nullptr, hi==32700 ? &hi:nullptr );
        }
        */

        if ((hi<lo) && (lo!=32700))
		  {
			LOG_USAGE( "Bad contour spec (hi<lo): %s", conf.c_str() );
            throw runtime_error("Bad contour spec (hi<lo): "+conf);
		  }

        // Step is used only for finding the upper/lower limits (if lo/hi==32700).
        // Within the lo..hi range step makes no sense.
        //
        ranges.push_back( pair<float,float>(lo,hi) );

        (void)zero_level;   // not used

    } else if (type==4) {
        // "4 n lo1 hi2 .. loN hiN ..."
        //
        int count= 0;
        ss >> count;
        for (int i = 0; i<count; i++) {
            float a,b;
            ss >> a >> b;
            if (ss.bad()) break;

            if ((b<a) && (a!=32700))
			  {
				LOG_USAGE( "Bad contour spec (hi<lo): %s", conf.c_str() );
                throw runtime_error("Bad contour spec (hi<lo): "+conf);
			  }

            ranges.push_back( pair<float,float>(a,b) );
        }
        if (ss.bad())
		  {
			LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
            throw runtime_error("Bad contour spec: "+conf);
		  }

    } else {
	  LOG_USAGE( "Bad contour type %d (expected 3,4)", type );
	  throw runtime_error("Bad contour type "+boost::lexical_cast<string>(type)+" (expected 3,4)");

    }

    // i.e. "rgba(40,40,40,100) none 2 2.0"
    //
    string stroke_color, fill_color, stroke_width, smooth_factor;
    
    ss >> stroke_color >> fill_color >> stroke_width >> smooth_factor;
    if (ss.bad())
	  {
		LOG_USAGE( "Bad contour spec: %s", conf.c_str() );
        throw runtime_error("Bad contour spec: "+conf);
	  }
    
    return new ContourInfo_Fill( ranges, 
                StrokeParams( stroke_color, stroke_width, smooth_factor ),
                FillParams( fill_color ) );
}


