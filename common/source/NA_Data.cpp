/*
* NA_DATA.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  19-Oct-10 AKa 
*/
#include "NA_Data.h"

#include "MemMatrix.h"

#include <vector>

using namespace std;

/*
* Note: These strings are used only as runtime keys. Feel free to rename without
*       Dire Consequences. Anything unique (among themselves) will do.
*/
const char *NA_Info::EXTRA_KEY_FILENAME=     "filename";

#ifdef USE_NEWBASE
const char *NA_Info::EXTRA_KEY_SQD_PRODUCER= "sqd_producer";
const char *NA_Info::EXTRA_KEY_SQD_GRIDSIZE= "sqd_gridsize";
const char *NA_Info::EXTRA_KEY_SQD_COMBO19=  "sqd_combo19";
const char *NA_Info::EXTRA_KEY_SQD_COMBO326= "sqd_combo326";
const char *NA_Info::EXTRA_KEY_SQD_REVLEVORDER= "sqd_revlevorder";
const char *NA_Info::EXTRA_KEY_SQD_RELATIVE_UV= "sqd_relative_uv";
#endif


/*---=== NA_Info ===---
*/

NA_Info::NA_Info( 
        const time_t loadTime_,
        const time_t modificationTime_, 
        const JDay &ot_, 
        const vector<JDay> &times_,
        const string & nativeLevelType_, const vector<NA_Level> &levels_,
        const vector<NA_Param> &params_,
        const Projection &projection_,
        const string & wkt_,
        const uint timeStep_,
        const double dx_,
        const double dy_,
		const bool gridData_
        ) throw()
: loadTime(loadTime_),
  modificationTime(modificationTime_),
  ot( ot_ ), 
  times( times_ ),
  nativeLevelType(nativeLevelType_), levels( levels_ ),
  params( params_ ),
  proj( projection_ ), 
  wkt( wkt_ ),
  timeStep( timeStep_ ),
  dx(dx_),
  dy(dy_),
  gridData(gridData_),
  //
  extra( /*extra_*/ )
{ 
    INVARIANT();
}


/*
*/
string_or_null NA_Info::getExtra_str( const char *key ) const {
    map<string,string>::const_iterator it= extra.find(key);
    if (it==extra.end()) {
        return NULL;
    }
    return it->second;
}

NA_Info &NA_Info::setExtra( const char *key, const char *v ) {
    extra[ key ]= v;
    return *this;
}

/*
*/
int NA_Info::getExtra_int( const char *key, int def ) const {
    string_or_null s= getExtra_str(key);
    if (!s.c_str()) { return def; }

    char *endp;
    int ret= strtol( s.c_str(), &endp, 10 );
    if (*endp) {
        throw E_LOG_USAGE( "Bad syntax in extra (expected an integer): '%s=%s'", key, s.c_str() );
    } else {
        return ret;
    }
}

NA_Info &NA_Info::setExtra( const char *key, unsigned v ) {
    string s= string_fmt( "%d", v );
    return setExtra( key, s.c_str() );
}

/*
*/
MatrixPos NA_Info::getExtra_pos( const char *key ) const {
    string_or_null s= getExtra_str(key);
    
    unsigned x,y;
    if (sscanf( s.c_str(), "%d,%d", &x, &y ) != 2) {
        return MatrixPos::ZERO;     // no such entry
    }
    return MatrixPos( x,y );
}

NA_Info &NA_Info::setExtra( const char *key, const MatrixPos &pos ) {
    string s= string_fmt( "%d,%d", pos.getX(), pos.getY() );
    return setExtra( key, s.c_str() );
}

/*
*/
bool NA_Info::getExtra_bool( const char *key ) const {
    string_or_null s= getExtra_str(key);
    return s == "true";
}

NA_Info &NA_Info::setExtra( const char *key, bool v ) {
    return setExtra(key, v ? "true" : "false");
}

//
// 31-Aug-2011 PKi: Moved here from SQD_Data.cpp
//
vector<NA_Level> reverse_vector( const vector<NA_Level> &vec ) {
    vector<NA_Level> vec2( vec.size() );
    unsigned n= vec.size();

    for( unsigned i=0; i<n; i++ ) {
        vec2[n-1-i]= vec[i];
    }
    return vec2;
}

/*
* Progress callback when writing out a 'NA_Data' object.
*
* Calls:
*   cancel_bool= func( progress_num 0..100, secs_num, total_secs_num, a_str, b_str, c_str )
*/
#ifdef METQU
bool /*cancel*/ NA_Data::ProgressCallback::progress( unsigned long current, unsigned long total, const char *a, const char *b, const char *c ) {

    if (!f_index) return false;     // no callback enabled

    assert(L);

    // Count estimated time of arrival
    //
    double secs= (now_ms() - t0) / 1000.0;

    double eta_secs= ((!current) || (secs==0.0)) ? 10.0   // no progress yet (cannot make an estimate)
                        : (((double)total) / current) * secs;

    double prc= (((double)current) / total) * 100.0;

//LOG_DEBUG( "Current %d, total %d, prc %lf", (int)current, (int)total, prc );

    bool ret;

L_START
    L_GROW(7);
    lua_pushvalue( L, f_index );    // 2nd ref of the function
    
    lua_pushnumber( L, prc );
    lua_pushnumber( L, secs );
    lua_pushnumber( L, eta_secs );    // note: this varies during the progress

    lua_pushstring( L, a );
    lua_pushstring( L, b );
    lua_pushstring( L, c );
    
    // Error within the progress function will trigger a C++ exception here.
    //
    int st= lua_pcall( L, 6 /*args*/, 1 /*retvals*/, 0 /*no errfunc*/ );
        //
        // 0 (ok)
        // LUA_ERRRUN
        // LUA_ERRMEM
        // (LUA_ERRERR)

    if (st!=0) {
        throw E_LOG_USAGE( "Runtime error in progress callback: %s", lua_tostring(L,-1) );
    }    

    ret= lua_toboolean(L,-1);
    lua_pop(L,1);
L_END(0)

    return ret;
}
#endif


/*
* Perform projection and/or scaling of SQD/MQD/other raw data.
*
* This is the default projection function and can be overridden by certain data provider modules
* (classes derived from 'NA_Data'). I.e. SQD format overrides this to handle Newbase projection
* strings.
*/
/*virtual*/ const Matrix *NA_Data::push_Matrix( lua_State *L, 
                                const JDay &vt, 
                                const NA_Level &lev, 
                                const NA_Param &p, 
                                const Projection &target_proj_orig,
                                const MatrixPos &target_gs_orig,
                                const DataIdList *dataIds ) const {
    assert(L);

	bool target_ready = false;
    const Matrix *m_native= 
#ifdef METQU
	push_NativeMatrix_const( L, vt, lev, p
#else
    push_NativeMatrix( L, vt, lev, p
#endif

#ifdef USE_NEWBASE_PROJ

		// 22-Aug-2011 PKi: Fetch SQD data using newbase interpolation, projection and scaling

		, &target_proj_orig, &target_gs_orig, &target_ready, dataIds

#endif
	);

    if ((!m_native) || target_ready) {
        return m_native;
    }

#ifndef NDEBUG
    NA_Param::e_Interpolation method= m_native->getUnit().getMethod();
    
    if (method == NA_Param::INTERPOLATE_UNKNOWN) {
        string name= p.toString( false /*prefer native names*/ );

        throw E_LOG_BUG( "Shouldn't be here with unknown interpolation (%s)", name.c_str() );
    }
#endif

    const Projection &native_proj= getProjection();
    const MatrixPos native_gs= m_native->getGridSize();

    const Projection &target_proj= target_proj_orig ? target_proj_orig : native_proj;
    const MatrixPos target_gs= target_gs_orig ? target_gs_orig : native_gs;

    if ((target_proj == native_proj) && (target_gs == native_gs)) {
        return m_native;    // on top of Lua stack

    } else {
        MemMatrix *m= new(L) MemMatrix( target_gs, p.getUnit(), target_proj );

        m->fit_from_( *m_native );

        lua_remove(L,-2);   // 'native_m' away

        m->set_readonly();
        return m;
    }
}

