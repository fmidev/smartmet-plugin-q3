/*
* PROJECTION.CPP                          Copyright (c) 2010, Ilmatieteen laitos
*/
#include "Projection.h"
    
#ifdef USE_NEWBASE
# include "SQD_Projection.h"
#endif

#include "Proj4_Projection.h"

using namespace std;

const Projection Projection::NONE;


/*---=== Helpers ===---*/



/*---=== Projection ===---*/

/*
*/
Projection::Projection( const char *proj_ ) throw(E_USAGE) : proj(), creation_str(proj_ ? proj_:"") {
    string reason;

    // Empty projection? (needed by 'TestRaw')
    //
    if (!proj_ || (!*proj_)) {
        *this= Projection::NONE;
        goto CHECK_INVARIANT;
    }
    
#ifdef USE_NEWBASE
    try {
        proj= new SQD_Projection( proj_ );   // throws 'runtime_error' if string not recognized
        goto CHECK_INVARIANT;
    }
    catch( const exception &e ) {
        reason= e.what();
        
        // go on trying with Proj4
    }
#endif

    // Proj4 projections
    //    
    try {
        proj= new Proj4_Projection( proj_ );   // throws 'E_USAGE' if string not recognized
        goto CHECK_INVARIANT;
    }
    catch( const E_USAGE &e ) {
        // We can choose here to keep the Newbase error (already in 'reason') or overwrite with ours.
        //
        reason= e.what();
    }

    throw E_LOG_USAGE( "Bad projection: %s (%s)", proj_, reason.c_str() );

CHECK_INVARIANT:
    INVARIANT();
}

/*
*/
Projection::Projection( const Projection &o ) : proj( o.proj ? o.proj->clone_() : NULL ), creation_str( o.creation_str ) {

    INVARIANT();
}

/*
*/
Projection::~Projection() {
    delete proj;
}

/*
*/
Projection & Projection::operator=( const Projection &o ) {
    delete proj;
    proj= o.proj ? o.proj->clone_() : NULL;
    creation_str= o.creation_str;

    INVARIANT();
    
    return *this;
}


/*
* Return LatLon of a certain point within the grid.
*
* 'pos' is the point, within (0,0)..(gs.X()-1,gs.Y()-1)
* 'gs' is the grid size
*/
LatLon Projection::latlon( const MatrixPos &pos, const MatrixPos &gs ) const {

    double x_top= gs.getX()-1;
    double y_top= gs.getY()-1;
    
    return latlon( pos.getX()/x_top, pos.getY()/y_top );
}


