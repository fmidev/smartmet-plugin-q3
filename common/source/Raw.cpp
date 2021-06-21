/*
* RAW.CPP                       Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  15-Oct-10
*/
#include "Raw.h"
#include "Tools.h"

#include "Proto.h"

#include "Matrix.h"
#include "Grid.h"
#include "VectorMatrix.h"

#ifdef USE_NEWBASE
# include "SQD_Data.h"
#endif

#ifdef MQD_ENABLED
# include "MQD_Data.h"
#endif

#if (!defined(USE_NEWBASE)) && (!defined(MQD_ENABLED))
# error "Not using Newbase, neither MQD."
#endif

#include <cstring>
#include <stdexcept>

#include "newbase/NFmiEnumConverter.h"

using namespace std;

LuaNew_ID RawBind::ID;


/*---=== Helpers ===---
*/

/*
* Filter what to place in 'def_level' member. 
*/
static NA_Level process_def_level( const NA_Level &lev ) {

    /*
    * If the 'Raw' was created with 'hybrid=true', 'hpa=true' or 'height=true' those cannot
    * be used as default levels when making a grid. All other level descriptions can.
    *
    * 23-Nov-2012 PKi: Height value query support. If support for height level query data is added later,
    * 				   height level can be used as the default
    */
    NA_Level::Type lt= lev.getType();
    
    if (((lt!=NA_Level::GROUND_LEVEL) && (lt!=NA_Level::NO_LEVEL) && (!lev.hasValue())) || (lt==NA_Level::HEIGHT_LEVEL)) {
        return NA_Level();  // NO_LEVEL (use the first level)
    } else {
        return lev;
    }
}


/*---=== Raw ===---
*/

/*
* Note: 'td' is _already_ acquired by its inception. We make another acquire to get the 'qd' pointer.
*       This means the GC or 'Raw' must do _two_ releases to really let the data go.
*/
Raw::Raw( TrackedData *td_, const NA_Level &def_level_, const MatrixPos &def_gridsize_, bool metaQuery ) throw()
    : td(td_)
    , qd(td->Acquire(metaQuery))
    , def_level( process_def_level(def_level_) )
    , def_gridsize( def_gridsize_ )
{
    assert(qd);

    INVARIANT();
}

/*
*/
Raw::~Raw() {
    INVARIANT();

    if (td) {
        td->Release();   // for 'Acquire()' done at 'Raw::Raw()' (to get 'qd')
#ifndef METQU
        td->Release();   // for 'Acquire()' done at the inception (within locks) of Q3
#endif
        // Note: 'qd' needs no release from us (it was managed by 'td')
    } else {
        delete qd;        // in-memory object; allocated by us
    }
}

/*
* Get projection object for global 'projection' or for data native projection
*/
Projection getProjection(lua_State *L,const NA_Data *qd = nullptr)
{
	if (L) {
		lua_getglobal( L, "projection" );
		string_or_null s= lua_tostring(L,-1);
		lua_pop(L,1);

		if (s.c_str()) {
			try {
				return Projection(s.c_str());
			}
			catch( const E_USAGE & ) {
				luaL_error( L, "Bad value in 'projection' global: '%s'", s.c_str() );
			}
		}
	}

	return qd ? qd->getProjection() : Projection::NONE;
}


/*
* ...= __index( raw_ud, key_any )
*/
int RawBind::__index( lua_State *L ) {
    const Raw_interface &r= *Raw_interface::instance(L,1);
    const NA_Data *qd= r.getData();
    assert(qd);
    
    const char *s= lua_tostring(L,2);
    if (!s) {
        luaL_error( L, "Bad index (for raw): %s", L_typename(2) );
    }

    // [time_str]= .mt_loadtime
    //
    if (strcmp(s,"mt_loadtime")==0) {
        new(L) JDay( qd->getLoadTime() );
        return 1;
    }

    // [time_str]= .mt_modificationtime
    //
    if (strcmp(s,"mt_modificationtime")==0) {
        new(L) JDay( qd->getModificationTime() );
        return 1;
    }

    // [filename_str]= .source
    //
    // Gives "" for scratch-made 'Raw's that don't have a filename associated.
    //
    if (strcmp(s,"source")==0) {
        string_or_null s_= r.getSource();
        if (s_.c_str()) {
            lua_pushstring( L, s_.c_str() );
            return 1;
        } else {
            return 0;
        }
    }
    
    // [uint]= .sqd_producer          (available only for SQD files)
    //
#ifdef USE_NEWBASE
    if (strcmp(s,"sqd_producer")==0) {
        uint prod= qd->getExtra_sqd_producer();
        if (prod>0) {
            lua_pushinteger( L, prod );
            return 1;
        } else {
            return 0;   // not SQD, does not have a producer code
        }
    }
#endif

    // jday_ud= .origintime
    //
    if (strcmp(s,"origintime")==0) {
        new(L) JDay( qd->getOriginTime() );
        return 1;
    }

    // str= .projection
    //
    if (strcmp(s,"projection")==0) {
        const Projection &pr= qd->getProjection();
        lua_pushstring( L, (pr.creationPrefix() + pr.toString()).c_str() );
        return 1;
    }

    // str= .mt_wkt
    //
    if (strcmp(s,"mt_wkt")==0) {
        lua_pushstring( L, qd->getWKT().c_str() );
        return 1;
    }

    // [matrixpos_ud]= .sqd_gridsize
    //
#ifdef USE_NEWBASE
    if (strcmp(s,"sqd_gridsize")==0) {
        MatrixPos gs= qd->getExtra_sqd_gridsize();
        if (gs) {
            new(L) MatrixPos(gs);
            return 1;
        }
        return 0;   // no global gridsize for MQD
    }
#endif

    // { number }= .mt_dx
    //
    if (strcmp(s,"mt_dx")==0) {
        lua_pushnumber(L,qd->getDx());
        return 1;
    }

    // { number }= .mt_dy
    //
    if (strcmp(s,"mt_dy")==0) {
        lua_pushnumber(L,qd->getDy());
        return 1;
    }

    // { str [, ...] }= .levels
    //
    // Returns the available levels in non-descending order
    //
    if (strcmp(s,"levels")==0) {
        lua_newtable(L);

        const vector<NA_Level> &vec= qd->getLevels();
        unsigned i=1;

        for( vector<NA_Level>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring( L, it->toString().c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str }= .mt_leveltype
    //
    // Returns level type's long name and native level type (number), e.g. "Ground:5000"
    //
    if (strcmp(s,"mt_leveltype")==0) {
        string level= qd->getLevels().front().toString(true);
        lua_pushstring( L, (level.substr(0,level.find(":")) + ":" + qd->getNativeLevelType()).c_str() );
        return 1;
    }

    // { str [, ...] }= .params
    //
    // Lists the parameters, prefering to give their standard names (i.e. "T" for "Lämpötila:4").
    //
    if (strcmp(s,"params")==0) {
        lua_newtable(L);

        vector<string> vec= ApiParam::convert_to_api( qd->getParams(), true );
        unsigned i=1;

        for( vector<string>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L,it->c_str());
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str [, ...] }= .native_params
    //
    // Lists the parameters, prefering to give their native names (i.e. "Lämpötila:4").
    //
    // Note: Since with 'USE_NEWBASE' define all 'NA_Param' parameters always carry
    //       native name (and ':NNN' id tail) all strings produced by this will deliver
    //       that.
    //
    // Vector parameters derived from the scalar params are not listed. Only those that
    // are native to the file itself.
    //
    if (strcmp(s,"native_params")==0) {
        lua_newtable(L);

        // 25-Oct-2011 PKi: Ignore virtual parameters
        // const vector<NA_Param> &vec= qd->getParams();
        const vector<NA_Param> vec= ApiParam::realParams(qd);
        unsigned i=1;

        for( vector<NA_Param>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L, it->toString(false /*native names*/).c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str [, ...] }= .mt_paramidents
    //
    // Lists the native parameters using the enumerated names (i.e. "Temperature:4").
    if (strcmp(s,"mt_paramidents")==0) {
        lua_newtable(L);

        NFmiEnumConverter converter;
        const vector<NA_Param> vec= ApiParam::realParams(qd);
        unsigned i=1;

        for( vector<NA_Param>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            const char *cp = strrchr(it->getNativeName_().c_str(),':');
            int id = (cp ? atoi(cp + 1) : 0);
            string name = converter.ToString(id);
            if (name.empty())
            	name = it->getNativeName_();

            lua_pushstring(L, name.c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str [, ...] }= .mt_paramdescriptions
    //
    if (strcmp(s,"mt_paramdescriptions")==0) {
        lua_newtable(L);

        const vector<NA_Param> vec= ApiParam::realParams(qd);
        unsigned i=1;

        for( vector<NA_Param>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L, it->getNativeName_().c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str [, ...] }= .mt_interpolations
    //
    if (strcmp(s,"mt_interpolations")==0) {
        lua_newtable(L);

        const vector<NA_Param> vec= ApiParam::realParams(qd);
        unsigned i=1;

        for( vector<NA_Param>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L, it->getInterpolationName().c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str [, ...] }= .mt_precisions
    //
    if (strcmp(s,"mt_precisions")==0) {
        lua_newtable(L);

        const vector<NA_Param> vec= ApiParam::realParams(qd);
        unsigned i=1;

        for( vector<NA_Param>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L, it->getPrecision().c_str() );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { str }= .mt_relative_uv
    //
    if (strcmp(s,"mt_relative_uv")==0) {
        lua_pushboolean(L, qd->getExtra_sqd_RelativeUV() );
        return 1;
    }

    // { jday_ud [, ...] }= .times
    //
    if (strcmp(s,"times")==0) {
        lua_newtable(L);

        const vector<JDay> &vec= qd->getTimes();
        unsigned i=1;

        for( vector<JDay>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            new(L) JDay( *it );
            lua_settable(L,-3);
        }
        return 1;
    }

    // { uint }= .mt_timestep
    //
    if (strcmp(s,"mt_timestep")==0) {
        lua_pushinteger(L,qd->getTimeStep());
        return 1;
    }

    // 29-Sep-2011 PKi
    // { latlon_ud [, ...] }= .locations
    //
    if (strcmp(s,"locations")==0) {
        lua_newtable(L);

        vector<LatLon> vec;
    	Projection proj = getProjection(L);

        qd->getLocations(vec,(proj != Projection::NONE) ? &proj : nullptr);

        unsigned i=1;

        for( vector<LatLon>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            new(L) LatLon( *it );
            lua_settable(L,-3);
        }
        return 1;
    }

    // 07-Apr-2015 PKi
    // { number[, ...] }= .dataids; data point id's (e.g. observation station id's)
    //
    if (strcmp(s,"dataids")==0) {
        lua_newtable(L);

        vector<unsigned long> vec;
    	Projection proj = getProjection(L);

    	try {
    		qd->getDataIds(vec,(proj != Projection::NONE) ? &proj : nullptr);
    	}
	catch (const std::exception & e) {
            luaL_error( L, e.what() );
    	}

        unsigned i=1;

        for( vector<unsigned long>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushnumber(L,*it);
            lua_settable(L,-3);
        }
        return 1;
    }

    // 07-Apr-2015 PKi
    // { string[, ...] }= .datanames; data point (e.g. observation station) names
    //
    if (strcmp(s,"datanames")==0) {
        lua_newtable(L);

        vector<string> vec;
    	Projection proj = getProjection(L);

    	try {
            qd->getDataNames(vec,(proj != Projection::NONE) ? &proj : nullptr);
    	}
	catch (const std::exception & e) {
            luaL_error( L, e.what() );
    	}

        unsigned i=1;

        for( vector<string>::const_iterator it= vec.begin();
            it != vec.end();
            ++it ) {
            lua_pushinteger(L,i++);
            lua_pushstring(L,it->c_str());
            lua_settable(L,-3);
        }
        return 1;
    }

    // func= .write
    // void= func( filename_str )
    //
#ifdef METQU
    if (strcmp(s,"write")==0) {
        lua_pushvalue( L, 1 );  // 2nd ref to the 'Raw'
        lua_pushcclosure( L, write, 1 /*upvalues*/ );
        return 1;
    }
#endif

    // func= .has_param
    // bool= func( param_str )
    //
    // Used for checking whether read of 'param' will cause an error or not.
    //
    // Note: Checking '.params' cannot safely be used for this, since a parameter can be
    //      accessed using multiple names (i.e. "Z-korkeus:2" as "Z", "Z-korkeus", "Z-korkeus:2"
    //      and ":2). This function checks all the alternatives (just as indexing a grid would).
    //
#if 0   // NOT NEEDED (now '.param' lists the standard param names)
    if (strcmp(s,"has_param")==0) {
        lua_pushvalue( L, 1 );  // 2nd ref to the 'Raw'
        lua_pushcclosure( L, has_param, 1 /*upvalues*/ );
        return 1;
    }
#endif

    // matrix|matrix_2d= .<param>
    //
    // i.e. 'HIR{ hpa=350 }.T'
    //
    L_GROW(2);
    
    /*
    * 'HIR{ hpa=350 }.T'
    *   -->
    * 'HIR{ hpa=350 }().T'    // Uses 'hpa=350' (or whichever single level was given) as default
    *                         // for creating the grid object.
    */
    int sPos = lua_gettop(L);					// 26-Oct-2011 PKi: To check # of retvals pushed by __call()
    lua_pushcfunction( L, RawBind::__call );
    lua_pushvalue( L, 1 );  // 2nd ref to us
    lua_call( L, 1 /*args*/, 2 /*retvals*/ );   // NOT expected to give an error
                                                // 26-Oct-2011 PKi: (1 -->) 2 retvals to get E_NO_MATCH message

    // 26-Oct-2011 PKi: If __call() failed (no grid at stack pos -2), get the error message instead of
    //                  just returning "Assert failed at ..."

    if ((sPos -= lua_gettop(L)) != -2)
    	sPos = -1;								// Weird, try top of stack

    if (! Grid::instance( L, sPos ))
    {
        const char *p = ((sPos == -2) ? lua_tostring(L, -1) : nullptr);
        luaL_error( L, p ? p : "RawBind::__call() failed");
    }
    else if (sPos == -2)						// Grid to top of stack
        lua_pop(L, 1);
    
    // [-1]: grid
    
    L_ASSERT( Grid::instance( L, -1 ) );
    
    // Note: Use the 'GridBind::Index()' function (via Lua) to get the benefits of caching
    //      grid matrices.
    //
    lua_pushstring( L,s );
    lua_gettable( L, -2 );      // leads to 'GridBind::index()'
    
    L_ASSERT( (Matrix::instance(L,-1)!=nullptr) || (VectorMatrix::instance(L,-1)!=nullptr) );
    
    // Note: Lua will clean out the reference to grid, but keep it alive via the matrix
    //      returned.
    //
    return 1;   // The matrix pushed there by 'GridBind::index()'
}

/*
* Loads data point (e.g. observation station) id's from lua stack.
*
*   'number | { number [, ...] }'
*
* Returns true on success
*/
bool get_dataids_from_ud(DataIdList & dataIds,struct lua_State * L,int index) {
    index= L_ABS(index);

    if (lua_isnumber(L,index)) {
    	dataIds.push_back((long) lua_tonumber(L,index));
    } else if (lua_istable(L,index)) {
        L_GROW(1);

        // 'index' has a table
        //
        for( unsigned n=lua_objlen(L,index),i=1; i<=n; i++ ) {
            lua_pushinteger( L,i );
            lua_gettable( L,index );

            if (!lua_isnumber(L,-1)) {
                lua_pop(L,1);
                return false;    // table of some other sort
            }

            dataIds.push_back((long) lua_tonumber(L,-1));

            lua_pop(L,1);
        }
    } else {
        return false;    // not number, not table
    }

    return (dataIds.size() > 0);
}


/*
* [grid_ud] [,err_str]= __call( raw_ud, [{ [time=jday_ud|time_str|true],
*                                       [projection=str|true],
*                                       [gridsize=pos_ud|true],
*                                       [ground=true]
*                                           |[hybrid=uint]
*                                           |[hpa|hPa=number]
*                                           |[flight=uint], (*)
*                                 }] )
*
* 'time':       'true' means ignore 'validtime' global (use first/last available time)
* 'projection': 'true' means use native projection (ignore global 'projection')
* 'gridsize':   'true' means use native gridsize (ignore global 'gridsize')
*
* 'ground', 'hpa', 'hybrid', 'height', 'flight':
*               Only one of these is allowed to be given. Defines the level for the
*               grid data returned. 'flight' is converted to pressure level.
*
* Defaults:
*       time:       from globals ("validtime")
*                       if still none, defaults to the FIRST TIME for model data
*                       and LAST TIME for observations
*       projection: from globals
*       gridsize:   default gridsize of 'raw' (if set when creating it) or from 'gridsize' global
*       level:      default level of 'raw' (if set when creating it) or simply first
*                   level in the data
*
* Upvalues: the 'Raw' data this operates on.
*/
int RawBind::__call( lua_State *L ) {
    proto( L, "Raw, [{ time=[jday|time_str|true|{jday|time_str}],"		// 22-Sep-2011 PKi: Time is table when called for cross
                      "projection=[string|true],"
                      "gridsize=[MatrixPos|true],"
                      "ground=[true],"
                      "hpa=[number|{number}],hPa=[number|{number}],"	// 22-Sep-2011 PKi: Level is table when called for cross
                      "hybrid=[uint|{uint}],"							//
#ifdef CONFIG_FLIGHT_LEVELS_API											//
                      "flight=[uint|{uint}],"							//
#endif																	//
                      "location=[latlon|{latlon}],"						// 22-Sep-2011 PKi: Locations when called for cross
                      "dataid=[number|{number}],"						// 07-Apr-2015 PKi: Data point (e.g. observation station) id's when called for point data
                      "cross=[true|false],"								// 22-Sep-2011 PKi: Flag for cross call
                      "flightroute=[true|false],"						// 05-Mar-2015 PKi: Flag for flightroute call
    				  "height=[number|{number}],"						// 04-Oct-2011 PKi: For cross call using heigths
    																	// 23-Nov-2012 PKi: and for height value query
                    "}]" );

    Raw_interface &r= *Raw_interface::instance( L, 1 );
    const NA_Data *qd= r.getData();
    assert(qd);

    JDay vt;
    bool vt_force_default= false;

    Projection proj= Projection::NONE;
    MatrixPos gridsize;
    bool native_gs= false;

    NA_Level level;    // NA_Level::NO_LEVEL

    // 22-Sep-2011 PKi: For cross call
    bool cross = false,flightRoute = false;
    std::vector<JDay> vtVec;
    std::vector<NA_Level> levelVec;
    LatLonList locs;
    DataIdList dataIds;

    // 13-Oct-2011 PKi: To check level type
    NA_Level::Type lt = NA_Level::NO_LEVEL;

    if (lua_gettop(L)>1) {

        const int table_idx= 2;
        L_GROW(2);
    
        // Get values from the param table
        //
        lua_pushnil(L);     // first key
        while( lua_next(L,table_idx)) {
            // [-1]: value
            // [-2]: key
    
            const char *key= lua_tostring(L,-2);
            if (!key) {
                luaL_error( L, "Unknown key: %s", L_typename(-2) );
            }

            if (strcmp(key,"time")==0) {
                if (lua_isboolean(L,-1) && lua_toboolean(L,-1)) {   // true
                    vt_force_default= true;   // force to use first/last available time (ignore 'validtime' global)
                } else
                // 22-Sep-2011 PKi: Time can be table when called for cross
                if (lua_istable(L,-1))  {
                	vtVec = vector_of_times(L,-1);
                }
                else {
                    vt= JDay(L,-1);
                    if (!vt) {
                        luaL_error( L, "Bad time: %s", L_string_or_typename(-1) );
                    }
                }

            } else if (strcmp(key,"projection")==0) {
                const char *s= lua_tostring(L,-1);
                if (s) {
                    try {
                        proj= Projection(s);    // throws 'E_USAGE' if bad projection string
                    }
                    catch( const E_USAGE & ) {
                        luaL_error( L, "Bad projection: %s", s );
                    }
                } else if (lua_isboolean(L,-1) && lua_toboolean(L,-1)) {    // true
                    proj= qd->getProjection();
                } else {
                    luaL_error( L, "Bad projection: %s", L_typename(-1) );
                }
            
            } else if (strcmp(key,"gridsize")==0) {
                MatrixPos *ud= MatrixPos::instance(L,-1);
                if (ud) {
                    gridsize= *ud;  // makes a copy
                } else if (lua_isboolean(L,-1) && lua_toboolean(L,-1)) {    // true
                    native_gs= true;
                } else {
                    luaL_error( L, "Bad gridsize: %s", L_typename(-1) );
                }

            } else if (
#ifdef CONFIG_FLIGHT_LEVELS_API
                    (strcmp(key,"flight")==0) || 
#endif
                    // 13-Oct-2011 PKi: Remember level type to check it later
                    (lt = NA_Level::lt_enum(key))) {
                if (level) {
                    luaL_error( L, "Must give only one of: 'ground', 'hybrid', 'hpa', 'height'"
#ifdef CONFIG_FLIGHT_LEVELS_API
                                    ", 'flight'" 
#endif
                    );
                }

                try {
                    // 22-Sep-2011 PKi: Level can be table when called for cross
                    if (lua_istable(L,-1))  {
                    	one_or_many_levels( L, -1, key, levelVec );
                    	if (levelVec.size() > 0)
                        	level= levelVec[0];
                    }
                    else
                    	level= one_level( L, -1, key );
                } 
                catch( const E_USAGE &e ) {
                    luaL_error( L, "%s", e.what() );
                }

            // 22-Sep-2011 PKi: New options for cross; 'cross=true' and 'location={latlon}'
            // 05-Mar-2015 PKi: New option 'flightroute=true' to return one value per location/level/time
            } else if ((strcmp(key,"cross")==0) || (strcmp(key,"flightroute")==0)) {
            	if (lua_isboolean(L,-1)) {
            		cross = lua_toboolean(L,-1);
					flightRoute = (cross && (strcmp(key,"cross")!=0));
				}
            	else
            		luaL_error( L, "Bad cross: %s", L_typename(-1) );
            } else if (strcmp(key,"location")==0) {
                LatLonList::e_state st= locs.init_from_ud(L,-1,false);

                if (st== LatLonList::NONE) {
                    luaL_error( L, "Bad location: %s", lua_isstring(L,-1) ? lua_tostring(L,-1) : L_typename(-1) );
                }
            } else if (strcmp(key,"dataid")==0) {
                if (!get_dataids_from_ud(dataIds,L,-1)) {
                    luaL_error( L, "Bad data id: %s", lua_isstring(L,-1) ? lua_tostring(L,-1) : L_typename(-1) );
                }
            } else {
                luaL_error( L, "Unknown option: %s", key );
            }
            lua_pop(L, 1);   // remove value
        }
    }

    // Use globals 'validtime', 'projection' and 'gridsize' if the caller didn't give them.
    //
    // 22-Sep-2011 PKi: If called for cross, set vt (level already set) from given vector to avoid
    //                  unnecessary default value handling

	cross |= flightRoute;

    if (cross && (vtVec.size() > 0))
        vt = vtVec[0];

    if (!vt) {
        lua_getglobal( L, "validtime" );
        if ((!vt_force_default) && (!lua_isnil(L,-1))) {
            vt= JDay( L, -1 );
            if (!vt) {
                luaL_error( L, "Bad validtime: %s", L_string_or_typename(-1) );
            }
        } else {
            // TBD: Observations could use the LAST time (once we support observations)
        	//
        	// 07-Apr-2015 PKi: Use last time for point data

        	const std::vector<JDay> & t = qd->getTimes();
            vt= t[qd->hasGrid() ? 0 : t.size()-1];     // first or last time
        }
        lua_pop(L,1);
    }
    assert(vt);

    if (!proj)
    	proj = getProjection(L,qd);

    if ((!native_gs) && (gridsize.getX() == 0)) {
        MatrixPos def_gridsize= r.getDefaultGridsize();

        if (def_gridsize) {
            gridsize= def_gridsize;       // certain default given at Raw creation
            
        } else if (def_gridsize == MatrixPos::DX) {      // (1,0) marks 'native as default'
            native_gs= true;

        } else {
            lua_getglobal( L, "gridsize" );
            const MatrixPos *mp= MatrixPos::instance(L,-1);
            if (mp) {
                gridsize= *mp;
            } else {
                native_gs= true;
            }
            lua_pop(L,1);
        }
    }

    // If using default level and 'def_level' has been set (by creating the Raw with just one
    // level id required).
    //
    // 13-Oct-2011 PKi: Check the level type if level was given; height=number|{number} supported
    //                  only for cross call
    // 23-Nov-2012 PKi: and for height value query; no need to check for noncross call
    //

    if (!level) {
        level= r.getDefaultLevel();
    }

    // 22-Sep-2011 PKi: If called for cross, passing time(s), level(s) and location(s) as vectors;
    //				    if not, report error if called with vector(s) (call from user script)

	if ((cross && (locs.size() < 1)) || ((! cross) && (locs.size() > 0)))
		luaL_error( L, "%s option: location", cross ? "Missing" : "Unknown" );
	else if (cross)
    {
    	if (vtVec.size() < 1)
    		vtVec.push_back(vt);
    	if (levelVec.size() < 1)
    		levelVec.push_back(level);
    }
    else
    {
    	const char *op[] = { "time", "level" };
    	const char *p;

    	if (((p = op[0]) && (vtVec.size() > 0)) || ((p = op[1]) && (levelVec.size() > 0)))
    		luaL_error( L, "Expecting scalar %s", p );
    }

    //---
    // NOTE: When there are exceptions within the 'new(L)' allocated constructor,
    //      we need to be taking care or rewinding the allocation, like this:
    //
    Grid *g;
    try {
    	g= new(L) Grid( r, vt, level, proj, native_gs ? MatrixPos::ZERO : gridsize
    		          //
    		  	      // 22-Sep-2011 PKi: For cross call
    		  	      //
                      , cross ? &vtVec : nullptr
                      , cross ? &levelVec : nullptr
                      , cross ? &locs : nullptr
                      , flightRoute
					  , (dataIds.size() > 0) ? &dataIds : nullptr
                      );
    }
    catch( const E_NO_MATCH &e ) {
        // Didn't get created (but Lua did allocate the object AND attach its GC
        // to the C++ destructor. Nuke the object before it gets to GC.
        //
        LuaNew_base::nuke(L,-1);  // removes the link from Lua GC to C++ destructor

        return L_nilerr( e.what_nosource() );
    }

    unsigned rkey= LuaNew_base::keep_alive(L,-1,1);     // keep 'r' alive throughout the grid's lifespan
    g->set_rkey(rkey);

    return 1;
}


/*
* Set up a metatable.
*/
void RawBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,__index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__call");
    lua_pushcfunction(L,__call);
    lua_settable(L,-3);
}


/*
* bool= has_param( param_str )
*
* Upvalues:
*   1: 'Raw' object
*/
#if 0   // NOT NEEDED (now '.params' lists the standard param names)
int RawBind::has_param( lua_State *L ) {
    const Raw_interface &me= * Raw_interface::instance( L, lua_upvalueindex(1) );
    const char *s= lua_tostring(L,1);

    ApiParam p(s);

    const NA_Data *d= me.getData();   // no release, valid as long as 'me' is

    // 'NA_Data *' is also 'NA_Info *'. It provides us with the set of parameters in the file, including
    // *both* the native names and id's and also their standard names.
    // 
    bool have_it= p.covered_by( d->getParams() );

    lua_pushboolean( L, have_it );
    return 1;
}
#endif

