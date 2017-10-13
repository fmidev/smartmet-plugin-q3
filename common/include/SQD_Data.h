/*
* SQD_DATA.H                            Copyright (c) 2009-10, Ilmatieteen laitos
*
* Adapter for Newbase SQD files
*
* Revised:  22-Oct-2010
*/
#ifndef SQD_DATA_H
#define SQD_DATA_H

#ifndef USE_NEWBASE
# error "This file shouldn't be included when compiling without Newbase."
#endif

#include "Tools.h"

#include "NA_Data.h"
#include "NA_Level.h"

#include "JDay.h"

#include "newbase/NFmiParameterName.h"

class Matrix;
class MemMatrix;
class VectorMatrix;

class NFmiQueryData;
class NFmiFastQueryInfo;

/*
* Covers a single "raw" data (SQD file)
*/
class SQD_Data : public NA_Data {
  public:
    SQD_Data( const char *fn ) throw(E_BAD_FILE);

#ifdef METQU
    SQD_Data( const NA_Info &info ) throw();    // create new, values not initialized
#endif

    ~SQD_Data();

#ifdef METQU
    /*virtual*/ bool isReadOnly() const { return is_readonly; }
    /*virtual*/ void output( std::ostream &out, ProgressCallback *cb ) const;
#endif

    /*virtual*/ CONST_IF_SERVER Matrix *push_NativeMatrix( lua_State *L, 
                                const JDay &vt, 
                                const NA_Level &lev, 
                                const NA_Param &p
								//
								// 22-Aug-2011 PKi: For fetching data using newbase interpolation, projection and scaling
								//
							  , const Projection *target_proj = NULL
							  , const MatrixPos *target_gs = NULL
							  , bool  *target_ready = NULL
							  , const DataIdList *dataIds = NULL
                              ) CONST_IF_SERVER throw();

    static NA_Info read_info( const char *fn ) throw(E_BAD_FILE);
    
    /*virtual*/ bool providesPressureLevelsFromHybrid() const;

    /*
      31-Oct-2011 PKi: Virtual parameter mapping for native parameters
    */
    static void mapParameter(const FmiParameterName id, std::map<std::string,FmiParameterName> &virtualParams);

  private:
    void setIter( NFmiFastQueryInfo &fi, const JDay &vt, const NA_Level &lev, const NA_Param &param,
                  bool &exact_time, bool &exact_level ) throw(E_NO_MATCH);

    void setIter_exact( NFmiFastQueryInfo &fi, const JDay &vt, const NA_Level &lev, const NA_Param &param ) throw();

#ifdef METQU
    static NFmiQueryData *new_qd( const NA_Info &info, const MatrixPos &gs ) throw(E_USAGE);
#endif

    MatrixPos getGridSize(size_t nPoints = 0) const;

    //FmiParameterName param_id_( const NA_Param &p ) const;

    // Note: Must have different name from 'push_NativeMatrix' (this is not virtual)
    //
    CONST_IF_SERVER Matrix *push_NativeMatrix_e( lua_State *L, 
                                const JDay &vt, 
                                const NA_Level &lev, 
                                FmiParameterName e
								//
								// 22-Aug-2011 PKi: For fetching data using newbase interpolation, projection and scaling
								//
							  , const Projection *target_proj = NULL
							  , const MatrixPos *target_gs = NULL
							  , bool  *target_ready = NULL
							  , const DataIdList *dataIds = NULL
                              ) CONST_IF_SERVER throw();

    //
    // 22-Sep-2011 PKi: For fetching cross using newbase
    //
    /*virtual*/ CONST_IF_SERVER Matrix *push_NativeCross( lua_State *L,
    						                              const std::vector<JDay> &vtVec,
    						                              const std::vector<NA_Level> &levelVec,
    						                              const LatLonList &locs,
    						                              const NA_Param &p,
    						                              bool flightRoute
    						                            ) const;

    //
    // 29-Sep-2011 PKi: For fetching grid locations and data point (e.g. observation station) metadata
    //
    /*virtual*/ void getLocations(std::vector<LatLon> &locations,const Projection *target_proj = NULL) const;
    /*virtual*/ void getDataIds(std::vector<unsigned long> &dataIds,const Projection *target_proj = NULL) const;
    /*virtual*/ void getDataNames(std::vector<std::string> &dataNames,const Projection *target_proj = NULL) const;

    /*
    * 13-Oct-2011 PKi: For checking the availability of height levels in data
    */
    /*virtual*/ bool providesHeightLevelsFromHybrid() const;

    /*
      18-Oct-2011 PKi: Primary/secondary parameter mapping for virtual parameters
    */
    /*virtual*/ FmiParameterName mapParameter(const NA_Param &p,bool primary = true,bool *mapped = NULL) const;

    // For 'SQD_Matrix'
    //
#ifdef METQU
    NFmiQueryData *getQD() { return qd; }
#else
    /*const*/ NFmiQueryData *getQD() const { return qd; }
#endif

    friend class SQD_Matrix;

    // data members
    //
    // Note: 'NFmiQueryData' is const in server mode, but since Newbase 'NFmiFastQueryInfo' constructor
    //       does not allow a const 'NFmiQueryData', we cannot declare it as const here. Bohooo... :(
    //
#ifdef METQU
    NFmiQueryData * const qd;
    const bool is_readonly;
#else
    /*const*/ NFmiQueryData * const qd;
    // always read-only
#endif

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant(qd);
    }
#endif
};

// 12-Sep-2011 PKi: Not static to be seen by other modules
//
bool derived_from( unsigned which, const NA_Param &p );

#endif
    // SQD_DATA_H
