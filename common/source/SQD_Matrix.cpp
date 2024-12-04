/*
* SQD_Matrix.CPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Revised:  22-Oct-2010
*/
#include "SQD_Matrix.h"
#include "SQD_Tools.h"
#include "SQD_Data.h"

#include "newbase/NFmiFastQueryInfo.h"
#include "newbase/NFmiParameterName.h"

#include "NA_Level.h"
#include "MoreTools.h"

using namespace std;


/*---=== Helpers ===---*/


/*
* Initialise an iterator.
*
* The time, level and parameter are _guaranteed_ to be covered by 'qd'; 
* otherwise an 'SQD_Matrix' would not have been created.
*
* Returns: non-nullptr pointer allocated by 'new' (if parameter existed)
*       nullptr if parameter did not exist
*/
static NFmiFastQueryInfo *new_iter( NFmiQueryData *qd, const JDay &vt, const NA_Level &lev, FmiParameterName e )
{
    assert(qd);

    NFmiFastQueryInfo *fi= new NFmiFastQueryInfo(qd);
    fi->Reset();

    bool ok= fi->Time( SQD_Tools::jd2mt(vt) );
    assert(ok);
#ifdef NDEBUG
	(void) ok;
#endif

    if ((!lev) || lev.isGroundLevel()) {
        ok= fi->FirstLevel();
        assert(ok);
    } else {
        ok= fi->Level( SQD_Tools::newbase_level(lev) );
        assert(ok);
    }

    if (!fi->Param( NFmiParam(e) )) {
        delete fi;
        return nullptr;    // no such param in data
    }

    return fi;
}


/*
* 'WD' param has special values (when stored in SQD file):
*       0.0 means no wind (also WS==0.0)
*       360.0 means North (implying WS>0)
*
* This is rather difficult for us and the idea of tying two parameters together like this causes
* all kinds of complications (i.e. setting 'WS' to 0 should implicitly also set 'WD' in those
* points).
*
* What we do is keep the stored range but forget about the synchronization between WD and WS.
* So, writing 0 (North) to an SQD file will have the value stored as 360. This should provide
* enough backwards compatibility, while retaining the complication issues sealed within SQD
* format handling (and not spread to rest of q3).
*
* Note: Even with 'SQD_WD_WS_TRACKING' enabled, making full synchronization is a shitty job.
*       One should track not only changes to WD, but also to WS since setting WS to NAN/non-NAN
*       would have implications on the WD side of things. REALLY NOT WORTH THE BOTHER.
*       --AKa 15-Dec-10
*/
static float get_WD( float v ) {    // SQD file -> script (360 -> 0)
    return fmodf( v, 360.0f );
}

static float set_WD( float v ) {    // script -> SQD file (0 -> 360)
    return (v==0.0) ? 360.0f : v;
}


/*---=== SQD_Matrix ===---*/

#ifdef METQU
SQD_Matrix::SQD_Matrix( SQD_Data *data, const JDay &vt, const NA_Level &lev, FmiParameterName e )
: Matrix( data->getGridSize(), SQD_Tools::unit_by_id(e), data->isReadOnly() ),
  fi( new_iter( data->getQD(), vt, lev, e ) ),
  proj( data->getProjection() ),
  get_f(0), set_f(0)
# ifdef SQD_WD_WS_TRACKING
  ,my_e(e), fi_WD(0), fi_WS(0)
# endif
{ 
    if (!fi) throw E_NO_MATCH();

    if (e==kFmiWindDirection) {
        get_f= get_WD;
        set_f= set_WD;
    }

# ifdef SQD_WD_WS_TRACKING
    if (e==kFmiWindVectorMS) {
        fi_WD= new_iter( data->getQD(), vt, lev, kFmiWindDirection );
    }
    if ((e==kFmiWindDirection) || (e==kFmiWindVectorMS)) {
        fi_WS= new_iter( data->getQD(), vt, lev, kFmiWindSpeedMS );
    }
# endif
    
    INVARIANT();
}
#endif

SQD_Matrix::SQD_Matrix( const SQD_Data *data, const JDay &vt, const NA_Level &lev, FmiParameterName e )
  : Matrix( data->getGridSize(), SQD_Tools::unit_by_id(e), true /*read-only*/ )
#ifdef METQU
  , fi( new_iter( const_cast<SQD_Data*>(data)->getQD(), vt, lev, e ) )
#else
  , fi( new_iter( data->getQD(), vt, lev, e ) )
#endif
  , proj( data->getProjection() ) 
  , get_f(0), set_f(0)
# ifdef SQD_WD_WS_TRACKING
  ,my_e(e), fi_WD(0), fi_WS(0)
# endif
{ 
    if (!fi) throw E_NO_MATCH();

    if (e==kFmiWindDirection) {
        get_f= get_WD;
        set_f= set_WD;
    }

# ifdef SQD_WD_WS_TRACKING
    if (e==kFmiWindVectorMS) {
        fi_WD= new_iter( const_cast<SQD_Data*>(data)->getQD(), vt, lev, kFmiWindDirection );
    }
    if ((e==kFmiWindDirection) || (e==kFmiWindVectorMS)) {
        fi_WS= new_iter( const_cast<SQD_Data*>(data)->getQD(), vt, lev, kFmiWindSpeedMS );
    }
# endif
    
    INVARIANT();
}

/*
*/
SQD_Matrix::~SQD_Matrix() {
    INVARIANT();

    delete fi;

#ifdef SQD_WD_WS_TRACKING
    delete fi_WD;
    delete fi_WS;
#endif
}

/*
*/
float SQD_Matrix::get_value_n( offset_t n ) const noexcept {
    fi->LocationIndex(n); 
    float v= fi->FloatValue();

#ifdef SQD_WD_WS_TRACKING
    if (fi_WD) { fi_WD->LocationIndex(n); }
    if (fi_WS) { fi_WS->LocationIndex(n); }
    
    // Check validity of WVEC values
    //
    if (my_e==kFmiWindVectorMS) {
        assert(fi_WD && fi_WS);

        float v_WD= fi_WD->FloatValue();
        float v_WS= fi_WS->FloatValue();

        // Data consistency rules for WVEC:
        //        
        // If WS is missing, also WVEC is (and vice versa)
        // If WD is missing, WVEC has 00 as the lowest two digits (mod 100)
        //
        // WVEC is 100*round(WS) + WD/10
        //
        bool bad= false;

        int v_int= (int)v;

        if (v==32700.0f) {
            bad= v_WS != 32700.0f;
        } else if (v_WS == 32700.0f) {
            bad= true;  // WVEC existed but WS missing
        } else {
            if ((v_WD==32700.0f) && (v_int%100 != 0)) {
                bad= true;  // WD missing but WVEC direction part != 0
            } else {
                // All three values exist. Is the formula right?
                //
                bad= ((v_int%100)*10 != v_WD) || ((v_int/100) != floorf(v_WS+0.5));
            }
        }

        if (bad) {
            // TBD: There's too many of these to keep the logging enabled. Newbase seems to be at fault.
            /*
            * WARNING ... Inconsistent WVEC: 1300.000000 (WS 13.400000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1400.000000 (WS 13.900000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1200.000000 (WS 11.800000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1200.000000 (WS 12.300000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1300.000000 (WS 12.800000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1300.000000 (WS 13.400000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1100.000000 (WS 11.300000, WD 360.000000)
            * WARNING ... Inconsistent WVEC: 1200.000000 (WS 11.900000, WD 360.000000)
            * ...
            */
            
            //LOG_WARNING( "Inconsistent WVEC: %f (WS %f, WD %f)", v, v_WS, v_WD );
        } else {
            // When 'WD' is other than 360, WVEC seems fine.
            //
            //LOG_DEBUG( "WVEC fine: %f (WS %f, WD %f)", v, v_WS, v_WD );
        }
    }
#endif

    return (v==32700.0f) ? NAN : (get_f ? get_f(v) : v);
}

/*
*/
#ifdef METQU
void SQD_Matrix::set_value_n( offset_t n, float v ) noexcept {
    assert( !isReadOnly() );    // upper levels should have checked

    fi->LocationIndex(n); 
    fi->FloatValue( isnanf(v) ? 32700.0f : (set_f ? set_f(v) : v) );
}
#endif

