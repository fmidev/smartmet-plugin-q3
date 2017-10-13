/*
* Smoothening contours.
*
* Defines:
*   - SMOOTH_AND_STRECTH compensates for the systematic error that happens when
*     averaging a curving line, or a closed loop in the extreme. Otherwise, 
*     all "islands" in the material will become smaller due to smoothening.
*   - SMOOTH_COPY whether to smoothen the curve as a copy or in place (default).
*     If smoothing in place, smoothed points affect smoothing of succeeding points.
*
* Author:   AKA 2-Feb-2009
*
* 26-Feb-2015 PKi: Code was taken from q2
*/

#include "Vectors.h"
#include "Smoothener.h"

using namespace std;

/*
* The actual smoothening of chain of points (modified in place)
*
* 'factor':     How much is the curve to be smoothened (0.0 = none)
* 'closed_':    If 'true', ties the last point to the first at path output.
*
* This smoothening is based on nice demo ("McMaster's Slide Averaging") at:
* <http://www.sli.unimelb.edu.au/gisweb/LGmodule/LGSmoothing.htm>
*
* For each window of SMOOTHENING_WINDOW_SIZE (5) points:
*   - take the average of their x,y
*   - move middle point a fraction towards the average (the point is not really
*     moved; the moved place is the smoothened version of that point)
*   - repeat..
*   - first and last two points are not moved
*
* Parameters:
*   - window width (3/5/7)
*   - factor of how muct to move the middle point (0.0 = none; 1.0 = full)
*
* Note:
*   - if the line is closed, we can smoothen everything, including the
*     start/stop position
*   - smoothened line is deterministic; certain data with certain parameters
*     always creates the same smoothened version. THIS IS IMPORTANT, since
*     we need to get exactly same results on multiple rounds, to i.e. have
*     neighbouring filled areas (with same smoothening factor) match.
*   - smoothened line is not dependent on the direction travelled.
*/
void SmoothPath::done( float factor, bool closed_ ) {

    closed= closed_;

    if (factor<=0.0) return;    // nothing to do (use as is)

#ifdef SMOOTH_AND_STRETCH
    /*
    * If 'ratio' of the smoothened area shrinks by this much (or more)
    * the area is expanded to compensate.
    */
    const double SHRINKING_TOLERANCE= 0.01;
#endif

    // Factors over 1.0 create bobwire-like graphics (not wanted)
    //
    if (factor>1.0) factor= 1.0;    

    unsigned n= rim.size();
    if (n==0) return;   // no points

    // If we're to compensate for systematic error, we need the curve's
    // center point.
    //
#ifdef SMOOTH_AND_STRETCH
    Point center(0,0);
    
    for( vector<PointAndEdge>::const_iterator it= rim.begin();
        it != rim.end();
        ++it ) {
        center += *it;     // we can bloat it to any amount (divide last)
    }
    center /= n;
    
    double shrunken_sum= 0.0;   // sum of all moves towards the center point
    double total_sum= 0.0;      // sum of all (moved) points from the center (before they were moved)
#endif

    // The following code covers both looping ('closed'==true) and non-looping
    // handling. We could also do them separately as if/else.
    //
    Vectors::Point avg(0,0);

    for( int i=-((int)SKIP); i<=(int)SKIP; i++ ) {
        avg.x = avg.x + rim[ (i+n)%n ].x;
        avg.y = avg.y + rim[ (i+n)%n ].y;
    }

//LOG_DEBUG( "Start: %g %g", rim[0].x, rim[0].y );
//LOG_DEBUG( "End: %g %g", rim[n-1].x, rim[n-1].y );

//#define SMOOTH_COPY

#ifdef SMOOTH_COPY
    std::vector<PointAndEdge> mir(rim);
#else
    std::vector<PointAndEdge> & mir = rim;
#endif

    for( unsigned i=0; i<n; i++ ) {
        if (closed_ || ((i>=SKIP) && (i+SKIP<n))) {

            // Do not move points that are at the edge or next to it
            //
        	bool move = true;
        	for( int j=-1; (j<=1) && move; j++ )
        		if (rim[(j+i+n)%n].at_edge())
        			move = false;

            if (move) {
#ifdef SMOOTH_AND_STRETCH
                double before= Vectors::Vector( center, rim[i] ).norm();
                total_sum += before;
#endif
                mir[i] += Vectors::Vector( rim[i], avg/WINDOW ) * factor;   // move the point

#ifdef SMOOTH_AND_STRETCH
                double after= Vectors::Vector( center, mir[i] ).norm();
                shrunken_sum += before-after;
#endif
            }
        }
        avg += rim[(i+SKIP+1)%n] - rim[(i+n-SKIP)%n];
    }

#ifdef SMOOTH_COPY
	rim.assign(mir.begin(),mir.end());
#endif

    // If the shrunken sum (per point) is essential, re-expand all points
    // by the average shrunkedness. This basically scales the curve/closure
    // by a factor > 1.0, while maintaining its general form.
    //
#ifdef SMOOTH_AND_STRETCH
    double f= shrunken_sum / total_sum;     // shrinking of radius in average (1D)

    if (f > SHRINKING_TOLERANCE) {
        for( unsigned i=0; i<n; i++ ) {
            // Leave unmoved points (open ends and edges) also un-expanded.
            //
            if (closed_ || ((i>=SKIP) && (i+SKIP<n))) {
            	bool move = true;
            	for( int j=-1; (j<=1) && move; j++ )
            		if (rim[(j+i+n)%n].at_edge())
            			move = false;

                if (move) {
                    rim[i] += Vectors::Vector( center, rim[i] ) * f;
                }
            }
        }
    }
#endif

    /* Now, this curve is ready for drawing */
}
