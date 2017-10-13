/*
* RAW_METQU.CPP                       Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  15-Oct-10
*
* Functions of 'Raw' class that apply to METQU (command line) mode only.
*/
#include "Raw.h"
#include "Tools.h"
#include "Projection.h"
#include "TrackedDataSet.h"
#include "Grid.h"
#include "Proto.h"

#ifdef USE_NEWBASE
# include "SQD_Data.h"
#endif
#ifdef MQD_ENABLED
# include "MQD_Data.h"
#endif

#if (!defined(USE_NEWBASE)) && (!defined(MQD_ENABLED))
# error "Not using Newbase, neither MQD."
#endif

#include <vector>
#include <string>
#include <fstream>

using namespace std;


/*
* [raw_ud] [,err_str]= new_Raw_ro( fn_mask_str, { 
*                  [origintime= jday_ud|time_str,]
*                  [times= { jday_ud|time_str [, ...] },]
*                  [ground= true,]
*                  [hybrid= { uint [, ...] },]
*                  [hpa= { number [, ...] },]
*                  [height= true,]
*                  [params= { str [, ...] },]
*               })
*
* Return a Raw object from the given file mask, and other criteria.
*/
int RawBind::new_Raw_ro( lua_State *L ) {
    proto( L, "string, { origintime=[jday|time_str],"
                        "times=[{jday|time_str,...}],"
                        "ground=[true],"
                        "hybrid=[{uint,...}],"
                        "hpa=[{number,...}],"
                        "height=[true],"
                        "params=[{string,...}],"
                      "}" );

    const char *fn_mask= lua_tostring(L,1);
    const unsigned table_index= 2;

    JDay ot;
    vector<JDay> times;
    vector<NA_Level> levels;
    vector<ApiParam> params;
    bool height_true= false;

    L_GROW(2);

    try {
        lua_pushnil(L);     // first key
        while( lua_next(L,table_index)) {
            // [-1]: value
            // [-2]: key
    
            const char *key= lua_tostring(L,-2);
            if (!key) {
                return L_nilerr_fmt( "Unknown key: %s", L_typename(-2) );
            }
    
            if (strcmp(key,"origintime")==0) {
                ot= JDay(L,-1);
                if (!ot) {
                    return L_nilerr_fmt( "Bad origintime %s", L_string_or_typename(-1) );
                }
    
            } else if (strcmp(key,"times")==0) {
                times= vector_of_times( L, -1 );   // may throw E_USAGE
    
            } else if (strcmp(key,"params")==0) {
                params= vector_of_apiparams( L, -1 );
   
            } else if (strcmp(key,"height")==0) {
                height_true= lua_toboolean( L, -1 );
   
            } else if (NA_Level::lt_enum(key)) {
                one_or_many_levels( L, -1, key, levels );   // may throw E_USAGE

            } else {
                return L_nilerr_fmt( "Unknown key: %s", key );
            }     
            lua_pop(L, 1);   // remove value
        }
    } catch( const E_USAGE &e ) {
        // i.e. "times", "level" or "params" had a bad value
        //
        return L_nilerr( e.what_nosource() );
    }

    // Place the 'TrackedDataSet' to Lua GC maintenance, allowing it to be
    // GC'ed when the returned Raw object (which needs it) has.
    //
    TrackedDataSet &tds= * new(L) TrackedDataSet( fn_mask );
    const int tds_index= lua_gettop(L);

    // Note: Giving 'L' into the constructor means Lua _must_not_ do garbage
    //      collection while we're in there. Can we change this to avoid the 
    //      'L' parameter? (TBD)
    //
    int pushed= new_Raw( L, tds, ot, times, levels, params, height_true, MatrixPos::ZERO );
    
    if (pushed==1) {
        assert( Raw::instance(L,-1) );      // raw pushed (success)

        LuaNew_base::keep_alive( L, -1, tds_index );
    }
    
    // If 'pushed==2' there was no Raw to be received; 'tds' will be GC'ed by Lua.

    return pushed;
}

/*
* [raw_ud] [,err_str]= new_Raw_rw( [template_raw_ud], { 
*                  [origintime= jday_ud|time_str,]
*                  [times= { jday_ud|time_str [, ...] },]
*                  [ground= true,]
*                  [hpa= { number [, ...] },]
*                  [hybrid= { uint [, ...] },]
*                  [params= { str [, ...] },]
*                  [projection= str,]
*                   --
*                  [sqd_producer= uint,]
*                  [sqd_gridsize= MatrixPos,]
*                  [sqd_combo19= bool,]
*                  [sqd_combo326= bool,]
*               })
*
* Create a fresh in-memory Raw object, writeable to SQD/MQD file. Either base
* it on an existing template or give all fields from scratch.
*
* If any of the 'sqd_...' params is given, format will be SQD. Otherwise MQD.
*
* If based on template, the material will be initialized with data from the
* template object. If there is no template (or the template does not have
* particular data) material is initialized to NAN.
*/
int RawBind::new_Raw_rw( lua_State *L ) {

    proto( L, "[Raw], { origintime=[jday|time_str],"
                       "times=[{jday|time_str,...}],"
                       "ground=[true],"
                       "hpa=[{number,...}],"
                       "hybrid=[{uint,...}],"
                       "params=[{string,...}],"
                       "projection=[string],"
                       //
                       "sqd_producer=[uint],"
                       "sqd_gridsize=[MatrixPos],"
                       "sqd_combo19=[bool],"
                       "sqd_combo326=[bool],"
                       " }" );

    const Raw_interface *r_template= Raw_interface::instance(L,1);

    const int table_index= 2;

    JDay ot;
    vector<JDay> times;
    vector<NA_Level> levels;
    vector<ApiParam> params;
    string proj,nativeLevelType;

#ifdef USE_NEWBASE    
    unsigned sqd_producer= 0;   // 0: "unspecified producer" by 'NFmiProducerName.h'
    MatrixPos sqd_gs;
    bool sqd_combo19= false;
    bool sqd_combo326= false;
    
    bool sqd_any= false;     // 'true' if any 'sqd_...' field provided
#endif

    // Get defaults from the template
    //
    const NA_Info *temp_info= r_template ? r_template->getData() : 0;     // NA_Data is also an NA_Info
    const NA_Data *temp_data= r_template ? r_template->getData() : 0;     // 25-Oct-2011 PKi: For handling virtual parameters

    if (temp_info) {
        ot= temp_info->getOriginTime();
        times= temp_info->getTimes();

		// 31-Aug-2011 PKi: Get template's levels in original order (otherwise data will be copied between unequal levels)
        //
        // levels= temp_info->getLevels();

        temp_info->getLevels(levels, temp_info->getExtra_sqd_revlevorder());

        //params= tempinfo->getParams();

#ifdef USE_NEWBASE
        // 'sqd_producer' intentionally NOT copied from the source; we're producing new stuff here
        sqd_gs= temp_info->getExtra_sqd_gridsize();
        sqd_combo19= temp_info->getExtra_sqd_combo19();
        sqd_combo326= temp_info->getExtra_sqd_combo326();
#endif
    }

    try {
        // Parameters from script
        //
        lua_pushnil(L);     // first key
        while( lua_next(L,table_index)) {
            // [-1]: value
            // [-2]: key
    
            const char *key= lua_tostring(L,-2);
            if (!key) {
                return L_nilerr_fmt( "Unknown key: %s", L_typename(-2) );
            }
    
            if (strcmp(key,"origintime")==0) {
                ot= JDay(L,-1);
                if (!ot) {
                    return L_nilerr_fmt( "Bad origintime %s", L_string_or_typename(-1) );
                }
    
            } else if (strcmp(key,"times")==0) {
                times= vector_of_times( L, -1 );   // may throw E_USAGE
    
            } else if (strcmp(key,"params")==0) {
                params= vector_of_apiparams( L, -1 );

            } else if (strcmp(key,"projection")==0) {
                const char *s= lua_tostring(L,-1);
                if (!s) {
                    return L_nilerr_fmt( "Bad projection: %s", L_typename(-1) );
                }
                proj= string(s);    // make a copy

            } else if (NA_Level::lt_enum(key)) {
                one_or_many_levels( L, -1, key, levels );   // may throw E_USAGE
                    
#ifdef USE_NEWBASE                    
            } else if (strcmp(key,"sqd_producer")==0) {
                sqd_producer= lua_tointeger(L,-1);
                sqd_any= true;
    
            } else if (strcmp(key,"sqd_gridsize")==0) {
                sqd_gs= *MatrixPos::instance(L,-1);     // makes a copy
                sqd_any= true;

            } else if (strcmp(key,"sqd_combo19")==0) {
                sqd_combo19= lua_toboolean(L,-1);
                sqd_any= true;
    
            } else if (strcmp(key,"sqd_combo326")==0) {
                sqd_combo326= lua_toboolean(L,-1);
                sqd_any= true;
#endif    
            } else {
  //UNKNOWN_KEY:
                return L_nilerr_fmt( "Unknown key: %s", key );
            }     
            lua_pop(L, 1);   // remove value
        }
    } catch( const E_USAGE &e ) {
        // i.e. "times", "level" or "params" had a bad value
        //
        return L_nilerr( e.what_nosource() );
    }

    if (!ot) {
        luaL_error( L, "'origintime' missing" );
    }
    if (times.size()==0) {
        luaL_error( L, "'times' missing" );
    }
    if (levels.size()==0) {
        luaL_error( L, "'levels' missing" );
    }
    if (params.size()==0 && (!temp_info)) {
        luaL_error( L, "'params' missing" );
    }
    if (proj=="" && (!temp_info)) {
        luaL_error( L, "'projection' missing" );
    }

    // 25-Oct-2011 PKi: Skipping virtual parameters.
    //
    //                  TBD: Obtain parameter information (interpolation and unit) from template data for
    //                  the parameters it has (to be copied) instead of using known_parameters[].
    //                  Intepolation is set to linear for parameters not found in known_parameters[];
    //                  otherwise using it's information and it might not match actual template data.

    NA_Info info( time(NULL), time(NULL),
                    ot, times, 
                    nativeLevelType, levels, 
                    ApiParam::convert_to_native(params,temp_data),
                    (proj!="") ? Projection(proj.c_str()) : temp_info->getProjection() );

#ifdef USE_NEWBASE
    if (sqd_any) {
        if (!sqd_gs) {
            luaL_error( L, "'sqd_gridsize' missing" );
        }    

        info.setExtra_sqd_producer( sqd_producer )
            .setExtra_sqd_gridsize( sqd_gs )
            .setExtra_sqd_combo19( sqd_combo19 )
            .setExtra_sqd_combo326( sqd_combo326 );
    }
# if 1  // MQD SUPPORT NOT READY YET
    else {
        luaL_error( L, "MQD support not ready yet (missing 'sqd_producer=nnn')" );
    }
# endif
#endif

    // All data gathered - create the Raw.
    //
    Raw *r;

    try {
        r= new(L) Raw( info );     // pushes 'Raw' onto Lua stack
    }
    catch( const E_USAGE &e ) {     // i.e. bad producer name
        return L_nilerr( e.what() );
    }

    // Note: This call can take incredebly long (>1min); makes copy of the data.
    //
    if (r_template) {
        r->fill_from( L, *r_template, 0 /*no callback*/ );
    }
    return 1;
}


/*
* Push the matching data from those behind a file mask, or 'nil' + error string if none.
*
* Pushes:
*   raw object      if succesful
*   nil + err_str   if failing
*
* 'default_gs' defines the gridsize for grid creation, if it is not explicitly said.
* Giving 'MatrixPos::DX' will default to using native grid size (ignoring the 'gridsize' global).
*
* 'height_true':    prefer matches with hybrid data (better vertical resolution) but
*                   use pressure data if no hybrid data available
*
* Returns the number of pushed values.
*/
int RawBind::new_Raw( lua_State *L, const TrackedDataSet &files, 
                                    const JDay &ot, 
                                    const vector<JDay> &required_times, 
                                    const vector<NA_Level> &required_levels,
                                    const vector<ApiParam> &required_params,
                                    bool height_true,
                                    const MatrixPos &default_gs ) throw(E_USAGE) {

    // Check the level requirement for sanity etc.
    //
    enum level_mode {
        UNDEFINED= 0,       // default: restrict like ONLY_GROUND
        ONLY_GROUND,        // ground level only
        HYBRID,             // hybrid level explicitly requested (may also have pressure level requests)
        ONLY_PRESSURE,      // Pressure levels only (or flight levels; they mean the same). Try to find exact,
                            // if not interpolate from hybrid data if supports pressure data, if not interpolate 
                            // from pressure data.
        HEIGHT              // Prefer matches with hybrid data (better vertical resolution) if
                            // the hybrid data ca be used for height calculations. Otherwise,
                            // use pressure data.
    } mode= height_true ? HEIGHT : UNDEFINED;

    for( vector<NA_Level>::const_iterator it= required_levels.begin();
        it != required_levels.end();
        ++it ) {

        if ((it->isGroundLevel() && (mode!=UNDEFINED)) || (mode==ONLY_GROUND)) {
            throw E_LOG_USAGE0( "Cannot require ground level and some other level" );
        }

        switch( it->getType() ) {
            case NA_Level::GROUND_LEVEL:
                mode= ONLY_GROUND;
                break;
                
            case NA_Level::HYBRID_LEVEL:
                // Having one requirement a hybrid level forces all other levels to be calculated
                // from it.
                //
                mode= HYBRID;
                break;

            case NA_Level::PRESSURE_LEVEL:
                // HYBRID, HEIGHT remain; we can calculate pressures from such data
                //
                if (mode==UNDEFINED) {
                    mode= ONLY_PRESSURE;
                }
                break;

            default:
                throw E_LOG_BUG0( "Unexpected level type" );    // shouldn't happen
        }
    }

    if (mode==UNDEFINED) {
        mode= ONLY_GROUND;
    }

    NA_Level default_level;   // DEFAULT_LEVEL type
    if (required_levels.size() == 1) {
        default_level= required_levels[0];   // used if 'grid()' given without level specifier
    }

    // Do the actual search for the right SQD file
    //
    // Each mask is matched *in the order* they are presented in the configuration
    // file.
    //
    // Concatenate the reasons why a match was not made into 'err'; one line per non-matched entry.
    //
    string err_= "";

    // These pointers are shadows to the ones maintained by 'files'. No need to delete these.
    //
    TrackedData *d;
    TrackedData *d2= 0;     // 2nd choice for ONLY_PRESSURE: first hybrid data match
    TrackedData *d3= 0;     // 3rd choice for ONLY_PRESSURE: first pressure data match

    unsigned i=0;       // current config entry (0..n)

    while(true) {
        try {
            d= files.getData( i, ot );     // increments 'i' automatically
        }
        catch(exception &e) {
            const char *what= e.what();
            throw E_LOG_ERROR( "%s", what ? what : "Unknown C++ exception" );
        }
        if (!d) break;

LOG_DEBUG( "Considering raw: %s", d->getSource().c_str() );

        NA_Info info= d->getInfo();
        bool match=true;

        //---
        // Does it cover the required times?
        //
        const vector<JDay> &times= info.getTimes();

        for( vector<JDay>::const_iterator it= required_times.begin();
            it != required_times.end();
            ++it ) {
            if (!it->covered_by( times )) {
                string tmp= it->toString();
                err_ += string_fmt( "\t%s (no validtime %s)\n", info.getExtra_fn().c_str(), tmp.c_str() );
                match=false;
                break;  // out of the iterator
            }
        }
        if (!match) continue;   // next mask

        //---
        // Does it have the required params?
        //
        //const vector<NA_Param> params= info.getParams();

        if (required_params.size()>0) {
            string missing_param;
            
            NA_Data *qd= d->Acquire();
            {
                const vector<NA_Param> &qd_params= qd->getParams();

                for( vector<ApiParam>::const_iterator it= required_params.begin();
                    it != required_params.end();
                    ++it ) {
                    // We use a static function instead of 'it->covered_by()' since it's only this file
                    // that ever needs this feature (for native params).
                    //
                    if (!it->covered_by( qd_params )) {
                        missing_param= it->toString();
                        break;
                    }
                }
            }
            d->Release();
            if (missing_param!="") {
                err_ += string_fmt( "\t%s (no param '%s')\n", info.getExtra_fn().c_str(), missing_param.c_str() );
                continue;   // next mask
            }
        }

        //---
        // Does it cover the required levels?
        //
        const vector<NA_Level> &levels= info.getLevels();
        
        if (required_levels.size()>0) {
            const bool is_hybrid_data= levels.front().isHybridLevel();
            bool all_match_exactly= true;
            string missing_level;
    
            NA_Data *qd= d->Acquire();
            {
                for( vector<NA_Level>::const_iterator it= required_levels.begin();
                    it != required_levels.end();
                    ++it ) {

                    bool exact;
                    if (!it->covered_by( *qd, exact )) {     // outside
                        match=false;
                        missing_level= it->toString();
LOG_DEBUG( "\tlevel not in the data: %s", it->toString().c_str() );
                        break;  // out of the iterator
                        
                    } else if (!exact) {
                        all_match_exactly= false;   // at least one level would need to be calculated
                    }
                }
            }
            d->Release();
            if (missing_level!="") {
                err_ += string_fmt( "\t%s (no level '%s')", info.getExtra_fn().c_str(), missing_level.c_str() );
                continue;   // next mask
            }

            // Any match with GROUND_LEVEL or HYBRID_LEVEL is good; use it.
            
            if ((mode==ONLY_PRESSURE) && (!all_match_exactly)) {
                // Keep this one as a secondary (or third) alternative
                //
                if (is_hybrid_data) {
                    if (!d2) d2= d;   // first hybrid data we found
                } else {
                    if (!d3) d3= d;   // first pressure data we found
                }
LOG_DEBUG0( "\tstill looking for perfect pressure data" );
                continue;   // next mask
            }
        }

        // Note: Data behind 'd' remains active for the lifespan of the pushed 'Raw' because
        //      it is using the 'Aquire()'/'Release()' interface. Only the GC of the Raw object
        //      will release 'd'.
        //
        assert(d);
        new(L) Raw( d, default_level, default_gs );    // use it
        return 1;

    } // while(true)

    assert(!d);

    // Did we have a 2nd or 3rd best choice?
    //
    if (d2 || d3) {
        new(L) Raw( d2 ? d2:d3, default_level, default_gs );
        return 1;
    }

    // Give an error describing why there was no match
    //
    if (err_=="") {
        err_= "No matching data (filename or origintime)";
    } else {
        // Remove the last newline
        //
        err_= string("No matching data (tried these):\n") + err_.substr(0,err_.length()-1);
    }

    return L_nilerr(err_.c_str());
}


/*
* Constructor used by 'RawBind::new_Raw()'
*/
static NA_Data *new_SQD_or_MQD_Data( const NA_Info &info ) throw() {

#ifdef USE_NEWBASE
    if (info.getExtra_sqd_producer() >= 0) {
        return new SQD_Data( info );
    }
#endif

#ifdef MQD_ENABLED
    return new MQD_Data( info );
#else
    throw E_LOG_BUG0( "MQD not enabled (and 'sqd_producer' not given)" );       // should have been handled by upper layers
#endif
}

Raw::Raw( const NA_Info &info ) throw()        // MQD format
  : td(0)  // not a tracked object
  , qd( new_SQD_or_MQD_Data(info) )     // uninitialized
  , def_level()                         // NO_LEVEL
  , def_gridsize( MatrixPos::ZERO )     // no default
{
    assert(qd);
    assert( !qd->isReadOnly() );
}


/*
* Lua stack is used as a data storage only; we leave it in the same state as it was.
*/
bool /*not cancelled*/ Raw::fill_from( lua_State *L, const Raw_interface &r_from, NA_Data::ProgressCallback *cb ) {

    const vector<JDay> &times= qd->getTimes();
    const vector<NA_Level> &levels= qd->getLevels();
    const vector<NA_Param> &params= qd->getParams();    // lists the visible scalar params (not vector, not hosts)

    const Projection &pr= qd->getProjection();

    unsigned progress_now= 0;
    unsigned progress_total= times.size() * levels.size() * params.size();

    // 12-Sep-2011 PKi: For handling host/sub parameters
    //
	NA_Param p_326(kFmiWeatherAndCloudiness);
	NA_Param p_19(kFmiTotalWindMS);
	NA_Param p;
    bool has_326 = qd->getExtra_sqd_combo326(),load_326;
    bool has_19 = qd->getExtra_sqd_combo19(),load_19;

    // Initialize either from the template or to NANs.
    //
    bool cancel= false;

    for( vector<JDay>::const_iterator it_times= times.begin();
        it_times != times.end();
        ++it_times ) {
        for( vector<NA_Level>::const_iterator it_levels= levels.begin();
            it_levels != levels.end();
            ++it_levels ) {

            // Note: 'r_from' need not provide all (or any) of the params (or levels, or times) 
            //       for the newborn data.
            //
            /*const*/ Grid *g= 0;
            try {
                g= ::new Grid( r_from, *it_times, *it_levels, pr );    // native grid size (may be separate for each parameter)
            }
            catch( E_NO_MATCH ) { }   // eat it up; no such time/level

            // Go through all the params and copy the values that exist in template.
            // Set others to empty.
            //

            // 30-Sep-2011 PKi: Bugfix, must be (re)initialized here
            load_326 = load_19 = true;

            for( vector<NA_Param>::const_iterator it_params= params.begin();
                it_params != params.end();
                ++it_params ) {

            	// 12-Sep-2011 PKi: Subparameter ? Copy host parameter if not already copied (jira-173)
            	if (has_326 && derived_from(326,*it_params))
            		if (load_326)
            		{
            			p = p_326;
            			load_326 = false;
            		}
            		else
            			continue;
            	else if (has_19 && derived_from(19,*it_params))
					if (load_19)
					{
						p = p_19;
						load_19 = false;
					}
					else
						continue;
            	else
            		// Standalone parameter
            		p = *it_params;

                // Note: If we don't have initial values (which also means the size for the particular
                //       param tile), we'll simply leave the data uninitialized. MQD will make 
                //       an empty tile of this.
                //
                if (g) {
                    const Matrix *m= g->push_ScalarMatrix( L, p );
                    if (m) {
                        Matrix *m_to= qd->push_NativeMatrix( L, *it_times, *it_levels, p );
                        assert(m_to);

                        // Note: Also handles the case where projections differ.
                        //
                        m_to->fit_from_( *m );   // causes the changes to go to underlying data

                        lua_pop(L,2);   // releases 'm_to' and 'm'
                    }
                }
                
                // Progress callback
                //
                if (cb) {
                    ++progress_now;
                    const string a= it_params->toString(false /*prefer native names*/);
                    const string b= it_levels->toString();
                    const string c= it_times->toString();

                    cancel= cb->progress( progress_now, progress_total, a.c_str(), b.c_str(), c.c_str() );
                    if (cancel) break;
                }
            }
            delete g;
            
            if (cancel) goto OUT;
        }
    }

OUT:
    return !cancel;
}


/*
* void= write( fn_str [,progress_func] )
*
* cancel_bool= progress_func( now_uint, total_uint, vt_str, level_str, param_str )
*
* Writes out the Raw object to either '.sqd' or '.mqd' file.
*
* This procedure may take a while; the caller can provide a progress/cancellation
* function.
*
* Upvalues:
*   1: 'Raw' object
*/
int RawBind::write( lua_State *L ) {
    const Raw_interface &me= * Raw_interface::instance( L, lua_upvalueindex(1) );
    const char *fn= lua_tostring(L,1);

    NA_Data::ProgressCallback cb_( L, lua_isfunction(L,2) ? 2:0 );
    NA_Data::ProgressCallback *cb= lua_isfunction(L,2) ? &cb_ : 0;

    if (!fn) {
        luaL_error( L, "Bad filename: %s", L_typename(1) );
    }

    ofstream os( fn, ios_base::binary | ios_base::out );
    if (os.fail()) {
        luaL_error( L, "Unable to write to: %s", fn );
    }

    me.getData()->output( os, cb );    // MIME type not needed when writing to disk

    return 0;
}

