/*
* TRACKEDDATASET.H                    Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Group of files covered by a file mask. Used by 'Raw.cpp' to find a good match.
*
* No two files must have the same origintime.
*/
#ifndef TRACKED_DATASET_H
#define TRACKED_DATASET_H

#ifndef METQU
# error "Not for server mode"
#endif

#include "Tools.h"
#include "TrackedData.h"

#include <vector>
#include <string>

class TrackedDataSet;

// Seems we need to have such only for being able to push 'TrackedDataSet'
// to Lua GC (even though no-one gets to see it or access via Lua).
//
struct TDSBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State * ) {}     // no metamethods
    static const char *name() { return "TDS"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef TrackedDataSet CAST_T;

  private:
};

/*
* Class to present a set of files (usually from a certain producer).
*
* The files are expected to remain on the disk throughout our use (= not
* to be suddenly deleted).
*/
class TrackedDataSet : public LuaNew<TDSBind> {
  public:
    TrackedDataSet( const char *fn_mask ) throw(E_USAGE);
    ~TrackedDataSet();

    // Set 'i' to 0 before first iteration. Gives matching data until nullptr.
    //
    TrackedData *getData( unsigned &i, const JDay &ot ) const throw();    // 'empty' ot for any origintime

    std::vector<JDay> getOriginTimes() const throw();
    TrackedData *getData_latest( unsigned i ) const throw();

  private:
    /*const*/ std::vector< TrackedData* > line;      // in origintime order (latest first)

#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        JDay ot_last;
        for( std::vector<TrackedData*>::const_iterator it= TrackedDataSet::line.begin();
            it != TrackedDataSet::line.end();
            it++ ) {
            JDay ot= (*it)->getOriginTime();
            assert_invariant( ot && ((!ot_last) || (ot < ot_last)) );
            ot_last= ot;
        }
    }
#endif
};

#endif
    // TRACKED_DATASET_H
