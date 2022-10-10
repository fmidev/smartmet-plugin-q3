/*
 * NA_DATA.H                            Copyright (c) 2009-10, Ilmatieteen
 * laitos
 *
 * Revised:  19-Oct-10 AKa
 */
#ifndef NA_DATA_H
#define NA_DATA_H

#include "JDay.h"
#include "MatrixPos.h"
#include "MoreTools.h"
#include "Projection.h"
#include "Tools.h"

#include "NA_Level.h"

#include <list>
#include <map>
#include <string>

class Matrix;
class LatLonBlanket;
class ApiParam;

typedef std::list<unsigned long> DataIdList;

//
// 22-Aug-2011 PKi: If USE_NEWBASE_PROJ is defined, using newbase interpolation,
// projection and scaling
//					(skipping the code build into Q3)
//
#define USE_NEWBASE_PROJ

// 31-Aug-2011 PKi: Moved to NA_Data.cpp from SQD_Data.cpp
//
std::vector<NA_Level> reverse_vector(const std::vector<NA_Level> &vec);

/*
 * Information about a 'NA_Data' derived data source (s.a. 'SQD_Data' or
 * 'MQD_Data').
 *
 * Can be used also as-is, for carrying requirements for a to-be-created
 * 'NA_Data' derivative.
 *
 * 'extra' carries format specific fields s.a.
 *       SQD format:
 *           "producer=NNN"          producer number (compulsory by SQD format)
 *           "gridsize=xx,yy"        global grid size of the data
 *           "combo19=true|false"    whether ":19" (TotalWindMS) is used
 * (carries multiple parameters) "combo326=true|false"   whether ":326"
 * (WeatherAndCloudiness) is used (carries multiple parameters)
 *           "revlevorder=true|false" whether levels in NA_Info are reverse
 * sorted "relative_uv=true|false" whether U and V are relative to the grid
 *
 *       MQD format:
 *           TBD: most likely we don't need this:
 *           "binary_offset=NNN"     start of binary block (16 byte aligned)
 * TBD: not sure if this is really needed
 */
class NA_Info {
public:
  NA_Info(const time_t loadTime_, const time_t modificationTime_,
          const JDay &ot_, const std::vector<JDay> &times_,
          const std::string &nativeLevelType,
          const std::vector<NA_Level> &levels_,
          const std::vector<NA_Param> &params_, const Projection &projection_,
          const std::string &wkt_ = "", const uint timeStep_ = 0,
          const double dx_ = 0, const double dy_ = 0,
          const bool gridData_ = true) throw();

  JDay getLoadTime() const { return loadTime; }
  JDay getModificationTime() const { return modificationTime; }
  JDay getOriginTime() const { return ot; }
  const std::vector<JDay> &getTimes() const { return times; }
  const std::string &getNativeLevelType() const { return nativeLevelType; }
  const std::vector<NA_Level> &getLevels() const { return levels; }
  bool getLevels(std::vector<NA_Level> &vec, bool revorder) const {
    vec = (revorder ? reverse_vector(levels) : levels);
    return revorder;
  }
  const std::vector<NA_Param> &getParams() const { return params; }
  const Projection &getProjection() const { return proj; }
  const std::string &getWKT() const { return wkt; }
  uint getTimeStep() const { return timeStep; }
  double getDx() const { return dx; }
  double getDy() const { return dy; }
  bool hasGrid() const { return gridData; }

  string_or_null getExtra_fn() const {
    return getExtra_str(EXTRA_KEY_FILENAME);
  }
  NA_Info &setExtra_fn(const char *fn) {
    return setExtra(EXTRA_KEY_FILENAME, fn);
  }

#ifdef USE_NEWBASE
  /*
  * Note: These are the currently known magic numbers (from
'NFmiProducerName.h'):
<<
enum FmiProducerName
{
      kFmiNoProducer = 0,		//!< Fmi number for Unspecified producer
      kFmiHIRLAM = 1,			//!< Fmi number for Hirlam
      kFmiHIRMESO = 2,		//!< Fmi number for Hirmeso
      kFmiTAFHIR = 11,		//!< Fmi number for TAFHIR
      kFmiMesan = 104,		//!< Fmi number for smesan
      kFmiECMWF = 131,		//!< Fmi number for ECMWF
      kFmiSmhiMesan = 160,	//!< Fmi number for Smhi mesan2
      kFmiMTAHIRLAM = 230,	//!< Fmi number for MTAHIRLAM
      kFmiMTAHIRMESO = 220,	//!< Fmi number for MTAHIRMESO
      kFmiMTAECMWF = 240,		//!< Fmi number for MTAECMWF
      kFmiSYNOP = 1001,		//!< Fmi number for synop observations
      kFmiSHIP = 1002,		//!< Fmi number for SHIP observations
      kFmiTEMP = 1005,		//!< Fmi number for TEMP sounding observations
      kFmiRAWTEMP = 1006,		//!< Fmi number for RAW-TEMP sounding observations
(used in MetEditor)
      kFmiFlashObs = 1012,	//!< Fmi number for flash detection producer
      kFmiRADARNRD = 1014,	//!< Fmi number for nrd radar
      kFmiBUOY = 1017,		//!< Fmi number for BUOY observations
      kFmiRoadObs = 1023,		//!< Fmi number for road observation station
data kFmiMETAR = 1025,		//!< Fmi number for METAR messages kFmiTestBed =
1101,		//!< Fmi number for testbed vxt data kFmiMETEOR = 2001,
//!< Fmi number for meteorologist kFmiAUTOMATIC = 2004,	//!< Fmi number for
automatic (derived) values kFmiTAFMET = 2011		//!< Fmi number for
TAFMET
};
<<
  */
  int getExtra_sqd_producer() const {
    return getExtra_int(EXTRA_KEY_SQD_PRODUCER, -1);
  }
  NA_Info &setExtra_sqd_producer(unsigned v) {
    return setExtra(EXTRA_KEY_SQD_PRODUCER, v);
  }

  MatrixPos getExtra_sqd_gridsize() const {
    return getExtra_pos(EXTRA_KEY_SQD_GRIDSIZE);
  }
  NA_Info &setExtra_sqd_gridsize(const MatrixPos &pos) {
    return setExtra(EXTRA_KEY_SQD_GRIDSIZE, pos);
  }

  bool getExtra_sqd_combo19() const {
    return getExtra_bool(EXTRA_KEY_SQD_COMBO19);
  }
  NA_Info &setExtra_sqd_combo19(bool v) {
    return setExtra(EXTRA_KEY_SQD_COMBO19, v);
  }

  bool getExtra_sqd_combo326() const {
    return getExtra_bool(EXTRA_KEY_SQD_COMBO326);
  }
  NA_Info &setExtra_sqd_combo326(bool v) {
    return setExtra(EXTRA_KEY_SQD_COMBO326, v);
  }

  // 31-Aug-2011 PKi: Flag for level order (true if reversed)
  bool getExtra_sqd_revlevorder() const {
    return getExtra_bool(EXTRA_KEY_SQD_REVLEVORDER);
  }
  NA_Info &setExtra_sqd_revlevorder(bool v) {
    return setExtra(EXTRA_KEY_SQD_REVLEVORDER, v);
  }

  bool getExtra_sqd_RelativeUV() const {
    return getExtra_bool(EXTRA_KEY_SQD_RELATIVE_UV);
  }
  NA_Info &setExtra_sqd_RelativeUV(bool v) {
    return setExtra(EXTRA_KEY_SQD_RELATIVE_UV, v);
  }
#endif

  std::vector<NA_Param>::const_iterator getParamsBegin() const {
    return params.begin();
  }
  std::vector<NA_Param>::const_iterator getParamsEnd() const {
    return params.end();
  }

private:
  static const char *EXTRA_KEY_FILENAME;
#ifdef USE_NEWBASE
  static const char *EXTRA_KEY_SQD_PRODUCER;
  static const char *EXTRA_KEY_SQD_GRIDSIZE;
  static const char *EXTRA_KEY_SQD_COMBO19;
  static const char *EXTRA_KEY_SQD_COMBO326;
  static const char *EXTRA_KEY_SQD_REVLEVORDER;
  static const char *EXTRA_KEY_SQD_RELATIVE_UV;
#endif

  string_or_null getExtra_str(const char *key) const;
  NA_Info &setExtra(const char *key, const char *v);

  int getExtra_int(const char *key, int def) const;
  NA_Info &setExtra(const char *key, unsigned v);

  MatrixPos getExtra_pos(const char *key) const;
  NA_Info &setExtra(const char *key, const MatrixPos &pos);

  bool getExtra_bool(const char *key) const;
  NA_Info &setExtra(const char *key, bool v);

  const time_t loadTime;
  const time_t modificationTime;
  const JDay ot; // origintime
  const std::vector<JDay> times;
  const std::string nativeLevelType;
  const std::vector<NA_Level> levels;
  const std::vector<NA_Param>
      params; // scalar params offered to API (host params not listed)
  const Projection proj;
  const std::string wkt;
  uint timeStep;
  double dx;
  double dy;
  bool gridData;

  std::map<std::string, std::string> extra;

private:
  NA_Info &operator=(
      const NA_Info &o); // not allowed (so we can keep the fields 'const')

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(ot);
    assert_invariant(times.size() > 0);
    assert_invariant(levels.size() > 0);
    assert_invariant(params.size() > 0);

    // Note: We do allow 'proj' to be "no projection" (i.e. 'TestRaw' uses that)
  }
#endif
};

/*
 * Abstract base class for 'SQD_Data' and 'MQD_Data' (and other possible data
 * adapters)
 */
class NA_Data : public NA_Info {
public:
  /*
   */
#ifdef METQU
  struct ProgressCallback {
    ProgressCallback(lua_State *L_, int f_index_)
        : L(L_), f_index(L_ABS(f_index_)), t0(now_ms()) {}

    bool /*cancel*/ progress(unsigned long current, unsigned long total,
                             const char *a = 0, const char *b = 0,
                             const char *c = 0);

  private:
    lua_State *L;
    unsigned f_index; // 0= no callback, otherwise the function to call (in 'L')
    uint64_t t0;      // start time
  };
#endif

  NA_Data(const NA_Info &info) : NA_Info(info) {}
  virtual ~NA_Data(){};

#ifdef METQU
  virtual bool isReadOnly() const = 0;
  virtual void output(std::ostream &os, ProgressCallback *cb) const = 0;
#endif

  // Providing data in the native projection
  //
  // Note: We cannot have the same function name for both a 'virtual' and a
  // non-virtual (for METQU,
  //       providing 'const' behaviour).
  //
#ifdef METQU
  const Matrix *
  push_NativeMatrix_const(lua_State *L, const JDay &vt, const NA_Level &lev,
                          const NA_Param &p
                          //
                          // 22-Aug-2011 PKi: For fetching data using newbase
                          // interpolation, projection and scaling
                          //
                          ,
                          const Projection *target_proj_orig = nullptr,
                          const MatrixPos *target_gs_orig = nullptr,
                          bool *target_ready = nullptr,
                          const DataIdList *dataIds = nullptr) const {
    return const_cast<NA_Data *>(this)->push_NativeMatrix(
        L, vt, lev, p, target_proj_orig, target_gs_orig, target_ready);
  }
#endif

  virtual CONST_IF_SERVER Matrix *
  push_NativeMatrix(lua_State *L, const JDay &vt, const NA_Level &lev,
                    const NA_Param &p
                    //
                    // 22-Aug-2011 PKi: For fetching data using newbase
                    // interpolation, projection and scaling
                    //
                    ,
                    const Projection *target_proj_orig = nullptr,
                    const MatrixPos *target_gs_orig = nullptr,
                    bool *target_ready = nullptr,
                    const DataIdList *dataIds = nullptr) CONST_IF_SERVER = 0;

  // Providing data with projection change (causes a read-only matrix if
  // projection and/or grid size is not native)
  //
  // Note: This function is NOT a pure virtual. We offer a default
  // implementation that does projection
  //      using proj4. However, a certain derived data provider may choose to
  //      override it, in case it thinks it can perform projections more
  //      effectively (i.e. only read part of the data in) or if it sports
  //      non-proj4 projection strings (s.a. Newbase for SQD data).
  //
  virtual const Matrix *push_Matrix(lua_State *L, const JDay &vt,
                                    const NA_Level &lev, const NA_Param &p,
                                    const Projection &proj,
                                    const MatrixPos &target_gs,
                                    const DataIdList *dataIds = nullptr) const;

  // Some levels are calculated by the adaptors, on-the-fly (height levels from
  // hybrid or pressure data; pressure levels from hybrid data). Checks
  // availablility of these.
  //
  virtual bool providesPressureLevelsFromHybrid() const = 0;

  /*
   * 22-Sep-2011 PKi: For fetching cross using newbase; overridden by SQD_Data,
   * uses native projection and gridsize
   */
  virtual CONST_IF_SERVER Matrix *
  push_NativeCross(lua_State *L, const std::vector<JDay> &vtVec,
                   const std::vector<NA_Level> &levelVec,
                   const LatLonList &locs, const NA_Param &p,
                   bool flightRoute) const {
    return nullptr;
  }

  /*
   * 29-Sep-2011 PKi: For fetching grid locations and data point (e.g.
   * observation station) metadata; overridden by SQD_Data
   */
  virtual void getLocations(std::vector<LatLon> &locations,
                            const Projection *target_proj = nullptr) const {
    return;
  }
  virtual void getDataIds(std::vector<unsigned long> &dataIds,
                          const Projection *target_proj = nullptr) const {
    return;
  }
  virtual void getDataNames(std::vector<std::string> &dataNames,
                            const Projection *target_proj = nullptr) const {
    return;
  }

  /*
   * 13-Oct-2011 PKi: For checking the availability of height levels in data;
   * overridden by SQD_Data
   */
  virtual bool providesHeightLevelsFromHybrid() const { return false; }

  /*
   * 18-Oct-2011 PKi: For virtual parameter mapping; overridden by SQD_Data
   */
  virtual FmiParameterName mapParameter(const NA_Param &p, bool primary,
                                        bool *mapped) const {
    *mapped = false;
    return (FmiParameterName)0;
  }

private:
};

#endif
// NA_DATA_H
