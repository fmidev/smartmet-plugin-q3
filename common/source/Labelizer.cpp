/*
* Labelizer.cpp
*
* Determining places for labels on contour lines.
*
* Changes:
*   AKa 20-Jan-2009: Speed optimization.
*   AKa 9-Dec-2008: Started the coding, specs together with Mika Heiskanen.
*
* Discussion of chain algorithm (optimization):
*
* Chains are stored in pieces ('ChainChunk'), with each chunk having its
* bounding box cached. This way, one can check for collisions with label
* rectangles Really Fast since most chunks' bounding boxes won't touch that
* of the labels. 
*
* One could also cache the overall Chain's bounding box, but the benefit
* would most likely be negligible.
*
* A 'ChainChunk' is grown until its boundary box area reaches a certain
* pre-determined limit. This is way better than taking in each N points to
* one chunk; now chunks become more or less same sized as to their surface
* area, which is decicive when making boundary box checks.
*   --AKa 20-Jan-2009
*/
#include "Labelizer.h"

// 29-Dec-2011 PKi: LogTools.h instead of QDLog.h
//
//#include "QDLog.h"
#include "LogTools.h"

#include <string.h>
#include <assert.h>
#include <math.h>
#include "LuaWrap.h"		// 01-Feb-2012 PKi

#include <stdexcept>

using namespace std;

/*
* Take at least this many points into each curve chunk, even if their combined
* area would exceed the limit from 'CurveOptimizationFactor'.
*
* TUNING: 
*   This setting was tested on one sample fetch, with values 2..10 giving
*   execution times 618ms..578ms..631ms. The fastest was with value 8.
*   (see 'Labelizer tuning.ods')    --AKa Jan/Feb 2009
*/
const unsigned MINIMUM_POINTS_IN_CHUNK= 10;  // 2..n (too small or too big is slow)

typedef Vectors::Point Point;
typedef Vectors::BoundingBox BoundingBox;
typedef Vectors::Vector Vector;
typedef Vectors::PointAndVector PointAndVector;

/*
* TUNING: 
*   PointScore with vectors cached (deriving from 'PointAndVector') is about
*   1.4 times faster than if the vectors were calculated on demand.
*   --AKa Jan-2009
*/
struct Labelizer::PointScore
    : public PointAndVector     // also have the N-1..N vector (cached)
{
    double score;       // >= 0.0: quality of the point for having the label
                        // USED_LABEL: already used, a label was placed here
                        // USED_IGNORE: already considered, abandoned

    static const double SCORE_LABEL;    // point already issued as a label
    static const double SCORE_REJECTED;   // point considered and found to be no-good
    static const double SCORE_TOO_CLOSE;   // point too close to existing label (don't consider)

    PointScore( const Point &p, const Vector &v, double score_ ) 
        : PointAndVector(p,v), score(score_) {}
};

// gcc really requires us to have them here (maybe this is due to 'us' being
// a dynamic module?). The values don't matter as far as they are <0.0 and
// SCORE_LABEL is unique.
//
const double Labelizer::PointScore::SCORE_LABEL= -1.0;
const double Labelizer::PointScore::SCORE_REJECTED= -2.0;
const double Labelizer::PointScore::SCORE_TOO_CLOSE= -2.1;

// 20-Dec-2011 PKi: Labelizer configuration
//
Labelizer::Config Labelizer::lblCfg;

/*
* Chunk of points, part of a chain. Area at most 'area_limit' unless 
* forced above by special cases (adding last point; adding second point)
*/
class Labelizer::ChainChunk {
  private:
    vector<PointScore> curve;
    double area_limit;

    BoundingBox bb;     // for intersection optimization

  public:
    ChainChunk( double area_limit_ ) : curve(), 
                   area_limit(area_limit_),
                   bb()
                   {}

    /*
    * Add a point to the current chunk.
    *
    * If the boundary box area would exceed the limit, return 'false' (and don't
    * add the point). However, if it within the first MINIMUM_POINTS_IN_CHUNK,
    * always add.
    */
    bool add_point( const Point &p, const Vector &prev_to_p ) {
        if (curve.size()< MINIMUM_POINTS_IN_CHUNK) {
            bb.extend(p);
        } else {
            BoundingBox bb2= bb;
            bb2.extend(p);
            if (bb2.area() > area_limit)
                return false;    // would be too big (use a new chunk)
                
            bb= bb2;
        }
        curve.push_back( PointScore(p,prev_to_p,0.0) );
        return true;
    }
    
    unsigned size() const {
        return curve.size();
    }
    
    bool intersects( const TiltedRect &tr, Labelizer::Drawer &debug_drawer ) const;
    bool intersects( const PointAndVector &tr, Labelizer::Drawer &debug_drawer ) const;

    PointScore &operator[]( unsigned i ) {
        return curve[i];
    }
};

/*
* Iterator for ChainChunks, which jumps through the chunks efficiently.
*
* The iterator allows either open-ended (0..size-1) or round-ring iteration.
* A closed loop can be iterated in either mode, depending on the needs of the
* particular code.
*
* Note: There is no const iterator mainly because it'd be a LOT of copy-paste.
*       If some day someone makes a const iterator, some of the usage of
*       'ChainIterator' could be changed to be 'const' members of 'Chain'. 
*       --AKa 21-Jan-2009
*/
class Labelizer::ChainIterator {
  private:
    vector<Labelizer::ChainChunk>& chunks;
    
    int chunk_n;   // 0..n (-1: not iterating = end)
    int inner_n;   // 0..chunks[x].size()-1 (-1: not iterating = end)

    bool is_closed;     // if 'true', an iterator goes round the loop and NEVER
                        // BECOMES INVALID (test for being same as the one started)

    // Documentary function declaring object field sanity rules.
    //
    void _INVARIANT_() const {
        if (chunk_n>=0) {
            assert( (unsigned)chunk_n < chunks.size() );      // 0..chunks.size()-1
            assert( inner_n >= 0 );
            assert( (unsigned)inner_n < chunks[chunk_n].size() );
        } else {
            assert( inner_n < 0 );
        }
    }

  public:
    ChainIterator( vector<ChainChunk>& chunks_, bool is_closed_=false ) 
        : chunks(chunks_), chunk_n(), inner_n(), is_closed(is_closed_) {

        if (chunks.size()>0) {
            chunk_n= inner_n= 0;    // first item
        } else {
            chunk_n= inner_n= -1;   // invalid (no data)
        }
    }

    /*
    * This is used for making a roll-around iterator out of a linear one
    * (or vice versa).
    */
    ChainIterator( const ChainIterator& o, bool is_closed_, int step=0 )
        : chunks( o.chunks ), chunk_n(o.chunk_n), inner_n(o.inner_n), is_closed(is_closed_) {
        
        if (step!=0)
            *this += step;
    }

    /*
    * We need to declare an assignment operation, due to the reference we
    * have. Compiler cannot know it is okay to share the reference with other
    * copies (because we use this in such way).
    */
    ChainIterator& operator=( ChainIterator& o ) {
        chunks= o.chunks;
        chunk_n= o.chunk_n;
        inner_n= o.inner_n;
        is_closed= o.is_closed;
        return *this;
    }

    bool valid() const {
        return chunk_n>=0;
    }

    bool is_first() const {
        return (!is_closed) && (chunk_n==0) && (inner_n==0);
    }
    
    bool is_last() const {
        return (!is_closed) && ((unsigned)chunk_n==chunks.size()-1) && ((unsigned)inner_n==chunks[chunk_n].size()-1);
    }

    // Intentionally only prefix ++/-- operators defined (not postfix).
    //
    void operator++() {
        if (!valid()) {
            // at end; stay that way
        } else if ((unsigned)(inner_n+1) < chunks[chunk_n].size()) {
            inner_n++;
        } else if ((unsigned)(chunk_n+1) < chunks.size()) {
            chunk_n++; inner_n=0;
        } else if (is_closed) {
            // hop around to beginning
            chunk_n= inner_n= 0;
        } else {
            chunk_n= inner_n= -1;   // end
        }
    }

    void operator--() {
        if (!valid()) {
            // at end; stay that way
        } else if (inner_n > 0) {
            inner_n--;
        } else if (chunk_n > 0) {
            chunk_n--; inner_n=chunks[chunk_n].size()-1;
        } else if (is_closed) {
            // hop around to last node
            chunk_n= chunks.size()-1;
            inner_n= chunks[chunk_n].size()-1;
        } else {
            chunk_n= inner_n= -1;   // end (or rather: before beginning)
        }
    }

    ChainIterator &operator+=( int step ) {
        /* We don't need to check for 'valid'; if 'this' becomes invalid,
        * ++ and -- will simply do nothing more.
        */
        if (step<0) {
            while(step++ < 0) --*this;
        } else {
            while(step-- > 0) ++*this;
        }
        return *this;
    }
    ChainIterator &operator-=( int step ) {
        return *this += (-step);
    }

    PointScore &operator*() {
        if (!valid())
		  {
			LOG_BUG0( "Referencing an invalid iterator" );
            throw runtime_error("Referencing an invalid iterator" );
		  } 
        return chunks[chunk_n][inner_n];
    }
    const PointScore &operator*() const {
        if (!valid())
		  {
			LOG_BUG0( "Referencing an invalid iterator" );
            throw runtime_error("Referencing an invalid iterator");
		  } 
        return chunks[chunk_n][inner_n];
    }

    PointScore *operator->() {
        return &(operator*());
    }
    const PointScore *operator->() const {
        return &(operator*());
    }
        
    /*
    * Comparison operators tell whether two iterators point to same point
    * (crucial for iterating closed loops!).
    *
    * Note: Two invalid iterators are taken to be unequal (just like NaN's).
    *
    * Note: Iterators must be accessing the same underlying chain.
    */
    bool operator==( const ChainIterator &o ) {
        return valid() && o.valid() && (chunk_n==o.chunk_n) && (inner_n==o.inner_n);
    }
    
    bool operator!=( const ChainIterator &o ) {
        return !(*this == o);
    }
};

class Labelizer::Chain {
  private:
    FontSpec fs;        // label and its size
    vector<ChainChunk> chunks;  // (see description earlier)

  public:
    BoundingBox bb;     // for calculating scores based on points' position
                        // within their curve's bounding box

  private:
    enum {
        ST_BUSY,        // being still worked upon (more points coming)
        ST_DONE_OPEN,   // no more points; ends are open
        ST_DONE_CLOSED  // no more points; closed loop ([0] connects to [size-1])
    } st;

    unsigned total_points;  // number of points altogether in the chunks
    
    double chunk_area_limit;      // area limit for chunk size (in data coords)
    
    Point prev_p;       // last point added
    Labelizer::Drawer &debug_drawer;

  public:
    Chain( const FontSpec &fs_, double chunk_area_limit_, Drawer &debug_drawer_ ) : fs(fs_), chunks(), 
            bb(),
            st(ST_BUSY), total_points(0), chunk_area_limit(chunk_area_limit_)
            ,prev_p()
            ,debug_drawer(debug_drawer_)
            {}
    ~Chain() {}

    void add_point( const Point &p );

    /*
    * A chain is to be closed to its starting point.
    */
    void close() { 
        // Revise the first point's vector to tie the knot.
        //
        ChainIterator head(chunks);   // points to first point

        *head= PointScore( head->p, Vector( prev_p, head->p ), head->score );
        st= ST_DONE_CLOSED;
    }

    bool is_closed() const { 
        assert( st != ST_BUSY );
        return st==ST_DONE_CLOSED;
    }

    void done( const Config &config );
    
    ChainIterator candidate( double *tilt_rad_ref, double *goodness_ref );
    
    unsigned size() const { return total_points; }

    const FontSpec &get_fs() const { return fs; }
    
    bool intersects( const PointAndVector & ) /*const*/;
    bool intersects( const TiltedRect & ) /*const*/;
    
    void draw_labels( Labelizer::Drawer &drawer );
};
typedef Labelizer::Chain Chain;


/*
*/
Labelizer::~Labelizer() {
    
    for( vector<Chain*>::iterator it= chains.begin();
        it != chains.end();
        ++it ) {
        delete *it;
    }

/*
    Print out our timing results
*/

#ifdef TIMINGS
    LOG_TIMER( "Profiling: Sort %ld, "
                        "Candidate %ld, "
                        "Limit along line %ld, "
                        "Limit direct %ld, "
                        "Limit crossing lines %ld, "
                        "Limit overlapping labels %ld\n",
                SORT_PROF.ms(),
                CANDIDATE_PROF.ms(),
                LIMIT_ALONG_LINE_PROF.ms(),
                LIMIT_DIRECT_PROF.ms(),
                LIMIT_CROSSING_LINES_PROF.ms(),
                LIMIT_OVERLAPPING_LABELS_PROF.ms() );
#endif
}


/*
* Add a point to the chain.
*
* Also points outside of global boundaries (if used) are added; they will be
* given 0.0 score though (so as to never consider placing a label there).
*/
void Labelizer::Chain::add_point( const Point &xy ) {

    if (st!=ST_BUSY)
	  {
		 LOG_BUG0("Cannot add points any more (chain is done)");
        throw runtime_error("Cannot add points any more (chain is done)");
	  }

    bool empty= (chunks.size()==0);
    if (empty) {
        // make the first chunk
        //
        chunks.push_back( ChainChunk(chunk_area_limit) );
        chunks.back().add_point(xy,Vector());     // null vector since we don't have a prior one
    } else {
        Vector v(prev_p,xy);     // stored in 'PointScore' class for optimization purposes
                                // (direction from point N-1 to N)
                                
        if (!chunks.back().add_point(xy,v)) {
            // make a new chunk (growing so big)
            //
            chunks.push_back( ChainChunk(chunk_area_limit) );
            bool _=chunks.back().add_point(xy,v);     // should always succeeed
            assert(_); (void)_;
        }
    }
    prev_p= xy;

    bb.extend(xy);
    total_points++;
}


/*
* A whole chain has been received; give initial quality for the points, based
* on their position within the chain.
*
* Points outside of the global bounding box are given 0.0 score; no such points
* will ever get a label.
*/
void Labelizer::Chain::done( const Config &config ) {

    // 'st' already is ST_DONE_CLOSED if there has been a loop in the data
    //
    if (st==ST_BUSY)
        st= ST_DONE_OPEN;

    if (config.dampen_corners > 1.0)
        LOG_WARNING( ".dampen_corners factor is supposed to be <= 1.0 (%lf)", config.dampen_corners );

    if (config.boost_horizontal < 1.0)
        LOG_WARNING( ".boost_horizontal factor is supposed to be >= 1.0 (%lf)", config.boost_horizontal );

    assert(total_points>0);

    const double DAMPEN_CORNERS_LIMIT_COS= cos( config.dampen_corners_limit_deg * (180.0/M_PI) );
    const double BOOST_HORIZONTAL_LIMIT_COS= cos( config.boost_horizontal_limit_deg * (180.0/M_PI) );  

    double bb_w= bb.width();
    double bb_h= bb.height();

    bool closed= is_closed();

    // We iterate 0..size-1 but the 'it_next' iterator is set up to be round-
    // ring. If the chain is closed, we still get a valid 'next'.
    //
    // Note: must not do '++it_prev' since it can be initially invalid (for 
    //       open ended chains).
    //
    ChainIterator it( chunks );     // first point
    ChainIterator it_next(it,closed,+1);

    for( ; it.valid(); ++it,++it_next ) {
    
        if (it==it_next) {
            // Should not happen, there's only one point in the chunks.
            //
            LOG_BUG( "One-point chunks should not happen (skipping): %g %g", it->p.x, it->p.y );
            it->score= Labelizer::PointScore::SCORE_REJECTED;
            break;
        }
        //assert( it_next != it );

        // All scores are 0.0 because of their constructor
        //
        assert( it->score == 0.0 );

        // Open ends should not be considered for labels, at all (would hide
        // the end point, and/or be at the edge of the target area).
        //
        if (it.is_first() || it.is_last()) {
            continue;
        }

        const Point &p= it->p;

        // This will allow the first point (of open ended chain) to be 
        // considered label placement. TBD
        //
        if (!it_next.valid()) {
            continue;   // start/end point (if open ended); skip
        } 
        else {
            // Standardization of x,y to (0,0)..(pi,pi) coordinates
            //
            double x= M_PI * ((bb_w==0.0) ? 0.5 : ((p.x - bb.lo.x) / bb_w));
            double y= M_PI * ((bb_h==0.0) ? 0.5 : ((p.y - bb.lo.y) / bb_h));
    
            // Calculation of initial quality: points getting higher scores
            // will be the primary candidates for labelling.
            //
            // The score function can be _anything_ mapping (0,0)..(pi,pi) to
            // a positive value range (<= 0.0 scores won't ever get a label).
            //
            double score= sin(x)+sin(y) - 2* pow(sin(x)*sin(y),2.0);
    
            Vector A= *it;          // N-1 to N
            Vector B= *it_next;     // N to N+1

            // Dampen score if the position is a corner (not smooth line)
            //
            // Smooth line (no corner at all) will give cos 1.0. Anything less
            // is a corner; -1.0 is complete u-turn (should not happen).
            //
            if ( A.cos(B) < DAMPEN_CORNERS_LIMIT_COS ) {
                score *= config.dampen_corners;
    
            } else if ((fabs(A.cos()) >= BOOST_HORIZONTAL_LIMIT_COS) &&
                    (fabs(B.cos()) >= BOOST_HORIZONTAL_LIMIT_COS)) {
                score *= config.boost_horizontal;
            } 
            it->score= score;
        }
    };    
}


/*
* Add a new chain
*/
void Labelizer::add_point( const Point &xy, const FontSpec &fs ) {

    if (current_chain) {
        current_chain->done( config );
        chains.push_back(current_chain);
    }

    // 'config.bounds' is in data coordinate system
    //
    double chunk_area_limit= 
        (config.curve_optimization != 0.0) ? bb.area() / config.curve_optimization
                                           : MAXFLOAT;   // no optimization (just one chunk)
    
    current_chain= new Chain(fs, chunk_area_limit, debug_drawer);
    current_chain->add_point(xy);
}


/*
* Add a new point to current chain.
*/
void Labelizer::add_point( const Point &xy ) {

    if (!current_chain)
        throw runtime_error( "No label; use 'add_point(xy,fs)' first" );

    // Eliminate duplicate points before they get to the chain.
    //
    if ((current_chain->size() > 0) && (last_point==xy)) {
        return;     // skipped; already in the chain

    } else {
        current_chain->add_point(xy);
        last_point= xy;
    }
}


/*
* Close a path to its starting point.
*/
void Labelizer::close() {

    if (!current_chain)
        throw runtime_error( "No points to close; use 'add_point()' first" );

    current_chain->close();
}


/*
* All points have been given; decide location for the labels.
*/
vector<Labelizer::TiltedRect> Labelizer::done(double scale) {

    if (current_chain) {
        current_chain->done( config );
        chains.push_back(current_chain);
        current_chain= 0;
    }

    struct local {  // this tricks C++ to give us local functions :)
        /*
        * Sort chains from smaller 'measure' to higher; small islands are given
        * labels first.
        */
        static bool SortChainByArea( const Chain *a, const Chain *b ) {
            double ma= a->bb.area();
            double mb= b->bb.area();
            return ma < mb;
        }
    };

    // Sort the chains based on their bounding area size. This lets small 
    // "islands" place their labels first; bigger chains will have more 
    // freedom in adjusting theirs.
    //
SORT_PROF.start();
    std::sort( chains.begin(), chains.end(), local::SortChainByArea );
SORT_PROF.stop();

    // Collection of labels that have been placed so far
    //
    vector< TiltedRect > done;

    for( vector<Chain*>::iterator it= chains.begin();
        it != chains.end();
        ++it ) {
        // '*it' is the chain to consider

        bool is_closed= (*it)->is_closed();

        while(true) {
            double tilt_rad=0.0;
            bool used= false;
            double goodness;   // goodness value (starts with best)

CANDIDATE_PROF.start();
            // Keep the iterator, with which we can travel back/forward the chain
            // (we'll need it soon to mark neighbouring points invalid, if this
            // becomes selected).
            //
            ChainIterator chosen= (*it)->candidate( &tilt_rad, &goodness );
CANDIDATE_PROF.stop();
            if (!chosen.valid())
                break;  // all points gone; did not find a good (or even bad) candidate

            const Point &chosen_p= chosen->p;

            // 'chosen' is the best candidate based on the curve's internal
            // aspects. But does it fit in with Rules?

            if (!LimitAlongTheLine( chosen, config.limit_along_line * scale, is_closed )) {
                // too close to earlier label point (RED)
                debug_drawer.mark_candidate( chosen_p, goodness, "red" );
            } 
            else if (!LimitDirect( chosen_p, done, config.limit_direct * scale )) {
                // too close to some (any) other already passed label (BLUE)
                debug_drawer.mark_candidate( chosen_p, goodness, "blue" );
            }
            else {
                TiltedRect tr( chosen_p, (*it)->get_fs().size, tilt_rad );
                debug_drawer.mark_tr(tr);

                // TiltedRect is within the target bounding box exactly when its
                // bounding box is.
                //
                if (!bb.inside_or_at_edge(tr.get_bounding_box())) {
                    // outside of the target area (reject)
                    debug_drawer.mark_candidate( chosen_p, goodness, "green" );
                }
                else if ((!config.allow_overlapping_lines) && (!LimitCrossingLines( tr, chains, *it ))) {
                    // some (other than chain) line passed through the label (YELLOW)
                    debug_drawer.mark_candidate( chosen_p, goodness, "yellow" );
                }
                else if ((!config.allow_overlapping_labels) && (!LimitOverlappingLabels( tr, done ))) {
                    // existing label already there (MAGENTA)
                    debug_drawer.mark_candidate( chosen_p, goodness, "magenta" );
                } else {
                    // Seems this label is worthy of output
                    
                    // This is for our own intersection detection
                    //
                    done.push_back(tr);

                    // This is for collecting the labels to clipping & drawing
                    //
                    chosenlabels.push_back( ChosenLabel( tr.get_center(), tilt_rad, (*it)->get_fs() ) );
                    used= true;

                    // Mark points surrounding the chosen one (but too close)
                    // as not to be considered any more.
                    //
                    RejectNearby( chosen, config.limit_along_line * scale, is_closed );
                }
            }
            
            /*
            * 'chosen' was either used for a label or rejected.
            */
            chosen->score= used ? PointScore::SCORE_LABEL : PointScore::SCORE_REJECTED;
        }
    }
    
    return done;
}


/*
* Output the results using custom 'drawer' object.
*/
void Labelizer::draw_labels( Drawer &drawer ) {

    for( vector<ChosenLabel>::const_iterator it= chosenlabels.begin();
        it != chosenlabels.end();
        ++it ) {
        drawer.label( it->center, it->fs.label, *(it->fs.lp), it->rad );
    }
}



/*
* Check if there are already labels too close along the same line (either 
* forward or backward, but checking the nearest issued labels is enough).
*
* If the line itself is shorter than the limit, no label is to be placed.
*
* Returns:
*   false if a limiting situation was found
*   true for go ahead
*/
bool Labelizer::LimitAlongTheLine( const ChainIterator &chosen, double limit, bool is_closed ) {
    bool ret= false;

    // Keep both distances (back and forth) to see if an open ended curve
    // is long enough to get a label, at all.
    //
    double dist[2]= { 0.0, 0.0 }; 

LIMIT_ALONG_LINE_PROF.start();
    {
    /*
    * Loop both ways from 'chosen' to the next SCORE_LABEL (counting distance)
    */
    for( unsigned round=0; round<2; ++round ) {
        int step= (round==0) ? +1:-1;

        ChainIterator it_was= chosen;
        ChainIterator it(chosen,is_closed, step);
    
        for( ; it.valid(); it_was=it, it+=step ) {
            if (it==chosen) {
                // Made full round, no need to do the same backwards
                //
                round=2; break;     // break twice
            }

            dist[round] += it->norm();     // vector from point N-1 to N

            if (dist[round] > limit)
                break;  // gone far enough
    
            if (it->score == PointScore::SCORE_LABEL)
                goto LABEL_TOO_NEAR;    // returns 'false'
        }
    }
    // no limits found

    // Is the chain long enough to bear a single label?
    //
    ret= (dist[0]+dist[1] >= limit);

LABEL_TOO_NEAR: ;
    }
LIMIT_ALONG_LINE_PROF.stop();
    return ret;
}


/*
* Check all other issued labels for being far enough.
*
* Returns:
*   false if a limiting situation was found
*   true for go ahead
*/
bool Labelizer::LimitDirect( const Point &xy, const vector<TiltedRect> &done, double limit ) {
    bool ret= false;

LIMIT_DIRECT_PROF.start();
    {
    for( vector<TiltedRect>::const_iterator it= done.begin();
        it != done.end();
        ++it ) {
        double dist= Vector( xy, it->get_center() ).norm();
//LOG_DEBUG( "Dist %lf,%lf -> %lf %lf = %lf (limit %lf)", xy.x, xy.y, it->get_center().x, it->get_center().y, dist, limit );

        if (dist < limit)
            goto LIMIT_DIRECT_RET;    // Too near; no label here
    }
    ret= true;   // ok (no limit)
    
LIMIT_DIRECT_RET: ;
    }
LIMIT_DIRECT_PROF.stop();
    return ret;
}

/*
* Check that no lines (any lines, except our chain) cross the label box.
*
* Returns:
*   false if a limiting situation was found
*   true for go ahead
*/
bool Labelizer::LimitCrossingLines( const TiltedRect &tr, const vector<Chain*> &chains, const Chain *our_chain ) {
    bool ret= false;

LIMIT_CROSSING_LINES_PROF.start();
    {
    for( vector<Chain *>::const_iterator it= chains.begin();
        it != chains.end();
        ++it ) {
        if (*it == our_chain) continue;

        if ((*it)->intersects( tr )) {
            goto LIMIT_CROSSING_LINES_RET;   // intersection (no label)
        }
    }
    ret= true;   // ok for label
    
LIMIT_CROSSING_LINES_RET: ;
    }
LIMIT_CROSSING_LINES_PROF.stop();
    return ret;
}

/*
* Check that the proposed label would not overlap any already issued one.
*
* Returns:
*   false if a limiting situation was found
*   true for go ahead
*/
bool Labelizer::LimitOverlappingLabels( const TiltedRect &tr, const vector<TiltedRect> &done  ) {
    bool ret= false;

LIMIT_OVERLAPPING_LABELS_PROF.start();
    {
    for( vector<TiltedRect>::const_iterator it= done.begin();
        it != done.end();
        ++it ) {
        if (tr.intersects(*it))
            goto LIMIT_OVERLAPPING_LABELS_RET;    // limitation
    }
    ret= true;
    
LIMIT_OVERLAPPING_LABELS_RET: ;
    }
LIMIT_OVERLAPPING_LABELS_PROF.stop();
    return ret;
}

/*
* Mark points around 'chosen' as unavailable.
*/
void Labelizer::RejectNearby( const ChainIterator &chosen, double limit_along_line, bool is_closed ) {

    // Backwards from 'chosen' (goes around the ring if 'is_closed')
    //
    for( unsigned round=0; round<2; round++ ) {
        double len= 0.0;
        int step= (round==0) ? +1:-1;

        ChainIterator it_was= chosen;
        for( ChainIterator it( chosen, is_closed, step );
            it.valid();
            it_was=it, it+=step ) {
        
            if (it==chosen) {
                // Gone round once; no need to do the same backwards
                round=2; break;
            }

            len += it->norm();     // length from N to N-1

            if (len<limit_along_line) {
                // ..just a sanity check (there cannot be a label since we're
                // marking this area too close of the new one)
                //
                assert( it->score != PointScore::SCORE_LABEL );

                it->score= PointScore::SCORE_TOO_CLOSE;
            } else {
                break;
            }
        }
    }
}


/*
* Constructor of 'TiltedRect'.
*
* The corner points are precalculated to speed up actually using them.
*
*  1-----2
*  |  p  |   Here 'tilt_rad' is 0.0. Positive rad will rotate counter-clockwise
*  0-----3   around the center point.
*/
Labelizer::TiltedRect::TiltedRect( const Point &p, const Vector &size, double tilt_rad )
    : side(), center(p), bb_cached() {

    // Note: 'side[4]' is initialized here. We'd LIKE it to be initialized
    //      in the constructor chain, but how to?   --AKa 19-Jan-2009
    
    double w= size.get_dx();
    double h= size.get_dy();

    /*
    * We'll count corner positions using radial coordinates.
    */
    double r= size.norm() / 2.0;
    
    double alpha= atan(h/w);    // basic angle (if not tilted)

    Point corner[4];

    for( unsigned i=0; i<4; i++ ) {
        double rad;
        switch (i) {
            case 0: rad= -M_PI + alpha; break;
            case 1: rad= M_PI - alpha; break;
            case 2: rad= alpha; break;
            case 3: rad= -alpha; break;
        }
        rad += tilt_rad;

        corner[i]= Point( p.x + r*cos(rad), p.y + r*sin(rad) );
    }
    
    for( unsigned i=0; i<4; i++ ) {
        side[i]= PointAndVector( corner[i], corner[(i+1)%4] );

        // BoundingBox is cached as a data member. 'bb()' initializes it to
        // "unconstrained"; calling '.extend()' sets up the box.
        //
        bb_cached.extend( corner[i] );
    }
}


/*
* Intersection of two labels (also containment of one label within the other
* qualifies).
*/
bool Labelizer::TiltedRect::intersects( const TiltedRect &other ) const {

    // Eliminate most cases by bounding box check
    //
    if (bb_cached.detached( other.bb_cached ))
        return false;

    for( unsigned i=0; i<4; i++ ) {
        if (intersects( other.get_side(i) ))
            return true;    // sides cross (or at least corners touch)
    }

    // Theoretically, 'other' can also be completely within us (or the other
    // way round). It is enough to check, if _any_one_ of the corners is 
    // inside; they either are all in, or all out (since sides don't cross)
    //
    if (contains(other.get_corner(0)) || other.contains(get_corner(0)))
        return true;

    return false;   // no intersection, overlap, or corner touch
}


/*
* Does the label box intersect a certain vector
*/
bool Labelizer::TiltedRect::intersects( const PointAndVector &v ) const {

    /* This is called only if 'v' is indeed within a TiltedRect's boundary box.
    */
    for( unsigned i=0; i<4; i++ ) {
        if (get_side(i).intersects(v))
            return true;
    }
    return false;
}


/*
* Is the point within (or even at sides/corners) of TiltedRect?
*
* It is within (or at sides) if all sides see it as being on the right (or
* straight front/straight back). We can use cross product (the z component)
* to calculate this effectively: negative if 'p' is right of a side; zero if
* at the line, positive if to the left ("right hand rule").
*/
bool Labelizer::TiltedRect::contains( const Point &p ) const {

    if (!bb_cached.inside_or_at_edge(p))
        return false;   // clearly outside (even outside the bounding box)

    for( unsigned i=0; i<4; i++ ) {
        double v= get_side(i).cross_z( Vector(get_corner(i), p) );
        if (v > 0.0)
            return false;   // does not contain (left of some of the sides)
    }
    return true;    // within, or touching
}


/*
* Find the best candidate among remaining points in the chain (those with
* score >0.0).
*
* Returns:
*   iterator to the best candidate
*   invalid iterator if no possible spot
*
* '*tilt_rad_ref' is set to the curvature at the candidate's position (0.0 =
* horizontal, rising counter-clockwise). The angle is limited to [-pi/2,pi/2) range
* making it always upright to the user.
*
* '*goodness_ref' is set to the relative goodness value that got this one
* selected (needed only for debugging; placing marks on the output).
*/
Labelizer::ChainIterator Labelizer::Chain::candidate( double *tilt_rad_ref, double *goodness_ref ) {

    assert( st != ST_BUSY );
    assert(tilt_rad_ref);

    bool closed= is_closed();
    double best= 0.0;   // below this (or same) never gets a label
    
    ChainIterator it(chunks);
    ChainIterator best_it(it,false,-1);    // makes an invalid iterator

    assert( !best_it.valid() );

    for( ChainIterator it(chunks); it.valid(); ++it ) {

        double score= it->score;

        if (score > best) {
            best= score;
            best_it= it;
        }
    }

    // Find out the tiltedness of the candidate (average of surrounding vectors)
    //
    if ((fs.lp->strategy==3 /*tilted labels*/) && best_it.valid()) {

        ChainIterator best_prev( best_it, closed, -1 );
        ChainIterator best_next( best_it, closed, +1 );

        bool best_prev_valid= best_prev.valid();
        bool best_next_valid= best_next.valid();

        double rad;

        if (best_prev_valid && best_next_valid) {
            // Average of surrounding vectors
            //
            double prev_rad= best_it->rad();   // vector from N-1 to N
            double next_rad= best_next->rad();  // vector from N to N+1

            rad= (prev_rad + next_rad) / 2.0;

        } else {
            // First or last point; should not really even give labels to
            // such?
            //
            if (best_prev_valid) {
                rad= best_it->rad();
            } else {
                assert( best_next_valid );
                rad= best_next->rad();
            }
        }

        // Adjust to [-pi/2,pi/2) range; no text shall be presented upside-down.
        //
        if (rad>=M_PI/2.0) rad -= M_PI;
        else if (rad<-M_PI/2.0) rad += M_PI;

        *tilt_rad_ref= rad;     // -M_PI/2 .. M_PI/2
    }

    if (goodness_ref)
        *goodness_ref= best;

    return best_it;
}


/*
*/
bool Labelizer::ChainChunk::intersects( const PointAndVector &v, Labelizer::Drawer &debug_drawer ) const {
 
    // Check boundary box first
    //
    if (bb.detached(v))
        return false;

    for( vector<PointScore>::const_iterator it= curve.begin();
        it != curve.end();
        ++it ) {
        if (it->intersects(v)) {
            debug_drawer.mark_vector( *it, "red" );
            return true;
        }
    }
    return false;   
}


/*
*/
bool Labelizer::ChainChunk::intersects( const TiltedRect &tr, Labelizer::Drawer &debug_drawer ) const {
 
    // Check boundary box first
    //
    if (bb.detached(tr.get_bounding_box()))
        return false;

    for( vector<PointScore>::const_iterator it= curve.begin();
        it != curve.end();
        ++it ) {
        // cached vector is from the earlier point to us, so we cannot use it directly
        //
        PointAndVector pv( it->p - *it, *it );

//debug_drawer.mark_vector( pv, "blue" );
        if (tr.intersects(pv)) {
//LOG_DEBUG( "%g %g -> %g %g", pv.p.x, pv.p.y, pv.get_dx(), pv.get_dy() );
debug_drawer.mark_vector( pv, "red" );
            return true;
        }
    }
    return false;   
}


/*
* Does a vector intersect any parts of a chain?
*
* We DON'T check the bounding box; already done by 'intersects(TiltedRect&)'
* that is calling us.
*
* Note: Marking 'const' would require a separate 'const iterator' to be
*       arranged. Mañana.. (TBD)
*/
bool Labelizer::Chain::intersects( const PointAndVector &v ) /*const*/ {

    assert( st != ST_BUSY );

    // If 'TiltedRect' is outside of the chain's bounding box, there's obviously
    // no intersection.
    //
debug_drawer.mark_bb( bb );
    if (bb.detached(v))
        return false;

    for( vector<ChainChunk>::const_iterator it= chunks.begin();
        it != chunks.end();
        ++it ) {
        if (it->intersects( v, debug_drawer ))
            return true;    // We don't care where the intersection was
    }
    return false;
}


/*
* Does a TiltedRect intersect any parts of a chain?
*/
bool Labelizer::Chain::intersects( const TiltedRect &tr ) /*const*/ {

    assert( st != ST_BUSY );

    // If 'TiltedRect' is outside of the chain's bounding box, there's obviously
    // no intersection.
    //
    if (bb.detached(tr.get_bounding_box()))
        return false;

    // Iterate by the chunks, since they each have a boundary box (optimizes,
    // compared to using 'ChainIterator')
    //
    for( vector<ChainChunk>::const_iterator it= chunks.begin();
        it != chunks.end();
        ++it ) {
debug_drawer.mark_tr(tr,"yellow");
        if (it->intersects( tr, debug_drawer )) {
            return true;    // We don't care where the intersection was
        }
debug_drawer.mark_tr(tr,"blue");
    }
    
    return false;
}


// 01-Feb-2012 PKi: Load Labelizer's default configuration (taken from config file)
//
const Labelizer::Config & Labelizer::loadDefaultCfg( lua_State *L, int idx )
{
	return Labelizer::loadCfg( L, idx, Labelizer::lblCfg, false);
}

// 01-Feb-2012 PKi: Load Labelizer's user configuration
//
const Labelizer::Config & Labelizer::loadUserCfg( lua_State *L, int idx, Labelizer::Config &lCfg )
{
	return Labelizer::loadCfg( L, idx, lCfg, true);
}

// 16-Dec-2011 PKi: Load Labelizer configuration from lua stack.
//
//                  uConfig is false when called for configuration loaded from config file
//                  (lCfg is reference to Labelizer::lblCfg and Labelizer::cfp field addresses can be
//                  used directly); otherwise (user given configuration) field offsets must be used
//
const Labelizer::Config & Labelizer::loadCfg( lua_State *L, int idx, Labelizer::Config &lCfg, bool uConfig )
{
    const Labelizer::_configFlds *lcfp = Labelizer::getConfigFlds();
	char *cpf = (char *) &Labelizer::lblCfg,*cpu = (char *) &lCfg;
    int n= 0;

    lua_pushnil(L);     // first key
    while( lua_next(L,idx) ) {
        // [-1]: value
        // [-2]: key
        n++;

        const char *key= lua_tostring(L,-2);
        L_ASSERT(key);

    	const Labelizer::_configFlds *cf = lcfp;
        for (; cf->fld; cf++)
            if (!strcmp(key,cf->fld))
            	break;

        if (! (cf->fld))
            luaL_error( L, "Labelizer config: unknown key %s", key );
        else if (lua_isnumber(L,-1) && (cf->vt==Labelizer::T_DOUBLE))
        	if (uConfig)
        		*(double *)(cpu + ((char *) cf->dp - cpf)) = lua_tonumber(L,-1);
        	else
        	    *(cf->dp) = lua_tonumber(L,-1);
        else if (lua_isboolean(L,-1) && (cf->vt==Labelizer::T_BOOL))
        	if (uConfig)
        		*(bool *)(cpu + ((char *) cf->bp - cpf)) = (lua_toboolean(L,-1) ? true : false);
        	else
        	    *(cf->bp) = (lua_toboolean(L,-1) ? true : false);
        else if (lua_isnumber(L,-1) && (cf->vt==Labelizer::T_INT))
        	if (uConfig)
        		*(int *)(cpu + ((char *) cf->ip - cpf)) = lua_tointeger(L,-1);
        	else
        	    *(cf->ip) = lua_tointeger(L,-1);
        else
            luaL_error( L, "Labelizer config: invalid value type %s for %s", lua_typename(L, lua_type(L, -1)), key );

        lua_pop(L,1);   // remove value, keep key for next iteration
    }

    // Return default configuration if config was empty
    //
    return (n ? lCfg : Labelizer::lblCfg);
}

// 16-Dec-2011 PKi: Push Labelizer configuration onto lua stack
//
int Labelizer::pushLabelizerCfg( lua_State *L )
{
	const Labelizer::_configFlds *cf = Labelizer::getConfigFlds();

    lua_newtable(L);

    for (; cf->fld; cf++)
    {
        lua_pushstring(L,cf->fld);

        if (cf->vt == Labelizer::T_DOUBLE)
            lua_pushnumber(L, *(cf->dp));
        else if (cf->vt == Labelizer::T_BOOL)
            lua_pushboolean(L, *(cf->bp) );
        else
            lua_pushinteger(L, *(cf->ip) );

        lua_settable(L,-3);
    }

    return 1;
}

// 01-Feb-2012 PKi: Returns Labelizer configuration field table
//
const Labelizer::_configFlds * Labelizer::getConfigFlds()
{
    static Labelizer::_configFlds labelCfgFlds[] = {
    		{ (const char*) "limitalongline", Labelizer::T_DOUBLE, &Labelizer::lblCfg.limit_along_line, NULL, NULL },
    		{ (const char*) "limitdirect", Labelizer::T_DOUBLE, &Labelizer::lblCfg.limit_direct, NULL, NULL },
    		{ (const char*) "allowoverlappinglines", Labelizer::T_BOOL, NULL, &Labelizer::lblCfg.allow_overlapping_lines, NULL },
    		{ (const char*) "allowoverlappinglabels", Labelizer::T_BOOL, NULL, &Labelizer::lblCfg.allow_overlapping_labels, NULL },
    		{ (const char*) "dampencornerslimitdeg", Labelizer::T_DOUBLE, &Labelizer::lblCfg.dampen_corners_limit_deg, NULL, NULL },
    		{ (const char*) "dampencorners", Labelizer::T_DOUBLE, &Labelizer::lblCfg.dampen_corners, NULL, NULL },
    		{ (const char*) "boosthorizontallimitdeg", Labelizer::T_DOUBLE, &Labelizer::lblCfg.boost_horizontal_limit_deg, NULL, NULL },
    		{ (const char*) "boosthorizontal", Labelizer::T_DOUBLE, &Labelizer::lblCfg.boost_horizontal, NULL, NULL },
    		{ (const char*) "curveoptimizationfactor", Labelizer::T_DOUBLE, &Labelizer::lblCfg.curve_optimization, NULL, NULL },
    		{ (const char*) "decimals", Labelizer::T_INT, NULL, NULL, &Labelizer::lblCfg.decimals },
    		{ NULL, Labelizer::T_NONE, NULL, NULL, NULL }
    };

    return labelCfgFlds;
}
