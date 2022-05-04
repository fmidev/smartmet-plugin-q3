/*
* CONTOUR_TRON.CPP                          Copyright (c) 2010, Ilmatieteen laitos
*
* Implementation of contouring adapter using FMI Tron library.
*/
#ifndef USE_TRON
# error "Shouldn't even read this file in."
#endif

#include <geos/geom/Coordinate.inl>

#include "Contour.h"

#include "Matrix.h"

#include <smartmet/tron/SavitzkyGolay2D.h>		// 08-Dec-2011 PKi: Tron smoothening
#include "Proto.h"								// 05-Jun-2012 PKi: For using tron hints
#include "TronHints.h"							//

#include "PathAdapterBase.h"					// 13-Oct-2015 PKI: Changes for new tron api
#include "GeosTools.h"							//

//---
// Tron mode:
//
// 1: (removed)
// 2: Use '::fill()' for all contours (and 'fill(NAN,NAN)' for getting edges)
//    Gives correct results, but maaayy beeee slooooooooooooooo.........oooow?
// 3: Our dream API, not yet available. :(
//
#define TRON_MODE 2

#if TRON_MODE==1
# error "Tron mode 1 no longer included."
#endif

// Tron headers
//
#include "tron/FmiBuilder.h"
#include "tron/Contourer.h"
#include "tron/Traits.h"
#include "tron/LinearInterpolation.h"

using namespace std;

// Force a hole in data (DEBUGGING ONLY!)
//
#ifndef NDEBUG
//# define DEBUG_FORCE_HOLE
#endif


/*---=== Helpers ===---
*/


/*---=== Tron specific ===---
*/

/*
* Coordinate type must be 'double', to allow both interpolations but
* also full 32-bit integer accuracy.
*/
typedef Tron::Traits< float,    // value type
                      double,   // coord type
                      Tron::NanMissing  // missing values are NaN's
                    > MyTraits;

class MyGrid {
  public:
    MyGrid( /*const 08-Dec-2011 PKi*/ ContourMatrix &cm_ )
        : cm(cm_)
            //,x_factor( 1.0 / (cm_.getXS()-1) )
            //,y_factor( 1.0 / (cm_.getYS()-1) ) 
          {}
    
    // for Tron 'Contourer' template
    //
    typedef unsigned int size_type;
    typedef MyTraits::value_type value_type;
    typedef MyTraits::coord_type coord_type;

    // 08-Dec-2011 PKi: Return reference; smoothening changes the data
    //
    value_type & operator()( size_type i, size_type j ) const {

        // Force a hole to test behaviour in such cases
        //
#ifdef DEBUG_FORCE_HOLE
        const double cx= 10;
        const double cy= 10;
        const double r= 4;
        if ((i-cx)*(i-cx) + (j-cy)*(j-cy) < r*r) {
            return NAN;
        }
#endif
        return cm.getValue( MatrixPos(i,j) );
    }

    size_type width() const { 
        return cm.getXS();
    }
    
    size_type height() const {
        return cm.getYS();
    }

    /*
    * Return position within the grid
    */
    coord_type x(size_type i, size_type j) const { 
        (void)j;
        return i;   //*x_factor;
    }
    
    coord_type y(size_type i, size_type j) const {
        (void)i;
        return j;   //*y_factor;
    }

    // 08-Dec-2011 PKi: Required by tron smoothening
    //
    void swap(MyGrid & other)
    {
    	for (auto i = 0; (i < cm.getXS()); i++)
        	for (auto j = 0; (j < cm.getYS()); j++)
        		cm.getValue( MatrixPos(i,j) ) = other.cm.getValue( MatrixPos(i,j) );
    }

    bool valid(size_t i, size_t j) const
    {
      return true;
    }

  private:
    /*const 08-Dec-2011 PKi*/ ContourMatrix &cm;

    //const double x_factor;     // stores '1.0 / (m.getSize().getXS()-1)' 
    //const double y_factor;     // -''- (for y)
};


/*---=== EdgeAdapter ===---
*
* Class to gather edges of a material into a vector.
*/
#if TRON_MODE==2
class Tron_EdgeAdapter : public PathAdapterBase , public vector<Contour> {
  private:
    typedef Tron::Contourer<
        MyGrid, Tron::FmiBuilder, MyTraits, Tron::LinearInterpolation
    > MyContourer;

  public:
    Tron_EdgeAdapter( /*const 08-Dec-2011 PKi*/ ContourMatrix &cm )
        : mg(cm), itsGeomFactory(geos::geom::GeometryFactory::create()) {
	Tron::FmiBuilder builder(*itsGeomFactory);
        MyContourer::fill( builder, mg, NAN, NAN );   // limits of the data (holes or grid edge)

        SmartMet::Q3GeosTools::getContours(&(*builder.result()),this);

        assert( size() > 0 );
    }

    virtual ~Tron_EdgeAdapter() { }

    void moveto( const geos::geom::Coordinate & coordinate ) {
        push_back( Contour() );   // new contour
        lineto(coordinate);		  // add first point
    }

    void lineto( const geos::geom::Coordinate & coordinate ) {
        back().add_point( EdgePoint(coordinate.x, coordinate.y, true) );
    }

    /*
    * Return 'true' if (x,y) is at the curve.
    */
    bool at_edge( double x, double y ) const {
        for( vector<Contour>::const_iterator it= begin(); it != end(); ++it ) {
            if (it->at_edge(x,y)) {
                return true;
            }
        }
        return false;
    }
    
  private:
    MyGrid mg;
    geos::geom::GeometryFactory::Ptr itsGeomFactory;

    //MatrixPos::xy_t x_max, y_max;
};
#endif	// TRON_MODE==2

// 05-Jun-2012 PKi: New class TronHints

/*---=== TronHintsBind ===---*/

LuaNew_ID TronHintsBind::ID;

/*
* Set up a metatable.
*/
void TronHintsBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

}

/*---=== TronHints ===---*/

typedef Tron::Hints<MyGrid,MyTraits> MyHints;

class TronHints : public LuaNew<TronHintsBind> {
  public:
	TronHints() : itsHints(nullptr) { }
	~TronHints() { if (itsHints) delete itsHints; itsHints = nullptr; }

    MyHints & hints(MyGrid & mg) { if (!itsHints) itsHints = new MyHints(mg); return *itsHints; }

    static int is( lua_State *L ) {  // for 'proto.TronHints()'
        const TronHints *ll= TronHints::instance(L,1);
        lua_pushboolean( L, ll != nullptr );
        return 1;
    }

  private:
    friend class TronHintsBind;

    // data members
    //
    MyHints *itsHints;
};

/*---=== ContourAdapter ===---
* 
* Class for tying into the Tron for gaining N contours of matrix 'm' value 'val'.
*/
class Tron_ContourAdapter : public PathAdapterBase {
  private:
    typedef Tron::Contourer<
        MyGrid, Tron::FmiBuilder, MyTraits, Tron::LinearInterpolation
    > MyContourer;

  public:
    Tron_ContourAdapter( ContourCollector &cc_, /*const 08-Dec-2011 PKi*/ ContourMatrix &cm, float lo_val, float hi_val, unsigned int smooth_length, unsigned int smooth_degree, TronHints * th)
        : cc(cc_), mg(cm), current_contour(0)
#if TRON_MODE==2
            , edges(nullptr), x_max( cm.getXS()-1 ), y_max( cm.getYS()-1 ), itsGeomFactory(geos::geom::GeometryFactory::create())
#endif
    {
#if TRON_MODE==3
        // Here Tron gives edge information to 'moveto()' and 'lineto()' calls (and all paths are closed)
        //
        MyContourer::fill( *this, mg, val, NAN, false);    // iso curves at value 'val' (right hand side is "uphill")

#elif TRON_MODE==2
	Tron::FmiBuilder builder(*itsGeomFactory);

        if (!isnanf(lo_val)) {
            // Need to have 'edges' as a member so it's visible to 'lineto()' callback
            //
			edges= new Tron_EdgeAdapter(cm);

            // 08-Dec-2011 PKi: Tron smoothening
            //
            // 01-Mar-2012 PKi: Smoothening length must be smaller than matrix dimensions; adjust the
        	//				    length if necessary
            //
        	if (smooth_length > 0) {
        		if ((int) smooth_length > x_max) smooth_length = x_max;
        		if ((int) smooth_length > y_max) smooth_length = y_max;

            	if (smooth_length > 0)
            		Tron::SavitzkyGolay2D::smooth(mg,smooth_length,smooth_degree);
        	}

            // iso curves at value 'val' (right hand side is "uphill")
            // 'this->lineto' will have access to edges and see which points are at edge
            //
			// Note: Range NAN,x==-inf,x and range x,32700==x,+inf
			//
        	if (th)
			MyContourer::fill( builder, mg, (lo_val == 32700) ? NAN : lo_val, hi_val, th->hints(mg) );
        	else
			MyContourer::fill( builder, mg, (lo_val == 32700) ? NAN : lo_val, hi_val );

            SmartMet::Q3GeosTools::getContours(&(*builder.result()),this);

        	if (edges) {
				delete edges;   // not needed any more
				edges= nullptr;
        	}

        	if (current_contour && ((lo_val == 32700) || (!isnan(hi_val))))
        		current_contour->range(true);
        } else {
            assert( edges==nullptr );
            MyContourer::fill( builder, mg, NAN, NAN );   // limits of the data (holes or grid edge)

            SmartMet::Q3GeosTools::getContours(&(*builder.result()),this);

            assert( current_contour );
        }
#else
# error "TRON mode 1 removed"
#endif
    }

#if TRON_MODE==3
    void moveto( double x, double y, bool edge ) {
        current_contour= cc.new_contour();  // pushed on the caller side's (to us hidden) Lua stack
        lineto(x,y,edge);       // add first point
    }
    void lineto( double x, double y, bool edge ) {
        assert( current_contour );
        current_contour->add_point( EdgePoint(x,y,edge) );
    }

#elif TRON_MODE==2
    virtual ~Tron_ContourAdapter() { }

    void moveto( const geos::geom::Coordinate & coordinate ) {
        current_contour= cc.new_contour();  // pushed on the caller side's (to us hidden) Lua stack
        lineto(coordinate);     // add first point
    }

    void lineto( const geos::geom::Coordinate & coordinate ) {
        assert(current_contour);

        // Old tron - find out if we're at edge separately for each point
        // 
        bool at_edge_= false;

        if (edges) {
			// First check if the point is at material edge
			//
			double eps[]= { coordinate.x, coordinate.y, coordinate.x-x_max, coordinate.y-y_max };
			for( unsigned i=0; i<4; i++ ) {
				if (fabs(eps[i]) < 1e-5) {
					at_edge_= true;
					break;
				}
			}

			// Check holes in the material
			//
			if ((!at_edge_) && edges) {
				at_edge_= edges->at_edge(coordinate.x,coordinate.y);
			}
        }

        current_contour->add_point( EdgePoint(coordinate.x, coordinate.y, at_edge_) );
    }
#else
# error "TRON mode 1 removed"
#endif

  private:
    ContourCollector &cc;
    MyGrid mg;
    Contour *current_contour;       // nullptr initially, copy of a pointer maintained by the caller (not to be released)

#if TRON_MODE==2
    const Tron_EdgeAdapter *edges;    // giving 'lineto()' visibility to edges we live in
    MatrixPos::xy_t x_max, y_max;
    geos::geom::GeometryFactory::Ptr itsGeomFactory;
#endif
};


/*
*/
void tron_contour( ContourCollector &cc, /*const 08-Dec-2011 PKi*/ ContourMatrix &cm, float lo_val, float hi_val, unsigned int smooth_length, unsigned int smooth_degree, lua_State * L, unsigned int thIndex, unsigned int & tos ) {

    // 05-Jun-2012 PKi: If tron hints are to be used (L is not null) and the 'TronHints' arg is nil, TronHints object is pushed onto stack;
	//					otherwise the top of stack is adjusted down by 1 (nothing was pushed) and the hints are just used and returned
	//
	// 10-Jan-2014 PKi: Even if tron hints were passed in (contour.lua was modified not to do so if resmoothing is to be done),
	//					push new object if smooth factor is given; the data will be (re)smoothed

	TronHints * th = nullptr;

    if (L) {
		if ((smooth_length > 0) || (! (th = TronHints::instance( L, thIndex ))))
			th = new(L) TronHints();
		else
			tos--;
    }

    Tron_ContourAdapter curves( cc, cm, lo_val, hi_val, smooth_length, smooth_degree, th );
}
