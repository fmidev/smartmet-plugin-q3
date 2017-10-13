/*
* TRACKEDDATA.H                 Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Presents a certain SQD/SQD.BZ2/MQD file (read-only) caught from a file
* mask and origintime.
*/
#ifndef TRACKEDDATA_H
#define TRACKEDDATA_H

#include "NA_Data.h"
#include "Tools.h"

/*
* There is only one 'Data' instance for each available SQD or BZ2 file.
* Multiple users will share that, and 'refcount' keeps us aware of them.
*
* 'TrackerBase' will regularily call 'Data::Wipe()' which may close open
* files (that haven't been used in a while).
*
* The 'qd' data portion is kept open even if refcount reaches zero; the
* same data may be needed in a short while, and then we already have it
* mapped.
*
* 'last_acquired' and 'meta_acquired' fields are set when the data is acquired; they are used to
* find data to wipe out.
*/
class TrackedData {
  public:
    TrackedData( const char *fn, const JDay &ot_given_, const string_or_null &tmp_pattern_, size_t tmp_threshold ) throw (E_BAD_FILE, E_USAGE);
    TrackedData( const char *fn, const NA_Info &info_given, const string_or_null &tmp_pattern_=0, size_t tmp_threshold_=0 ) throw (E_USAGE);
    ~TrackedData();

    NA_Data *Acquire(bool metaQuery = false) throw(E_BAD_FILE, E_BUG);   // get data with increasing reference count
                               // and automatic (on demand) expansion of BZ2 data
#ifdef METQU
    void Release() {}   // nothing to do (no refcounts)
#else
    void Release();
#endif

    const std::string &getSource() const { return source; }
    const NA_Info &getInfo() const;
    JDay getOriginTime() const { return ot_given ? ot_given : getInfo().getOriginTime(); }

  private:
    // For 'Sqd_Tracker' and 'BZ2_Tracker' only:
    //
#ifndef METQU
    friend class TrackerBase;
    friend class Bz2_Tracker;
    friend class Sqd_Tracker;
    friend class Mqd_Tracker;

    bool Wipe( unsigned wiping=0, unsigned metawiping=0 ); // wiping==0 : force
#endif
    void Wipe_LOCKED();

    // static data
    //
    static const char *four_Xs;

    // data fields
    //
    const std::string source;   // source filename (.sqd, .sqd.bz2 or .mqd)
    mutable volatile const NA_Info *info_;       // originally NULL, loaded on demand by 'getInfo()'

    JDay ot_given;            // origintime 'known' in the constructor (i.e. from cache)
                              // (if unknown, we must load 'info' to get the origintime)
 
    // Locked by 'qd_m' (in server mode)
        NA_Data *qd;         // use 'Acquire'/'Release' to grab a hold of this

        string_or_null tmp_fn;   // if BZ2 which is extracted to disk, this holds the extraction filename (for removal)
    
#ifndef METQU
    Mutex qd_m;
        volatile uint_t refcount;
        volatile time_t last_acquired;     // time stamp
        volatile time_t meta_acquired;     // time stamp
#endif

    const string_or_null tmp_pattern;   // if non-NULL, try to extract BZ2 to disk
    const size_t tmp_threshold;         // amount of free space to remain on 'tmp_pattern'

    // Don't allow (would render refcount useless)
    //
    TrackedData( const TrackedData & );
    TrackedData & operator=( const TrackedData & );

#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        const char *tp= tmp_pattern.c_str();
        if (tp) {
            assert_invariant( ends_with( tp, four_Xs ) );
        }
        
        const char *fn= tmp_fn.c_str();
        if (fn) {
            assert_invariant( tmp_pattern.c_str() );    // must have had a template
            assert_invariant( qd );                     // must be open if 'tmp_fn' is there
        }
    }
#endif
};

#endif
    // TRACKEDDATA_H
