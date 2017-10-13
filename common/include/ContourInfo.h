/*
* Contour specifications
*
* Contour creation strings formed into classes.
*/
#ifndef CONTOUR_INFO_H
#define CONTOUR_INFO_H

#include <string>
#include <utility>
#include <vector>

#include <math.h>

// 29-Dec-2011 PKi: Just to build
//
#define USE_NFMIDATAMATRIX_FOR_CONTOURS

/*
* On WinContour, it is better to use 'NFmiDataMatrix' for the data directly.
* On Q2 plugin, the interfacing is via 'Q2Matrix' (Q2 engine was intentionally
* stripped of all NFmi* features; this being one of them).
*
* Doing the NFmiDataMatrix->Q2Matrix conversion will slightly degrade Q2 plugin
* performance (until/if ever NFmiDataMatrix is completely eliminated).
*  --AKa 11-Dec-2008
*/
#ifdef USE_NFMIDATAMATRIX_FOR_CONTOURS
# include "newbase/NFmiDataMatrix.h"
# define ContourMatrix NFmiDataMatrix
#else
# include "Q2Matrix.h"
# define ContourMatrix Q2Matrix
#endif

#include <cairo.h>

class ContourInfo_Line;
class ContourInfo_Fill;

/*
* Common things to all ContourInfo
*/
class ContourInfo_common {
  public:
    // Alpha info (0..255) in bits 24..31. RGB are not scaled based on the
    // alpha (like Cairo does); any combination of 32 bits can be used.
    // Alpha is transparency, not opaqueness (opaqueness is 255-alpha).
    //
    typedef unsigned int uint_ARGB;     // 0xaarrggbb

    static const uint_ARGB BLACK=  0x00000000;
    static const uint_ARGB RED=    0x00ff0000;
    static const uint_ARGB GREEN=  0x0000ff00;
    static const uint_ARGB BLUE=   0x000000ff;
    static const uint_ARGB YELLOW= RED | GREEN;
    static const uint_ARGB WHITE=  RED | GREEN | BLUE;
    static const uint_ARGB NONE=   0xff000000;  // totally transparent

    static double A(uint_ARGB argb) { return 1.0 - (((argb)>>24) & 0xff) / 255.0; }
    static double R(uint_ARGB argb) { return (((argb)>>16) & 0xff) / 255.0; }
    static double G(uint_ARGB argb) { return (((argb)>>8) & 0xff) / 255.0; }
    static double B(uint_ARGB argb) { return ((argb) & 0xff) / 255.0; }

    /*const*/ struct StrokeParams {
        uint_ARGB color;
        std::vector<unsigned> dash;  // empty vector for not being used
	                                 // <on pixels>,<off pixels>, ... (2,4,6, .. entries)
        float width;
        float smooth;     // 0.0 (no smooth) .. 1.0 (full smooth)

      protected:
        friend class ::ContourInfo_Line;
        friend class ::ContourInfo_Fill;
        StrokeParams(
            const std::string &color_s,
            const std::string &width_s,
            const std::string &smooth_s );
            
      public:
        bool operator!=( const StrokeParams &o ) const;
    } stroke;

    virtual bool line_type() const = 0;   // pure abstract

    // We'll return either 'ContourInfo_Line' or 'ContourInfo_Fill'. C++ does
    // not allow us to mark this without a pointer (since our class is pure
    // virtual).
    //
    static const ContourInfo_common *create( const std::string &conf, const ContourMatrix<float>& data );
    static const ContourInfo_common *create( const std::string &conf );

    // Needed by C++ (though we do none)
    //
    virtual ~ContourInfo_common() { };

  protected:
	ContourInfo_common( const StrokeParams &stroke_ )
	   : stroke(stroke_) { }
};


/*
* Isoline drawing (types 1 & 2)
*/
class ContourInfo_Line : public ContourInfo_common {
  public:
    /*const*/ std::vector<float> levels;

    // Labelling details
    //
    /*const*/ struct LabelParams {
        unsigned strategy;    // 0: no labels, 1&2: horizontal, 3: tilted according to curvature

        float font_size_;
        uint_ARGB font_color;

        uint_ARGB box_fill_color;
        uint_ARGB box_stroke_color;
        float box_stroke_width;
      
      private:
        friend class ::ContourInfo_Line;
        LabelParams( 
            const std::string &strategy_s,
            const std::string &font_height_s,
            const std::string &font_color_s,
            const std::string &box_fill_color_s,
            const std::string &box_stroke_color_s,
            const std::string &box_stroke_width_s );
    } label;

    ContourInfo_Line( const std::vector<float> &levels_,
                      const StrokeParams &stroke_,
	                  const LabelParams &label_ )
	    : ContourInfo_common( stroke_ )
	    ,levels( levels_ )
	    ,label( label_ ) { }

    virtual bool line_type() const { return true; }

  private:
    friend class ContourInfo_common;
    static const ContourInfo_Line *create( const std::string &conf, const ContourMatrix<float>& data );
};


/*
* Section drawing (types 3 & 4)
*/
class ContourInfo_Fill : public ContourInfo_common {
  public:
    const std::vector< std::pair<float,float> > ranges;

    // Filling details
    //
	// Not 'const' since 'cairo_pattern_set_matrix' will change it
	// (so will 'cairo_set_source').
	//
    cairo_pattern_t *pattern;
    size_t patternSize() const { return pSize; }

    /*
    * This struct is just for providing fill/hatch parameters to the constructor;
    * internally we keep them as a ready-made Cairo pattern.
    */
    struct FillParams {
	   uint_ARGB color;

	   float hatch_width;    // 0: use solid 'fill_color'; 1..4
	   bool hatch_offset;
	   enum e_hatch_type {
	       HATCH_HORIZONTAL=   0,
	       HATCH_VERTICAL=     1,
	       HATCH_DOWN=         2,     // downwards 45 degrees
	       HATCH_UP=           3,     // upwards 45 degrees
	       HATCH_HORIZONTAL_VERTICAL= 4,
	       HATCH_DOWN_UP=      5
	   } hatch_type;
	   
      private:
        friend class ::ContourInfo_Fill;
        cairo_pattern_t *cairo_pattern_create(size_t size) const;
        FillParams( const std::string &fill_color_s );
	};

    ContourInfo_Fill( const std::vector< std::pair<float,float> > &ranges_,
                      const StrokeParams &stroke_,
                      const FillParams &fill )
        : ContourInfo_common( stroke_ )
        ,ranges(ranges_)
        ,pSize(10)
    {
    	pattern = fill.cairo_pattern_create(pSize);
    }

	~ContourInfo_Fill() {
		if (pattern) {
			// Note: 'cairo_pattern_destroy()' should take in a const, right?
			//
			cairo_pattern_destroy( (cairo_pattern_t*) pattern );
		}
	}
        
    virtual bool line_type() const { return false; }

  private:
    friend class ContourInfo_common;
    static const ContourInfo_Fill *create( const std::string &conf, const ContourMatrix<float>& );

    size_t pSize;
};

/*
* Projection calculation. Given as 0..2 "x,y=xx,yy" pairs.
* Used as panning & scaling values.
*/
class s_projection {
  private:
	unsigned points;	// 0..2

  public:
	long x[2],y[2], xx[2],yy[2];	// x,y in pixel coords is xx,yy data coords

	s_projection() : points(0) {}

	void set_point( long x_,long y_, long xx_,long yy_ ) {
		if (points<2) {
			x[points]= x_;
			y[points]= y_;
			xx[points]= xx_;
			yy[points]= yy_;
			points++;
		}
	}
	void done() {
		if (points==0) {
			x[0]= y[0]= xx[0]= yy[0]= 0;	// no panning
			x[1]= y[1]= xx[1]= yy[1]= 100;	// 1:1 scaling
		} else if (points==1) {
			x[1]=x[0]; y[1]=y[0]; xx[1]=xx[0]; yy[1]=yy[0];
			x[0]= y[0]= xx[0]= yy[0]= 0;
		} else {
			// points==2; all ok
		}
	}

	/* Note: We don't check projection against div-by-zero. dEmO
	*/
	double scale_x() const {
		return (x[1]-x[0])/(xx[1]-xx[0]);
	}
	double scale_y() const {
		return (y[1]-y[0])/(yy[1]-yy[0]);
	}

	long pan_x() const {
		return x[0] - (long)floor(xx[0]*scale_x()+0.5);
	}
	long pan_y() const {
		return y[0] - (long)floor(yy[0]*scale_y()+0.5);
	}
};

#endif
