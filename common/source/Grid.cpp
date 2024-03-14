/*
* GRID.CPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Handles one time, level, projection and gridsize (all parameters) data management.
*
* Takes care of vector parameters, projection and scaling of data.
*
* Revised:  15-Oct-2010 AKa
*/
#include "Grid.h"
#include "VectorMatrix.h"
#include "Tools.h"

#include "ApiParam.h"
#include "LatLon.h"

using namespace std;

LuaNew_ID GridBind::ID;

/*-------------------
* Caching performance results:
*
* Running a test case (testbed #25) with 50,50 gridsize
*
*   without caching of 'grid.XXX':  ~15 sec
*   with caching:                   ~1.0 sec
*
* The purpose of the caching is not so much the speedup, but preventing situations where
* explicit optimizations are needed (i.e. local aliasing).
*--------------------*/


/*---=== Helpers ===---
*/


/*---=== GridBind ===---
*/

/*
* Set up metatable.
*/
void GridBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,__index);
    lua_settable(L,-3);

#ifdef METQU
    lua_pushliteral(L,"__newindex");
    lua_pushcfunction(L,__newindex);
    lua_settable(L,-3);
#endif
}


/*
* [m2_ud | m_ud]= __index( grid_ud, param_name_str )
*
* Gives an error if the parameter/level/validtime is not available.
*
* Caches the returned matrices (as weak links, to avoid circular references)
* so that asking 'g.X' more than once will usually return the _same_ object
* as the first query (unless it's been garbage collected in between). 
* This is a VITAL OPTIMIZATION to make scripting seamless (i.e. use of 'g.X' 
* within inner loops will not take the system crawling down).
*
* Note that due to using weak links for the caching, we are not creating
* potentially harmful memory load conditions; matrices no longer in use
* will get collected.
*
* Also attaches the grid object to every returned matrix, so 'm.grid' can
* be used to backtrace to projection and siblings. Note that THIS link
* cannot be weak.
*/
int GridBind::__index( lua_State *L ) throw() {

    const unsigned my_index= 1;
    Grid &my= *Grid::instance(L,my_index);

    const char *s= lua_tostring(L,2);
    if (!s) {
        luaL_error( L, "Bad index (for grid): %s", L_typename(2) );
    }
    else if (strcmp(s,"raw") == 0) {
        unsigned key= my.get_rkey();

        if (LuaNew_base::push_alive( L, 1, key )) {
            L_ASSERT( Raw::instance(L,-1) );
            return 1;
        }

        luaL_error( L, "No raw for grid" );
    }

    L_GROW(2);

    /*
    * Note: There is no '.gridsize' (or '.size') property since in our data model the size
    *       may be different for different parameters. Instead, take a parameter matrix and
    *       use its '.size'.
    */

    // Check if the matrix is (still) cached
    //
    lua_getuservalue ( L, 1 );
    lua_pushvalue( L, 2 );  // 2nd ref of the string
    lua_gettable( L, -2 );
        //
        // [-1]: environment[ param_name ] (Matrix, VectorMatrix); or nil
        // [-2]: environment table

    if (lua_isnil(L,-1)) {
        lua_pop(L,1);
        L_GROW(3);    

        CONST_IF_SERVER ApiMatrix *mb= my.push_Matrix( L, ApiParam(s) );
        if (!mb) {
            luaL_error( L, "Unknown parameter: %s", s );
        }
        // [-1]: 'Matrix' or 'VectorMatrix' return value

        // Remember the matrix via cache (weak value)
        //
        lua_pushvalue( L, 2 );  // 2nd ref of the string (key)
        lua_pushvalue(L,-2);    // 2nd ref of the matrix

        L_ASSERT( lua_istable(L,-4) );  // the environment table
        lua_settable( L, -4 );

        // Regardless the type of pushed matrix (MemMatrix or proxy) the grid needs to be kept
        // alive for the '.grid' feature to work.
        //
        // 'SQD_Matrix' and 'MQD_Matrix' would require such keep-alive simply to remain usable.
        //
        unsigned key= LuaNew_base::keep_alive( L, -1, 1 );
        mb->setGridKey(key);
    }
    
    // [-1]: Matrix|VectorMatrix (either from cache or freshly pushed)
    // [-2]: environment table (will be automatically popped)

    return 1;
}


/*
* void= __newindex( grid_ud, param_name_str, m_ud| m2_ud| num| vector_ud )
*
* Set the contents of a whole (SQD/MQD created-from-scratch) matrix at once.
*
* TBD: Handling of projections and grid size within here must be checked.
*      What happens (should happen) if matrices with different projection and/or grid size
*      are written on each other.
*/
#ifdef METQU
int GridBind::__newindex( lua_State *L ) throw() {

    Grid &my= *Grid::instance(L,1);

    const char *s= lua_tostring(L,2);
    if (!s) {
        luaL_error( L, "Bad index (for grid): %s", L_typename(2) );
    }

    ApiParam p(s);

    ApiMatrix *m= my.push_Matrix( L, p );
    if (!m) {
        luaL_error( L, "No such param: '%s'", s );
    }

    // 'm' is either a matrix of vectors or scalars
    //
    if (m->is_2d()) {
        VectorMatrix *mv= static_cast<VectorMatrix*>(m);

        const VectorMatrix *mv2= VectorMatrix::instance(L,3);
        if (mv2) {
            *mv= *mv2;    // copies by value
        } else {
            const Vector *vector= Vector::instance(L,3);
            if (vector) {
                *mv= *vector;
            } else {
                luaL_error( L, "Cannot set VectorMatrix to: %s", L_typename(3) );
            }
        }

    } else {
        // Scalar parameter
        //
        Matrix *ms= static_cast<Matrix*>(m);

        const Matrix *ma= Matrix::instance(L,3);
        if (ma) {
            ms->copy_from( *ma );         // copies by value
        } else if (lua_isnumber(L,3)) {
            ms->fill_with( lua_tonumber(L,3) );
        } else {
            luaL_error( L, "Cannot set a matrix to: %s", L_typename(3) );
        }
    }

    lua_pop(L,1);   // remove the pushed matrix
    return 0;
}
#endif


/*---=== Grid ===---
*/

/*
* 'gs' == 'MatrixPos::ZERO' marks "native gridsize" (for each param tile, which can have different sizes)
*/
#ifdef METQU
Grid::Grid( Raw_interface &r, const JDay &vt_, const NA_Level &lev_, const Projection &proj_, const MatrixPos &gs_
          //
          // 22-Sep-2011 PKi: For fetching cross using newbase
          //
          , std::vector<JDay> const *vtVec_
          , std::vector<NA_Level> const *levelVec_
          , LatLonList const *locs_
          , bool flightRoute_
          , const DataIdList *dataIds_
		  ) throw(E_NO_MATCH)
    : data( r.getData_rw() ), vt(vt_), level(lev_), proj(proj_), gridsize(gs_), rkey(0) 
    , vtVec(vtVec_ ? *vtVec_ : std::vector<JDay>())
    , levelVec(levelVec_ ? *levelVec_ : std::vector<NA_Level>())
    , locs(locs_ ? *locs_ : LatLonList())
    , flightRoute(flightRoute_)
    , dataIds(dataIds_ ? DataIdList() : DataIdList())
{ 
	// 13-Oct-2011 PKi: Added coverage check for time/level vectors (cross usage)

	unsigned int i,n;

    if ((n = vtVec.size()) < 1)
    {
		if (!vt.covered_by( data->getTimes() )) {
			throw E_NO_MATCH(vt);
		}
    }
//  11-Nov-2011 PKi: Do not check cross times; just return missing values for times not in data
//
//  else
//  {
//      const std::vector<JDay> times = data->getTimes();
//
//    	for (i = 0; (i < n); i++)
//			if (!vtVec[i].covered_by( times )) {
//				throw E_NO_MATCH(vtVec[i]);
//			}
//  }

    if ((n = levelVec.size()) < 1)
    {
		if (!level.covered_by( *data )) {
			throw E_NO_MATCH(level);
		}
    }
    else
    	for (i = 0; (i < n); i++)
    		if (!levelVec[i].covered_by( *data )) {
    			throw E_NO_MATCH(levelVec[i]);
    		}

    INVARIANT();
}
#endif

Grid::Grid( const Raw_interface &r, const JDay &vt_, const NA_Level &lev_, const Projection &proj_, const MatrixPos &gs_
          //
          // 22-Sep-2011 PKi: For fetching cross using newbase
          //
          , std::vector<JDay> const *vtVec_
          , std::vector<NA_Level> const *levelVec_
          , LatLonList const *locs_
          , bool flightRoute_
          , const DataIdList *dataIds_
		  ) throw(E_NO_MATCH)
    : 
#ifdef METQU
    data( const_cast<NA_Data*>( r.getData() ) )
#else
    data( r.getData() )
#endif
    , vt(vt_), level(lev_), proj(proj_), gridsize(gs_), rkey(0) 
    , vtVec(vtVec_ ? *vtVec_ : std::vector<JDay>())
    , levelVec(levelVec_ ? *levelVec_ : std::vector<NA_Level>())
    , locs(locs_ ? *locs_ : LatLonList())
    , flightRoute(flightRoute_)
    , dataIds(dataIds_ ? *dataIds_ : DataIdList())
{ 
    // In METQU mode the data can be read/write. If we get a 'const' 'r', the data better 
    // also be marked read-only.
    //
#ifdef METQU
    assert( data->isReadOnly() );
#endif

	unsigned int i,n;

    if ((n = vtVec.size()) < 1)
    {
		if (!vt.covered_by( data->getTimes() )) {
			throw E_NO_MATCH(vt);
		}
    }
//  11-Nov-2011 PKi: Do not check cross times; just return missing values for times not in data
//
//  else
//  {
//    	const std::vector<JDay> times = data->getTimes();
//
//    	for (i = 0; (i < n); i++)
//			if (!vtVec[i].covered_by( times )) {
//				throw E_NO_MATCH(vtVec[i]);
//			}
//  }

    if ((n = levelVec.size()) < 1)
    {
		if (!level.covered_by( *data )) {
			throw E_NO_MATCH(level);
		}
    }
    else
    	for (i = 0; (i < n); i++)
    		if (!levelVec[i].covered_by( *data )) {
    			throw E_NO_MATCH(levelVec[i]);
    		}

    INVARIANT();
}


/*
*/
#ifdef METQU
void Grid::setMatrix( const NA_Param &p, const Matrix &m ) throw(E_READONLY, E_BAD_SIZE, E_NO_MATCH) {

    Matrix *sm= new_ScalarMatrix(p);
    {
        sm->copy_from(m);   // assumes same grid size
    }
    delete sm;
}
#endif

/*
*/
#ifdef METQU
void Grid::setMatrix( const NA_Param &p, float v ) throw(E_READONLY, E_NO_MATCH) {

    Matrix *sm= new_ScalarMatrix(p);
    {
        sm->fill_with(v);
    }
    delete sm;
}
#endif


/*
* Push a matrix connected to the underlying SQD/MQD data.
*
* If native projection and gridsize are used (as initialized in 'this' object), we connect directly to
* the underlying data (allowing writes in command line mode). Otherwise the returned matrix is a 
* projected, scaled read-only 'MemMatrix'.
*
* 'na' and (if a vector) 'nb' are parameters that have already been proven to be in the data.
*/
CONST_IF_SERVER ApiMatrix *Grid::push_Matrix( lua_State *L, const ApiParam &p ) CONST_IF_SERVER {

    const vector<NA_Param> &params= data->getParams();
    
    NA_Param na, nb;
    bool polar;
    
    if (!p.covered_by( params, na, nb, polar )) {
        return nullptr;    // no match for 'p' in 'params'
    }

    L_GROW(3);
    
    CONST_IF_SERVER Matrix *ma= push_ScalarMatrix( L, na );   // scalar | first component
    assert(ma);

    if (!nb) {
        return ma;
    } else {
        CONST_IF_SERVER Matrix *mb= push_ScalarMatrix( L, nb );
        assert(mb); (void)mb;

        return new(L) VectorMatrix( L, polar );     // eats components
    }
}

/*
* Push a scalar matrix, scaled to 'gridsize' (native gridsize for the parameter if 
* 'gridsize'=='MatrixPos::ZERO').
*/
CONST_IF_SERVER Matrix *Grid::push_ScalarMatrix( lua_State *L, const NA_Param &p ) CONST_IF_SERVER {

	// 22-Sep-2011: Called for cross ? Using native projection and gridsize

	if ((vtVec.size() > 0) && (levelVec.size() > 0) && (locs.size() > 0)) {
        return data->push_NativeCross( L, vtVec, levelVec, locs, p, flightRoute );
// 09-Apr-2015 PKi: Do not call push_NativeMatrix to take projection into account
//
//	} else if (!gridsize) {
//  	return data->push_NativeMatrix( L, vt, level, p, nullptr, nullptr, nullptr, &dataIds);
    } else {
        const Matrix *m= data->push_Matrix( L, vt, level, p, proj, gridsize, &dataIds );    // project and scale
#ifdef METQU
        // We're returning a non-const pointer but essentially it's a read-only object
        // (since it's a projection or scale of the real thing, not 1:1 proxy).
        //
        if (m) {
            assert( m->isReadOnly() );
        }
        return const_cast<Matrix *>(m);
#else
        return m;
#endif
    }
}

/*
* Push a read-only matrix for reading SQD/MQD data.
*/
#ifdef METQU
ApiMatrix *Grid::push_NativeMatrix( lua_State *L, const ApiParam &p ) {

    ApiScalarParam a,b;
    bool polar= false;  // to please the compiler ('is_2d' does set it if 'p' is vector param)

    L_GROW(3);

    p.is_2d(a,b,polar);

    const vector<NA_Param> &params= data->getParams();

    NA_Param na= a.covered_by(params);
    if (na) { 
        Matrix *ma= data->push_NativeMatrix( L, vt, level, na );   // scalar | first component
        assert(ma);

        if (!b) {
            return ma;  // also pushed on 'L'

        } else {
            NA_Param nb= b.covered_by(params);
            if (nb) {
                Matrix *mb= data->push_NativeMatrix( L, vt, level, nb );
                assert(mb); (void)mb;
                    
                return new(L) VectorMatrix( L, polar );  // eat the components
            }

            lua_pop(L,1);   // remove 'ma' from 'L' stack
        } 
    }

    return nullptr;
}
#endif

#ifdef METQU
Matrix *Grid::push_NativeScalarMatrix( lua_State *L, const ApiScalarParam &p ) {

    const vector<NA_Param> &params= data->getParams();

    NA_Param np= p.covered_by(params);

    if (np) {
        return data->push_NativeMatrix( L, vt, level, np );
    } else {
        return nullptr;    // 'p' not among the data
    }
}
#endif


