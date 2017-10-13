/*
* TRACKEDDATASET.CPP                    Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#ifndef METQU
# error "Not for server mode"
#endif

#include "TrackedDataSet.h"

#include "TrackedData.h"

#include <set>
#include <algorithm>
    // sort

using namespace std;

LuaNew_ID TDSBind::ID;

static const unsigned GB= 1024*1024;

static const string tmp_pattern( "/tmp/metqu-XXXXXX" );    // Where to place extracted BZ2 files
static const size_t tmp_threshold= 1*GB;                   // How much free should remain on that mount


/*
*/
TrackedDataSet::TrackedDataSet( const char *fn_mask ) throw(E_USAGE) 
    : line()
{
    set<JDay> used_ots;

    const char *fn_abs;
#ifdef UNIX
    glob_t gbuf;
    int gbuf_i= -1;   // first round
/*
typedef struct {
size_t gl_pathc;    // Count of paths matched so far
char **gl_pathv;    // List of matched pathnames.
size_t gl_offs;     // Slots to reserve in 'gl_pathv'.
} glob_t;
*/
    while( (fn_abs= glob_fn( fn_mask, gbuf, gbuf_i )) != NULL )
#else
# error "Not implemented for Win32"
#endif
    {
        TrackedData *td;
        JDay jd;    // undefined
        
        try {
            td= new TrackedData( fn_abs, jd, tmp_pattern, tmp_threshold );
        }
        catch( const E_BAD_FILE &e ) {
            LOG_WARNING( "Ignoring %s: %s", fn_abs, e.what() );
            continue;
        }

        // Check that the origintimes are unique
        //
        JDay ot= td->getOriginTime();
        assert(ot);     // 'TrackedData' (or lower levels) should have checked that (and given E_BAD_FILE)

        set<JDay>::const_iterator it= used_ots.find(ot);
        if (it != used_ots.end()) {
            LOG_WARNING( "Ignoring %s: same origintime as another file", fn_abs );
            continue;
        }
        used_ots.insert(ot);
        
        // Add to the vector (not sorted, yet)
        //
        line.push_back(td);
    }

    struct my_sort {
        /*
        * Return 'true' is 'a' is to be ahead of 'b' in the vector.
        */
        static bool f(TrackedData* a, TrackedData *b) {
            assert( a && b );
            JDay ot_a= a->getOriginTime();
            JDay ot_b= b->getOriginTime();
            assert( ot_a && ot_b );
            assert( ot_a != ot_b );     // no two entries are the same

            return ot_a > ot_b;
        };
    };

    // Sort the vector (latest origintime first)
    //
    sort( line.begin(), line.end(), my_sort::f );

#if 1
    JDay ot_last;
    for( std::vector<TrackedData*>::const_iterator it= TrackedDataSet::line.begin();
        it != TrackedDataSet::line.end();
        it++ ) {
        JDay ot= (*it)->getOriginTime();

        assert( (!ot_last) || (ot < ot_last) );
        //LOG_DEBUG( "%s", ot.tostring().c_str() );
        
        ot_last= ot;
    }
#endif

    INVARIANT();
}

/*
*/
TrackedDataSet::~TrackedDataSet() {
    INVARIANT();

    for( std::vector<TrackedData*>::iterator it= line.begin();
        it != line.end();
        it++ ) {
        delete *it;
    }
}

/*
* Find the next suitable entry in the data set.
*
* 'i':      Set to zero by the caller before the iteration (we update it
*           to point to the next entry to consider after the returned one).
*
* 'ot'==0:  Any origintime is good, progress from latest (first) to oldest
* 'ot'!=0:  Only a specific origintime is good (max one hit only)
*
* Note: The returned pointers remain valid (and unchanged) as long as
*       the 'TrackedDataSet' object does. They need not and MUST NOT BE 
*       DELETED by the caller.
*/
TrackedData *TrackedDataSet::getData( unsigned &i, const JDay &ot ) const throw() {

    unsigned n= line.size();

    while( i<n ) {
        TrackedData *td= line[i];
        ++i;

        JDay td_ot= td->getOriginTime();

        if ((!ot) || (td_ot==ot)) {
            return td;
        } else if (td_ot<ot) {
            break;  // went past
        }
        // continue search
    }
    return NULL;    // no match
}


/*
* Return the the X+1'th latest data.
*
* 0: latest, 1: 2nd latest, ...
*/
TrackedData *TrackedDataSet::getData_latest( unsigned i ) const throw() {

    if (i<line.size()) {
        return line[i];     // [0] is the most recent
    } else {
        return NULL;        // nothing so old
    }
}


