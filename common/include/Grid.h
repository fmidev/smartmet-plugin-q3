/*
* GRID.H                            Copyright (c) 2009-10, Ilmatieteen laitos
*
* Revised:  15-Oct-2010 AKa
*/
#ifndef GRID_H
#define GRID_H

#include "LuaNew.h"
#include "Raw.h"
#include "Matrix.h"
#include "NA_Data.h"
#include "LatLon.h"

/*
*/
class Grid;

struct GridBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Grid"; }
    static const char *env_mode() { return "v"; }   // instance environment table, weak values 
    static const LuaNew_ID & id() { return ID; }
    typedef Grid CAST_T;

  private:
    static int __index( lua_State *L ) throw();
#ifdef METQU
    static int __newindex( lua_State *L ) throw();
#endif
};

/*
* Holds certain time, level and projection/gridsize combination and allows reading 
* all parameters from that.
*/
class Grid : public LuaNew<GridBind> {
  public:
#ifdef METQU
    Grid( Raw_interface &r_, const JDay &vt, const NA_Level &lev, const Projection &projection, const MatrixPos &gs= MatrixPos::ZERO
        //
    	// 22-Sep-2011 PKi: For fetching cross using newbase
    	//
        , std::vector<JDay> const *vtVec = nullptr
        , std::vector<NA_Level> const *levelVec = nullptr
        , LatLonList const *locs = nullptr
		, bool flightRoute = false
		, const DataIdList *dataIds = nullptr
        ) throw(E_NO_MATCH);
#endif
    Grid( const Raw_interface &r_, const JDay &vt, const NA_Level &lev, const Projection &projection, const MatrixPos &gs= MatrixPos::ZERO
        //
        // 22-Sep-2011 PKi: For fetching cross using newbase
        //
        , std::vector<JDay> const *vtVec = nullptr
        , std::vector<NA_Level> const *levelVec = nullptr
        , LatLonList const *locs = nullptr
		, bool flightRoute = false
		, const DataIdList *dataIds = nullptr
    	) throw(E_NO_MATCH);

    void set_rkey(unsigned rk) {
        assert( rkey==0 );
        assert( rk!=0 );
        rkey= rk;
    }
    unsigned get_rkey() { return rkey; }

    CONST_IF_SERVER ApiMatrix *push_Matrix( lua_State *L, const ApiParam &p ) CONST_IF_SERVER;

    float at( const LatLon &ll, const NA_Param &p ) const;   // single value at given location

    const MatrixPos &getGridSize() const { return gridsize; }   // 'MatrixPos::ZERO' for native gridsize
    NA_Info getInfo() const { return *data; }   // does a copy

#ifdef METQU
    void setMatrix( const NA_Param &param, const Matrix &m ) throw(E_READONLY, E_BAD_SIZE, E_NO_MATCH);
    void setMatrix( const NA_Param &param, float v ) throw(E_READONLY, E_NO_MATCH);

    Matrix *new_ScalarMatrix( const NA_Param &p ) {   // caller must 'delete'
        return push_ScalarMatrix( 0, p );
    }
#endif

    CONST_IF_SERVER Matrix *push_ScalarMatrix( lua_State *L, const NA_Param &p ) CONST_IF_SERVER;

  private:
    Grid( const Grid &o );    // copying not allowed
    Grid &operator= ( const Grid &o );    // assignment not allowed

#ifdef METQU
    ApiMatrix *push_NativeMatrix( lua_State *L, const ApiParam &p );
    Matrix *push_NativeScalarMatrix( lua_State *L, const ApiScalarParam &p );
#endif

    // data members:
    //
#ifdef METQU
    NA_Data *data;
#else
    const NA_Data *data;
#endif

    const JDay vt;
    const NA_Level level;

    const Projection proj;
    const MatrixPos gridsize;       // 'MatrixPos::ZERO' for native grid size, otherwise non-native (do scaling)

    unsigned rkey;            // magic number used by 'push_alive()' to get a Lua instance of 'r'
                              // needed for connecting also '[SQD|MQD]_Matrix' objects to the same instance's
                              // lifecycle. (0 immediately after construction, but soon set thereafter)
                              
    const std::vector<JDay> vtVec;			// 22-Sep-2011 PKi: For fetching cross using newbase
    const std::vector<NA_Level> levelVec;	//
    const LatLonList locs;					//
	bool flightRoute;						// 05-Mar-2015 PKi: Flag for flightroute query (one value per loc/level/time)
	DataIdList dataIds;						// 07-Apr-2015 PKi: Data point (e.g. observation station) id's for point data query

    friend class GridBind;

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant(data);

        assert_invariant(vt);
        
        // 'gridsize' is allowed to be empty (marking "native grid size for each param tile")
    }
#endif
};

#endif
    // GRID_H
