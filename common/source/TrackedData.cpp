/*
* TRACKEDDATA.CPP               Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#include "TrackedData.h"

#include <errno.h>
#include <string.h>

#include <sys/statvfs.h>

#ifdef USE_NEWBASE
# include "SQD_Data.h"
#endif
#ifdef MQD_ENABLED
# include "MQD_Data.h"
#endif

using namespace std;

// Four X's give 62^4 = 14776336 unique combinations of filenames
//
const char *TrackedData::four_Xs= "XXXX";


/*---=== Helpers ===---*/


/*---=== TrackedData ===---*/

/*
* Open a new tracker entry
*
* Note:
*       To actually load the data, caller must call 'Acquire'
*/
TrackedData::TrackedData( const char *fn, const JDay &ot_given_, const string_or_null &tmp_pattern_, size_t tmp_threshold_, bool relative_uv_ )
    : source(fn), info_(0), ot_given(ot_given_), qd(0), tmp_fn(0)
#ifndef METQU
    , qd_m(), refcount(0), last_acquired(0), meta_acquired(0)
#endif
    , tmp_pattern(tmp_pattern_), tmp_threshold(tmp_threshold_), relative_uv(relative_uv_)
{
    assert(fn);

    const char *tp= tmp_pattern_.c_str();
    if (tp && (!ends_with(tp,four_Xs))) {
        throw E_LOG_USAGE( "Temp file pattern must end with '%s': %s", four_Xs, tp );
    }

    INVARIANT();
}

TrackedData::TrackedData( const char *fn, const NA_Info &info_given, bool relative_uv_, const string_or_null &tmp_pattern_, size_t tmp_threshold_ )
    : source(fn), info_(new NA_Info(info_given)), ot_given(), qd(0), tmp_fn(0)
#ifndef METQU
    , qd_m(), refcount(0), last_acquired(0), meta_acquired(0)
#endif
    , tmp_pattern(tmp_pattern_), tmp_threshold(tmp_threshold_), relative_uv(relative_uv_)
{
    assert(fn);

    const char *tp= tmp_pattern_.c_str();
    if (tp && (!ends_with(tp,four_Xs))) {
        throw E_LOG_USAGE( "Temp file pattern must end with '%s': %s", four_Xs, tp );
    }

    INVARIANT();
}

/*
*/
TrackedData::~TrackedData() {
    INVARIANT();

    if (info_) delete info_;
    
#ifndef METQU
    qd_m.Destroy();
#endif

    Wipe_LOCKED();      // deletes 'qd' and removes 'tmp_fn' (if any)
    assert( qd==0 );
}

/*
* Load info on demand.
*
* Once loaded the info will remain until our destructor.
*/
const NA_Info &TrackedData::getInfo() const {

    static Mutex my_m;  // local lock needed only here

    // 'info_' only transitions from nullptr -> valid via us.
    //
    if (!info_) {
        ClaimMutex lock(my_m);  // automatically unclaims at scope exit

        // Recheck in case some other thread initialized it already (now that 
        // we're inside the lock)
        //
        if (!info_) {
            const char *fn= source.c_str();

            // These may throw 'E_BAD_FILE'
            //
#ifdef MQD_ENABLED
            // Try with MQD first, we're able to tell from the first line if it's an MQD file.
            //
            if (MQD_Data::is_mqd_file(fn)) {
                info_= new NA_Info( MQD_Data::read_info(fn) );
            }
#endif
#ifdef USE_NEWBASE
            if (!info_) {
                info_= new NA_Info( SQD_Data::read_info(fn) );
            }
#endif
            if (!info_) {
                throw E_LOG_USAGE( "Unable to open file: %s", fn );
            }
        }
    }

    // We can cast away the 'volatile' since now it's not going to change.
    // Seems the Way gcc 4.1.2 allows us to do this is 'const_cast<>'. Weird.
    //
    return * const_cast<const NA_Info*>(info_);
}

/*
*/
void TrackedData::Wipe_LOCKED() /*throw(E_FATAL)*/ {

#ifndef METQU
    // Haven't seen the problem for a while - most likely gone.
    //
    assert( refcount==0 );

    /*
    if (refcount>0) {
        // This seems to happen in development, when exceptions are thrown and
        // not caught properly (or something). SHOULD NOT HAPPEN AT PRODUCTION.
        //
        throw E_LOG_FATAL0( "Data is still being actively used" );
    }
    */
#endif

const char *fn_f = strrchr(source.c_str(),'/'); if (fn_f && *(fn_f + 1)) fn_f++; else fn_f = source.c_str();
LOG_OK( "Wipe_LOCKED(%d: %s)", (int) pthread_self(), fn_f );
    if (qd) {
        delete qd;
        qd= 0;
    }
    
    // If we have extracted the .BZ2 contents to a temporary file, now is the time
    // to remove it.
    //
    const char *fn= tmp_fn.c_str();
    if (fn) {
        remove_file(fn);
        tmp_fn= 0;
    }
}

/*
* Make sure the data is loaded, and increase its refcount.
*
* Note: The 'tmp_threshold' value defines how much free space is required to exist
*       BEFORE the extraction of a data, for it to be extracted on disk. It does NOT
*       guarantee keeping that much of free space - apply a wide safety margin!
*
* Throws: E_BAD_FILE if we don't have access rights to open the file (we may have had
*           rights to view it in the directory listing), or if some jerk has removed
*           the file manually (which is okay).
*/
NA_Data *TrackedData::Acquire(bool metaQuery) {

#ifndef METQU
    { ClaimMutex lock( qd_m );
#endif

    if (!qd) {
//printf("*** %s loading ***\n",source.c_str());
        const char *fn= 0;

        // Check if we're wished to extract a .BZ2 file on disk, instead of the default
        // (opening it in memory via Newbase).
        //
#if 1
        // TBD: we need to remake the whole archive/extraction code (use MQD with compression built-in to the format)
#else
        const char *tp= tmp_pattern.c_str();
        char buf[FILENAME_MAX];

        if (tp) {
            // Extract to temporary location, if there's enough space.
            //
            strncpy( buf, tp, sizeof(buf) );

            // Note: 'mkstemp()' both modifies 'buf' AND opens a file (in read-write mode)
            //      so we don't have to worry about two threads getting the same filename.
            //      (( Do NOT use the regular 'mktemp()' here. ))
            //
            int fd= mkstemp(buf);
            if (fd==-1) {
                // Just proceed and load the file via Newbase (as if 'tmp_pattern' was nullptr)
                //
                // EEXIST:  Could not create a unique temporary filename.
                // EINVAL:  Last _six_ characters were not 'X'.
                
                LOG_WARNING( "'mkstemp(%s)' failed, errno %d", tp, errno );
            } else {
                // Check if there's enough space
                //
                size_t av= available(fd);

LOG_DEBUG( "Extracting... (%ld bytes free space)", (long)av );
                
                if (av > tmp_threshold) {
                    long bytes= uncompress( fd, source.c_str() );
                    if (bytes < 0) {
                        LOG_WARNING( "Unable to extract %s -> %s (we'll load to memory instead)", source.c_str(), buf );
                    } else {
LOG_DEBUG( "Extracting done (%d bytes used)", (int)bytes );
                        fn= buf;
                        tmp_fn= fn;         // so 'Wipe_LOCKED()' will remove the file
                    }
                }
                
                close(fd);
                if (!fn) {
                    remove_file(buf);   // something went wrong; remove the stub
                }
            }
        }
#endif

        if (!fn) fn= source.c_str();

        qd= 0;
        try {
#ifdef MQD_ENABLED
            if (MQD_Data::is_mqd_file(fn)) {
                qd= new MQD_Data( fn );
            }
#endif
#ifdef USE_NEWBASE
            if (!qd) {
                qd= new SQD_Data( fn, relative_uv );
            }
#endif
        }
        catch( const E_BAD_FILE &e ) {
            // i.e. "Unable to read (Could not open '...' for reading):
            //       /smartmet/brainstorm/hirlamrcr/pinta/200911300936_hirlam_pinta.sqd"
            //
            LOG_WARNING( "Ignoring %s: %s", fn, e.what() );
            throw e;
        }
        catch(exception &e) {    // should not happen
            LOG_WARNING( "Ignoring %s: %s", fn, e.what() );
            throw E_BAD_FILE ( source, e.what() );
        }
    }

#ifndef METQU
    ++refcount;
    if (!metaQuery)
    {
//printf("*** %s set last_acquired=%ld ***\n",source.c_str(),time(nullptr));
        last_acquired= time(nullptr);
    }
    else if (meta_acquired == 0)
    {
//printf("*** %s set meta_acquired=%ld ***\n",source.c_str(),time(nullptr));
        meta_acquired= time(nullptr);
//last_acquired=meta_acquired;
    }
    //printf("*** Ack %s refcount %d\n",source.c_str(),refcount);
    }   // ClaimMutex
#endif

    return qd;
}


/*
* Release a data
*
* This will _only_ reduce the refcount; possible closing of the data
* (releasing its memory mapping) is done in wiping (server mode).
*/
#ifndef METQU
void TrackedData::Release() {

    ClaimMutex lock(qd_m);
    --refcount;
    //printf("*** Rel %s refcount %d\n",source.c_str(),refcount);
}
#endif


/*
* Wipe data
*
* If the data has not been acquired since 'wiping' and 'metawiping' seconds, and it's currently not
* in use, close it (releasing the memory mapping).
*
* 'wiping'==0 is a force flag; try to wipe unconditionally.
*
* Returns 'true' if the data was wiped ('qd'==0), 'false' if kept as is.
*/
#ifndef METQU
bool TrackedData::Wipe( unsigned wiping, unsigned metawiping )
{
    ClaimMutex lock(qd_m);      // automatically released on 'return'

    if (!qd) {
        return true;  // already wiped
    } 

    if (
        (refcount==0) &&
        (
         (wiping==0) ||
         (
          ((time(nullptr) - last_acquired) > (int)wiping) &&
          ((meta_acquired==0) || ((time(nullptr) - meta_acquired) > (int)metawiping))
         )
        )
       ) {
        Wipe_LOCKED();
        meta_acquired = 0;
        assert( qd==0 );
        return true;
    }
//printf("*** %s NOT wiped: refcnt=%ld, wip=%u, ldff=%ld, mwip=%u, mdff=%ld***\n",source.c_str(),refcount,wiping,time(nullptr) - last_acquired,metawiping,time(nullptr) - meta_acquired);
    return false;
}
#endif

