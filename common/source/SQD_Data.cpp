/*
 * SQD_Data.CPP                          Copyright (c) 2009-10, Ilmatieteen laitos
 *
 * Revised:  22-Oct-2010
 */
#include "SQD_Data.h"
#include "SQD_Matrix.h"
#include "SQD_Projection.h"
#include "SQD_Tools.h"

#include "NA_Level.h"

#include "MemMatrix.h"
#include "Projection.h"

#include "newbase/NFmiAreaFactory.h"
#include "newbase/NFmiFastQueryInfo.h"
#include "newbase/NFmiGrid.h"
#include "newbase/NFmiMetMath.h"
#include "newbase/NFmiMetTime.h"
#include "newbase/NFmiQueryData.h"
#include "newbase/NFmiQueryDataUtil.h"
#include "newbase/NFmiTimeList.h"
#include "newbase/NFmiTotalWind.h"
#include "newbase/NFmiWeatherAndCloudiness.h"

#include <cstring>

#include <boost/lexical_cast.hpp>

#include <sys/stat.h>
#include <time.h>

#include "newbase/NFmiRotatedLatLonArea.h"

using namespace std;

/*
 * If we need to poll data for height (ids 2,3) or pressure (id 1) params, this is how many
 * NANs are tolerated before turning our back. Ideally, if there are such params, they should
 * have valid values as well.
 *
 * ( Data issue reported to Jari Winberg. 26-Oct-10 AKa )
 */
const unsigned HEIGHT_POLLING_GIVING_UP_LIMIT = 500;

/*
 * Define if we know 1..n indexing of matrices follows the same rules (y outermost, bottom to up,
 * x innermost, left to right) as Newbase does.
 */
#define SQD_AND_Q3_INDEXING_ARE_THE_SAME

/*
 * Newbase 'NFmiFastQueryInfo::PressureLevelValue()' can change the current parameter if requested
 * pressure level is not available. This is a bug in Newbase.
 */
#define PRESSURELEVELVALUE_HAS_SIDE_EFFECTS

/*---=== Helpers ===---*/

/* Note: Wasn't able to do this as a template.
 *
 * 31-Aug-2011 PKi: Moved to NA_Data.cpp
 */
/*
static vector<NA_Level> reverse_vector( const vector<NA_Level> &vec ) {
    vector<NA_Level> vec2( vec.size() );
    unsigned n= vec.size();

    for( unsigned i=0; i<n; i++ ) {
        vec2[n-1-i]= vec[i];
    }
    return vec2;
}
*/

/*
 * Returns:
 *   -1 if 'param' exists and its values are descending
 *   0 if 'param' does not exist (or it does but orientation does not matter)
 *   +1 if 'param' exists and its values are ascending
 *
 * Note: 'fi' param and level iterators are changed
 *
 * NOTE: Newbase has similar tools for doing this, maybe we should use them?
 *       (tried earlier but never got them working fine - maybe another try together with Marko?)
 *       26-Oct-10 AKa
 */
static int TestRising(NFmiFastQueryInfo &fi, FmiParameterName id, const char *fn_debug)
{
  (void)fn_debug;

  fi.Reset();
  if (!fi.Param(id))
  {
    return 0;  // not in data
  }

  unsigned n = 0;

  // Loop through as much of the data until we get two values in the same time,location (on
  // different levels). There may be missing values between such two values; does not matter.
  //
  for (/*fi.ResetTime()*/; fi.NextTime();)
  {
#if 1
    assert(fi.IsTimeUsable());

    JDay vt(SQD_Tools::mt2jd(fi.Time()));
    string_or_null time_str = vt.toString();
#endif
    for (fi.ResetLocation(); fi.NextLocation();)
    {
      float v[2];
      unsigned found = 0;
#if 1
      assert(fi.IsLocationUsable());
      /*
      NFmiPoint xy= fi.LatLon();  // this would segfault
      double x= xy.X();
      double y= xy.Y();
      */
#endif
      for (fi.ResetLevel(); fi.NextLevel();)
      {
        ++n;
#if 1
        assert(fi.IsLevelUsable());

        const NFmiLevel *nlev = fi.Level();
        FmiLevelType lt = nlev->LevelType();
        long lv = (long)nlev->LevelValue();
#endif
        /* NOTE: Must be repeated, '.NextLevel()' may have changed the param setting.
         */
        fi.Param(id);

        float a = fi.FloatValue();
#if 1
        if (n >= HEIGHT_POLLING_GIVING_UP_LIMIT)
        {
          LOG_DEBUG(
              "GIVING UP parameter orientation detection. Over %d missing values for param %d.",
              n,
              (int)id);
          return 0;
        }
#endif
#if 0
                if (n < 10) {
                    LOG_DEBUG( "%s %d:%d -> %f", time_str.c_str(), (int)lt,(int)lv, a );
                }
#else
        (void)lt;
        (void)lv;
        (void)n;
#endif
        if (a != kFloatMissing)
        {
          v[++found] = a;
          if (found == 2)
          {
            if (v[1] == v[0])
            {  // heights should not be the same; if they are, continue looking
              found--;
            }
            else
            {
              return (v[1] > v[0]) ? +1 : -1;
            }
          }
        }
      }
    }
  }

  LOG_WARNING("Material with COMPLETELY MISSING param %d values. ('%s')", (int)id, fn_debug);

  return 0;  // There were no two values in same time,location *anywhere* in the data - orientation
             // does not matter
}

/*
 * Is a parameter part of the 326 (WeatherAndCloudiness) or 19 (TotalWindMS) combo.
 *
 * 12-Sep-2011 PKi: Not static to be seen by other modules
 *
 */
#ifdef METQU
bool derived_from(unsigned which, const NA_Param &p)
{
  string std_name = p.getStandardName();
  if (std_name == "")
  {
    return false;  // no standard name (some custom param)
  }

  FmiParameterName id = NA_Param::standard_param_native_id(std_name.c_str());

  switch (id)
  {
    // WeatherAndCloudiness (326) derivatives
    //
    case kFmiTotalCloudCover:          // N
    case kFmiLowCloudCover:            // CL
    case kFmiMediumCloudCover:         // CM
    case kFmiHighCloudCover:           // CH
    case kFmiPrecipitation1h:          // RR
    case kFmiPrecipitationType:        // PRET
    case kFmiPrecipitationForm:        // PREF
    case kFmiFogIntensity:             // FOG
    case kFmiProbabilityThunderstorm:  // THUND
    case kFmiWeatherSymbol1:           // HSADE
    case kFmiWeatherSymbol3:           // HESSAA
    case kFmiMiddleAndLowCloudCover:   // MiddleAndLowCloudCover
      return (which == 326);

    case kFmiWindDirection:      // WD
    case kFmiWindSpeedMS:        // WS
    case kFmiHourlyMaximumGust:  // GUST
    case kFmiWindUMS:            // U
    case kFmiWindVMS:            // V
    case kFmiWindVectorMS:       // WVEC
      return (which == 19);

    default:
      return false;
  }
}
#endif

/*
 * Fill 'MemMatrix' from a Newbase 'NFmiDataMatrix<float>'
 */
static void matrix_fill(MemMatrix &m, const NFmiDataMatrix<float> &from)
{
  typedef NFmiDataMatrix<float>::size_type sz_t;
  const sz_t xs = from.NX();
  const sz_t ys = from.NY();
  MatrixPos::offset_t n = 0;
  float v;

  for (sz_t y = 0; y < ys; y++)
  {
    for (sz_t x = 0; x < xs; x++)
    {
      // 7-Sep-2011 PKi: Set missing values to NAN
      //
      // m[n++]= from[x][y];
      m[n++] = (((v = from[x][y]) == kFloatMissing) ? NAN : v);
    }
  }
  assert(n == m.getN());
}

/*
 * Sets an iterator to 'NFmiQueryData'.
 *
 * Returns true if the requested time and level are exact (and param existed).
 *         false if the values are interpolatable.
 *
 * NOTE: Expecting trouble with 'NFmiFastQueryInfo' copy constructor, so changing 'fi' as reference.
 */
void SQD_Data::setIter(NFmiFastQueryInfo &fi,
                       const JDay &vt,
                       const NA_Level &lev,
                       const NA_Param &p,
                       bool &exact_time,
                       bool &exact_level) throw(E_NO_MATCH)
{
  // Note: 'p' is one of the available params of 'this' and must carry its id.
  //       (native name having ':NNN' tail)

  fi.Reset();

  NFmiMetTime mt = SQD_Tools::jd2mt(vt);

  exact_time = fi.Time(mt);
  if (!exact_time)
  {
    // Check if time is even within the valid range
    //
    if (!fi.TimeDescriptor().IsInside(mt))
    {
      throw E_NO_MATCH(vt);
    }
  }

  LOG_DEBUG("Exact time: %d", (int)exact_time);

  // This checks the validity of the level (that if ground level is required, data only has one;
  // that if interpolation is required the data actually can provide such a level).
  //
  bool exact;
  if (!lev.covered_by(*this, exact))
  {
    throw E_NO_MATCH(lev);  // outside
  }
  else if (exact)
  {
    if ((!lev) || lev.isGroundLevel())
    {
      fi.FirstLevel();
    }
    else
    {
      fi.Level(SQD_Tools::newbase_level(lev));
    }
  }
  exact_level = exact;

  // Get the Newbase enumeration for 'p'
  //
  FmiParameterName e = SQD_Tools::param_id(p);

  // Note: This must be *after* setting the level (otherwise parameter may get changed, i.e. to 'P'
  //       when using interpolated pressure level).
  //
  if (!fi.Param(e))
  {
    throw E_NO_MATCH(p);
  }
}

/*
 * Set an iterator to 'NFmiQueryData', knowing the time, level, param exist.
 */
void SQD_Data::setIter_exact(NFmiFastQueryInfo &fi,
                             const JDay &vt,
                             const NA_Level &lev,
                             const NA_Param &p) throw()
{
  bool exact_time, exact_level;

  try
  {
    setIter(fi, vt, lev, p, exact_time, exact_level);
    assert(exact_time);
    assert(exact_level);
  }
  catch (const E_NO_MATCH &e)
  {
    throw E_LOG_BUG("Supposed to have had exact time, level and param: %s", e.what());
  }
}

/*
 * Return the times available, in ascending order.
 */
static vector<JDay> getTimes(NFmiQueryInfo &info) /*throw(E_BUG)*/
{
  vector<JDay> vec;

  info.ResetTime();

  JDay vt_last;
  while (info.NextTime())
  {
    JDay vt(SQD_Tools::mt2jd(info.Time()));  // conversion from 'NFmiMetTime'
    if (vt_last && (vt < vt_last))
    {
      throw E_LOG_BUG("SQD data had times in non-ascending order: %s > %s",
                      vt_last.toString().c_str(),
                      vt.toString().c_str());
    }

    vec.push_back(vt);
    vt_last = vt;
  }

  return vec;
}

/*
 * Returns the levels available, in an order "from the ground up" (height param ascending, pressure
 * descending)
 *
 * Throws E_BAD_FILE on file conformance issues.
 */
static vector<NA_Level> getLevels(const NFmiQueryInfo &info,
                                  const char *fn_debug,
                                  bool &revorder,
                                  string &nativeLevelType) throw(E_BAD_FILE)
{
  vector<NA_Level> vec;

  // We need 'FastQueryInfo()' for 'HeightValueAvailable()' and others.
  //
  NFmiFastQueryInfo fi(info);

  // A single level with type 5000 (or anything else we didn't get a name on)
  // will be reported as ground level. If there are more levels, such levels
  // _won't_ be reported.
  //
  bool any_level_found = revorder = false;

  //---
  // Collect 'vec' first - reverse later if necessary.
  //
  nativeLevelType.clear();

  fi.ResetLevel();
  while (fi.NextLevel())
  {
    const NFmiLevel *nlev = fi.Level();  // Gives a pointer, means no harm.
    assert(nlev);

    FmiLevelType lt = nlev->LevelType();
    if (nativeLevelType.empty()) nativeLevelType = boost::lexical_cast<string>((int)lt);
    long lv = (long)nlev->LevelValue();  // is 'long' for Newbase heritage

    any_level_found = true;

    if ((lt != kFmiGroundSurface) && (lt != kFmiHybridLevel) && (lt != kFmiPressureLevel) &&
        (lt != kFmiSoundingLevel))
    {
      continue;  // skip such levels (i.e. 5000)
    }

    NA_Level tmp = SQD_Tools::q3_level(lt, lv);
    vec.push_back(tmp);
  }

  if (vec.size() == 0)
  {
    if (any_level_found)
    {
      vec.push_back(NA_Level(NA_Level::GROUND_LEVEL));
      return vec;
    }

    throw E_LOG_BAD_FILE(fn_debug, "no levels");
  }

  // Figure out if we need to reverse 'vec'

  //---
  // Rule #0. Having just one level (MUST be ground level) - order does not matter
  //
  // Order does not matter for sounding data either, we'll just return the data as is
  //
  if (vec.size() == 1 || vec[0].isSoundingLevel())
  {
    return vec;
  }

  //---
  // Rule #1. If the file has pressure levels (then it ONLY has pressure levels in SQD), they should
  // be given in
  //          order from the highest to lowest (since pressure at ground level is highest).
  //
  if (vec[0].isPressureLevel())
  {
    if (vec[0].getValue() > vec[1].getValue())
    {
      return vec;
    }
    else
    {
    REVERSE:
      // Note: Seems 'std::reverse()' is not available.
      //
      // 31-Aug-2011 PKi: Must use original level order when creating a raw object using template
      //				    (otherwise data will be copied between unequal levels);
      // inform about reversed order

      revorder = true;
      return reverse_vector(vec);
    }
  }

  //---
  // Rule #2. Find the order of height parameters (:2 and/or :3) in the file
  //
  // 1: order is ascending (fine, must not reverse)
  // 0: no such param or no comparable height values - either order is fine
  // -1: order must be reversed
  //
  int p2_order = TestRising(fi, kFmiGeopHeight /*2*/, fn_debug);
  int p3_order = TestRising(fi, kFmiGeomHeight /*3*/, fn_debug);

  if (p2_order * p3_order < 0)
  {
    throw E_LOG_BAD_FILE(fn_debug, "Params 2 and 3 grow in different directions!");
  }

  if ((p2_order > 0) || (p3_order > 0))
  {
    return vec;
  }
  else if ((p2_order < 0) || (p3_order < 0))
  {
    goto REVERSE;
  }

  //---
  // Rule 3. Pressure available as a parameter (:1)
  //
  int p1_order = TestRising(fi, kFmiPressure /*1*/, fn_debug);

  if (p1_order > 0)
  {
    return vec;
  }
  else if (p1_order < 0)
  {
    goto REVERSE;
  }

  //---
  // End of rules - seems the order is unimportant.
  //
  return vec;
}

static std::string interpolation_name(FmiInterpolationMethod method)
{
  switch (method)
  {
    case kNoneInterpolation:
      return "None";
    case kLinearly:
      return "Linear";
    case kNearestPoint:
      return "NearestPoint";
    case kByCombinedParam:
      return "ByCombinedParam";
    case kLinearlyFast:
      return "LinearlyFast";
    case kLagrange:
      return "Lagrange";
    default:
      return "Unknown";
  }
}

void addParam(vector<NA_Param> &vec, NFmiQueryInfo &info, FmiParameterName e)
{
  std::string interpolation, precision;

  if (info.Param(e))
  {
    interpolation = interpolation_name(info.Param().GetParam()->InterpolationMethod());
    precision = info.Param().GetParam()->Precision().CharPtr();
  }

  vec.push_back(NA_Param(e, interpolation, precision));
}

uint getDataTimeStep(NFmiQueryInfo &info)
{
  uint timeStep = 0;

  if (info.FirstTime())
  {
    NFmiTime t1 = info.ValidTime();
    timeStep = (info.NextTime() ? info.ValidTime().DifferenceInMinutes(t1) : 60);

    info.FirstTime();
  }

  return timeStep;
}

/*
 * Returns: vector of parameters provided by the data
 *
 * Note: Only parameters that are actually accessible are listed, the "host" params
 *       ":326" and ":19" are not. They are indicated by 'has_326' and 'has_19' flags.
 *
 * Actual parameter names as stored in the SQD file are returned (i.e. "Lämpötila:4").
 * For derived params, only the param names are returned (i.e. "WD","WS,...).
 */
static vector<NA_Param> getParams(NFmiQueryInfo &info, bool &has_326, bool &has_19)
{
  vector<NA_Param> vec;

// 31-Oct-2011 PKi: Parameter mapping for virtual parameters handled now through virtual_params[]
//                  instead of coding parameterwise
#if 0
#ifdef SQD_CASCADE_Z_ENABLED
    bool has_Z= false, has_priZ = false, has_secZ = false;
#endif
#ifdef SQD_CASCADE_W_ENABLED  // 18-Oct-2011 PKi: Parameter mapping for W too
    bool has_W= false, has_priW = false, has_secW = false;
#endif
#endif
  // 31-Oct-2011 PKi: Virtual parameters to be added to the parameter set
  map<string, FmiParameterName> virtualParams;

  /*
   * Find out whether there are ':19' and ':326' combos in the data
   *
   * NOTE: SQD files are supposed to NOT have the same parameter as 'stand-alone' (i.e. "WD:20")
   *       and combo (":19"), in the same file. We cannot detect this situation by Newbase API,
   *       anyhow. Just trust it does not happen, or that the behaviour is then undetermined
   *       (either the combo or the stand-alone gets referred to, but we don't know, which one).
   */
  has_19 = info.Param(kFmiTotalWindMS);
  has_326 = info.Param(kFmiWeatherAndCloudiness);

  info.ResetParam();

  while (info.NextParam())
  {
    NFmiDataIdent id = info.Param();
    enum FmiParameterName e = (FmiParameterName)id.GetParamIdent();
    const char *latin1_name = id.GetParamName().CharPtr();

    bool show_id = true;

    switch (e)
    {
      case kFmiTotalWindMS:
      case kFmiWeatherAndCloudiness:
        continue;  // don't list the host params

      // WeatherAndCloudiness (326) derivatives
      //
      case kFmiTotalCloudCover:          // N
      case kFmiLowCloudCover:            // CL
      case kFmiMediumCloudCover:         // CM
      case kFmiHighCloudCover:           // CH
      case kFmiPrecipitation1h:          // RR
      case kFmiPrecipitationType:        // PRET
      case kFmiPrecipitationForm:        // PREF
      case kFmiFogIntensity:             // FOG
      case kFmiProbabilityThunderstorm:  // THUND
      case kFmiWeatherSymbol1:           // HSADE
      case kFmiWeatherSymbol3:           // HESSAA
      case kFmiMiddleAndLowCloudCover:   // MiddleAndLowCloudCover
        if (has_326) continue;
        break;

      case kFmiWindDirection:      // WD
      case kFmiWindSpeedMS:        // WS
      case kFmiHourlyMaximumGust:  // GUST
      case kFmiWindUMS:            // U
      case kFmiWindVMS:            // V
      case kFmiWindVectorMS:       // WVEC
        if (has_19) continue;
        break;

        // Note: There's yet more disturbance by the subtle rules of "how Newbase does it".
        //      Looking for param "Z" in Newbase matches either #2 ('kFmiGeopHeight') or #3
        //      ('kFmiGeomHeight'), in this order.
        //
        //      If we find either #2 or #3 we also add "Z" (with no id mentioned) to the list
        //      of available params.
        //
        //      TBD: Should this be in addition to the #2 and/or #3. Maybe. But if there is
        //          a param "Z:2" (or "Z:anyid") the q3 rules will use that instead of
        //          the #2->#3 cascade chain. Confusing. Unnecessarily complex. Newbase is.
        //          Over and over...  *siiiigh**
        //
        //          Thing should be kept Simple. Newbase fails to do that. Always. And again.
        //
        // 18-Oct-2011 PKi: If the data has #43 (primary) or #39 (secondary) adding "W:xxx"
        //                  (with generated id) to the list of available params.
        //                  Z has similar logic; "Z:xxx" is added if the data has #2 (primary) or
        //                  #3 (secondary).
        //
        // 31-Oct-2011 PKi: Parameter mapping for virtual parameters handled now through
        // virtual_params[]
        //                  instead of coding parameterwise
#if 0
#ifdef SQD_CASCADE_Z_ENABLED
//            case kFmiGeomHeight:
//                // If the file has 'Z:2' list only that. Otherwise, list a generic 'Z'.
//                //
//                if (strcmp( latin1_name, "Z" ) != 0) {
//                    has_Z= true;
//                }
//                break;
//            case kFmiGeopHeight:
//                has_Z= true;
//                break;

              case SQD_PRIMARY_Z:
                  has_Z= true;
                  has_priZ= (strcmp( latin1_name, "Z" ) == 0);
                  break;
              case SQD_SECONDARY_Z:
                  has_Z= true;
                  has_secZ= (strcmp( latin1_name, "Z" ) == 0);
                  break;
#endif

#ifdef SQD_CASCADE_W_ENABLED
              case SQD_PRIMARY_W:
                  has_W= true;
                  has_priW= (strcmp( latin1_name, "W" ) == 0);
                  break;
              case SQD_SECONDARY_W:
                  has_W= true;
                  has_secW= (strcmp( latin1_name, "W" ) == 0);
                  break;
#endif
#endif
      default:
        // 31-Oct-2011 PKi: Collect virtual parameters
        //
        SQD_Data::mapParameter(e, virtualParams);
        break;
    }

    // SQD parameter names are in Windows Latin-1 encoding (i.e. "Lämpötila").
    // Scripts see them in UTF-8.
    //
    string name = latin1_to_utf8(latin1_name);

    assert(e);
    NA_Param param(show_id ? e : (FmiParameterName)0,
                   name.c_str(),
                   SQD_Tools::unit_by_id(e),
                   interpolation_name(id.GetParam()->InterpolationMethod()),
                   id.GetParam()->Precision().CharPtr());

    vec.push_back(param);
  }

// 31-Oct-2011 PKi: Add the collected virtual parameters to the parameter set
//
#if 0
#ifdef SQD_CASCADE_Z_ENABLED
    if ( has_Z /* && (! has_priZ) && (! has_secZ) */ ) {
        vec.push_back( NA_Param( "Z", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE ) );
    }
#endif
#ifdef SQD_CASCADE_W_ENABLED
    if ( has_W /* && (! has_priW) && (! has_secW) */ ) {
        vec.push_back( NA_Param( "W", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE ) );
    }
#endif
#endif
  for (map<string, FmiParameterName>::const_iterator it = virtualParams.begin();
       (it != virtualParams.end());
       it++)
    vec.push_back(NA_Param(it->first.c_str(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE));

  if (has_19)
  {
    addParam(vec, info, kFmiWindDirection);      // WD
    addParam(vec, info, kFmiWindSpeedMS);        // WS
    addParam(vec, info, kFmiHourlyMaximumGust);  // GUST
    addParam(vec, info, kFmiWindUMS);            // U
    addParam(vec, info, kFmiWindVMS);            // V
    addParam(vec, info, kFmiWindVectorMS);       // WVEC
  }

  if (has_326)
  {
    addParam(vec, info, kFmiTotalCloudCover);          // N
    addParam(vec, info, kFmiLowCloudCover);            // CL
    addParam(vec, info, kFmiMediumCloudCover);         // CM
    addParam(vec, info, kFmiHighCloudCover);           // CH
    addParam(vec, info, kFmiPrecipitation1h);          // RR
    addParam(vec, info, kFmiPrecipitationType);        // PRET
    addParam(vec, info, kFmiPrecipitationForm);        // PREF
    addParam(vec, info, kFmiFogIntensity);             // FOG
    addParam(vec, info, kFmiProbabilityThunderstorm);  // THUND
    addParam(vec, info, kFmiWeatherSymbol1);           // HSADE
    addParam(vec, info, kFmiWeatherSymbol3);           // HESSAA
    addParam(vec, info, kFmiMiddleAndLowCloudCover);   // MiddleAndLowCloudCover
  }

  return vec;
}

/*---=== SQD_Data ===---*/

/*
 */
SQD_Data::SQD_Data(const char *fn, bool relative_uv) throw(E_BAD_FILE, std::runtime_error)
    : NA_Data(read_info(fn)),
      qd(new NFmiQueryData(fn))
#ifdef METQU
      ,
      is_readonly(true)
#endif
{
  setExtra_sqd_RelativeUV(relative_uv);
  INVARIANT();
}

#ifdef METQU
SQD_Data::SQD_Data(const NA_Info &info) throw()
    : NA_Data(info),
      qd(new_qd(info, info.getExtra_sqd_gridsize())),
      is_readonly(false)  // read-write
{
  INVARIANT();
}
#endif

/*
 */
SQD_Data::~SQD_Data()
{
  INVARIANT();

  delete qd;
}

/*
 * Prepare an SQD in-memory data with non-initialized values.
 */
#ifdef METQU
NFmiQueryData *SQD_Data::new_qd(const NA_Info &info, const MatrixPos &gs) throw(E_USAGE)
{
  int prod_num = info.getExtra_sqd_producer();  // 0 if none

  bool host_326 = info.getExtra_sqd_combo326();
  bool host_19 = info.getExtra_sqd_combo19();

  //---
  // Times
  //
  NFmiTimeList tlist;
  const vector<JDay> &times = info.getTimes();

  for (vector<JDay>::const_iterator it = times.begin(); it != times.end(); ++it)
  {
    NFmiMetTime mt = SQD_Tools::jd2mt(*it);

    // NOTE: '.Add()' insists in taking a pointer and it REALLY NEEDS TO BE A NEW
    //      ALLOCATED POINTER. Otherwise 'NFmiTimeList' goes BOOM later on.
    //      Marko's own code shows using &' (address-of) here - that is NOT safe
    //      (at least not here).    --AKa 11-Sep-2009
    //
    tlist.Add(new NFmiMetTime(mt), false /*no duplicates*/, false /*insert in time order*/);
  }

  //---
  // Levels
  //
  NFmiLevelBag lbag;
  const vector<NA_Level> &levels = info.getLevels();

  if (!levels.front().isGroundLevel())
  {
    for (vector<NA_Level>::const_iterator it = levels.begin(); it != levels.end(); ++it)
    {
      lbag.AddLevel(SQD_Tools::newbase_level(*it));
    }
  }

  //---
  // Producer
  //
  // We don't use producer in the q3/metqu data model, but Newbase needs it for creating
  // an SQD file.
  //
#if 0
    if (!prod_num) {
        throw E_LOG_USAGE0( "No producer provided" );
    }
#endif

  //---
  // Params
  //
  NFmiParamBag pbag;

  // Go through params and collect the ones really taking space on disk (i.e. skip
  // those derived from ':19' or ':326').
  //
  for (vector<NA_Param>::const_iterator it = info.getParamsBegin(); it != info.getParamsEnd(); ++it)
  {
    if (host_326 && derived_from(326, *it))
    {
      continue;
    }
    if (host_19 && derived_from(19, *it))
    {
      continue;
    }

    // 'NFmiParam' needs to be also given the name of the parameter, explicitly.
    // Each time.
    //
    // explicit NFmiParam(unsigned long theIdent,
    // 	 const NFmiString &theName = "Koiranpentu",
    // 	 double theMinValue = kFloatMissing,
    // 	 double theMaxValue = kFloatMissing,
    // 	 float theScale = kFloatMissing,
    // 	 float theBase = kFloatMissing,
    // 	 const NFmiString itsPrecision = "%.1f",
    // 	 FmiInterpolationMethod theInterpolationMethod= kNearestPoint);

    // 'USE_NEWBASE' define makes it sure that native name is always there.
    //
    string name_part_utf8;
    FmiParameterName e;

    if (!SQD_Tools::cut_at_colon(it->getNativeName_().c_str(), name_part_utf8, e))
    {
      throw E_LOG_BUG0("Bad 'NA_Param'");  // shouldn't happen
    }

    string latin1_name = utf8_to_latin1(name_part_utf8.c_str());  // without the colon and id

    /*
     * Note: The interpolation method stored matters only for SQD format (and if the
     *       file is used outside of q3). q3 now uses its own interpolation code and
     *       is not dependent on these values.
     *
     * Note: INTERPOLATE_LINEAR_DEG params (s.a. 'WD') are stored with 'kNearestPoint'
     *       since there is no interpolation method enum in Newbase for degree values.
     *       This is a safe choise. Newbase may still do interpolations differently
     *       for such entries - we don't know (or care).   -AKa 12-Oct-2010
     */
    FmiInterpolationMethod method;

    switch (it->getMethod())
    {
      case NA_Param::INTERPOLATE_UNKNOWN:
        method = kNoneInterpolation;
        break;
      case NA_Param::INTERPOLATE_NEAREST:
      case NA_Param::INTERPOLATE_LINEAR_DEG:
        method = kNearestPoint;
        break;
      case NA_Param::INTERPOLATE_LINEAR:
        method = kLinearly;
        break;
      default:
        throw E_LOG_BUG0("Unexpected interpolation type");
    }

    pbag.Add(NFmiParam(e,
                       latin1_name.c_str(),
                       kFloatMissing,  // minvalue
                       kFloatMissing,  // maxvalue
                       kFloatMissing,  // scale
                       kFloatMissing,  // base
                       "%.1f",  // written to SQD header so we better go as default constructor does
                       method));
  }

  // Newbase requires this to be dynamically allocated (we think...)  AKa 4-Oct-10
  //
  const NFmiProducer *nprod = new NFmiProducer(prod_num);
  assert(nprod);

  // Newbase has separate static functions for creating the 'kFmiTotalWindMS' and
  // 'kFmiWeatherAndCloudiness' mega parameters.
  //
  // Note: 'CreateParam()' should be a static function (but is not).
  //
  // Note: These two won't need to be given a name, since they are never directly
  //      used by scripts, anyways.
  //
  if (host_326)
  {
    NFmiWeatherAndCloudiness wc;
    NFmiDataIdent *id = wc.CreateParam(*nprod, nullptr);  // for us to 'delete'
    pbag.Add(*id, true /*prevent duplicates (not really needed)*/);
    delete id;
  }

  if (host_19)
  {
    NFmiTotalWind tw;
    NFmiDataIdent *id = tw.CreateParam(*nprod, nullptr);  // for us to 'delete'
    pbag.Add(*id, true /*prevent duplicates (not really needed)*/);
    delete id;
  }

  pbag.SetProducer(*nprod);

  //---
  // Projection and gridsize (SQD file has same gridsize throughout the file)
  //
  auto ap(NFmiAreaFactory::Create(info.getProjection().toString().c_str()));
  NFmiGrid grid(ap.get(), gs.getX(), gs.getY());

  NFmiParamDescriptor p_desc(pbag);
  NFmiTimeDescriptor t_desc(SQD_Tools::jd2mt(info.getOriginTime()), tlist);
  NFmiHPlaceDescriptor h_desc(grid);
  NFmiVPlaceDescriptor v_desc(lbag);

  NFmiQueryInfo ninfo(p_desc, t_desc, h_desc, v_desc);

  // Need to use 'CreateEmptyData()' to create (new) the object and also initialize it
  // (standard constructor does not initialize).      -- AKa 9-Sep-2009 (based on email with Marko)
  //
  // 'Create' means we are (our caller is) in charge of deleting 'data'
  //
  NFmiQueryData *qd = NFmiQueryDataUtil::CreateEmptyData(ninfo);
  if (!qd)
  {
    throw E_LOG_BUG0("Unable to create NFmiQueryData");
  }

  // Note: We're NOT expecting anything from the 'CreateEmptyData()' though in practise it does
  //       set the matrix to all 32700.0.

  return qd;  // to be deleted by caller
}
#endif
// METQU

/*
 */
#ifdef METQU
void SQD_Data::output(ostream &out, ProgressCallback *cb) const /*throw(E_FATAL)*/
{
  assert(qd);
  (void)cb;  // Not supported by Newbase

  try
  {
    out << *qd;
  }
  catch (exception &e)
  {
    const char *what = e.what();
    throw E_LOG_FATAL("Writing SQD failed: %s", what ? what : "(unknown C++ exception)");
  }
}
#endif

/*
 */
NA_Info SQD_Data::read_info(const char *fn) throw(E_BAD_FILE)
{
  NFmiQueryInfo info;

  // Seems this can ignite:
  //  "NFmiQueryInfo::Read : Input stream does not contain querydata"
  //
  try
  {
    info = NFmiQueryInfo(fn);
  }
  catch (const std::runtime_error &e)
  {
    throw E_LOG_BAD_FILE(fn, e.what());
  }

  bool revlevorder = false;
  string nativeLevelType;
  // getTimes() and getLevels() can throw (E_BUG, E_LOG_BAD_FILE)
  vector<JDay> times;
  vector<NA_Level> levels;
  try
  {
    times = ::getTimes(info);
    levels = ::getLevels(info, fn, revlevorder, nativeLevelType);
  }
  catch (const std::exception &e)
  {
    throw E_LOG_BAD_FILE(fn, e.what());
  }

  bool has_326, has_19;
  vector<NA_Param> params = ::getParams(info, has_326, has_19);

  if (times.size() == 0)
  {
    throw E_LOG_BAD_FILE(fn, "no times");
  }
  if (params.size() == 0)
  {
    throw E_LOG_BAD_FILE(fn, "no params");
  }
  assert(levels.size() > 0);  // '::getLevels()' threw the exceptions

  const NFmiGrid *grid = info.Grid();
  const NFmiArea *area = nullptr;

  // 07-Apr-2015 PKi: Handling point data (observations) too
  //
  if (grid)
  {
    area = grid->Area();  // This has lifespan tied to the 'NFmiGrid' object
    assert(area);
  }

  // 'pr' initialized like this since we *know* the projection is 'SQD_Projection'
  // (avoids going through any other parsing)
  //
  // 22-Aug-2011 MPi/PKi: Use grid's area to create projection; if using creation string the
  //						projection may not exactly be the same (floating
  //point accuracy problem)
  //

  const Projection pr(grid ? Projection(area->AreaStr().c_str(), SQD_Projection(area))
                           : Projection(nullptr));

  struct stat attrib;
  stat(fn, &attrib);

  NA_Info info2(time(nullptr),
                attrib.st_mtime,
                SQD_Tools::mt2jd(info.OriginTime()),
                times,
                nativeLevelType,
                levels,
                params,
                pr,
                area ? area->WKT() : "",
                getDataTimeStep(info),
                area ? area->WorldXYWidth() / grid->XNumber() / 1000.0 : 0,
                area ? area->WorldXYHeight() / grid->YNumber() / 1000.0 : 0,
                grid ? true : false);

  unsigned prod = info.Producer()->GetIdent();
  MatrixPos gs(grid ? grid->XNumber() : 0, grid ? grid->YNumber() : 0);

  info2.setExtra_fn(fn)
      .setExtra_sqd_producer(prod)
      .setExtra_sqd_gridsize(gs)
      .setExtra_sqd_combo19(has_19)
      .setExtra_sqd_combo326(has_326)
      .setExtra_sqd_revlevorder(revlevorder);  // 31-Aug-2011 PKi: Flag for original level order

  return info2;
}

/*
 * Get the Newbase param id, for accessing the particular SQD data.
 *
 *   - If only an id is given, use that.
 *   - If id and native name are given, use them.
 *   - If only native name is given, use it.
 *   - If only standard name is given, try converting it to SQD id. If not
 *     a standard id, look for params with same (native) name in the data.
 */
#if 0
FmiParameterName SQD_Data::param_id_( const NA_Param &p ) const {

    FmiParameterName p_id= (FmiParameterName) p.getNativeId();
    string p_name= p.getNativeName();
    
    FmiParameterName e2;

    if (p_id) {
        if (p_name=="") {
            return p_id;    // use the id as such
        } else {
            // Does the file have a parameter with this name - do the id's match?
            //
            e2= SQD_Tools::newbase_id( p_name.c_str(), this );
            
            return (e2==p_id) ? e2 : (FmiParameterName) 0;
        }
    }

    if (p_name!="") {
        e2= SQD_Tools::newbase_id( p_name.c_str(), this );
    
        if (e2) {
            return e2;      // id by the native name
        }
        
        // Go on trying with the standard name
    }

    // Try a standard parameter match first (i.e. any "T" -> SQD id 4)
    //
    string std_name= p.getStandardName();

    FmiParameterName e= NA_Param::standard_param_native_id( std_name.c_str() );
    if (e) { 
        return e;

    } else {
        // Any native name in the data with that name?  (This is actually a very common case,
        // looking for parameter "xxx" from the script brings here)
        //
        e2= SQD_Tools::newbase_id( std_name.c_str(), this );
        return e2;
    }
}
#endif

/*
 */
/*virtual*/ bool SQD_Data::providesPressureLevelsFromHybrid() const
{
  // Pressure levels available (calculated from hybrid levels) if data has param 1 (kFmiPressure).
  //
  NFmiFastQueryInfo fi(qd);
  return fi.PressureValueAvailable();
}

/*
 * SQD data has (for version 7 at least) same grid size throughout.
 */
MatrixPos SQD_Data::getGridSize(size_t nPoints) const
{
  const NFmiGrid *grid = qd->Info()->Grid();

  if (grid) return MatrixPos(grid->XNumber(), grid->YNumber());

  // 07-Apr-2015 PKi: Use number of given data points for point data. If projection (bounding box)
  //					was given instead, the matrix will later be resized to the number
  //of data points 					within the given area
  //
  return MatrixPos(
      qd->Info()->SizeLevels() * ((nPoints > 0) ? nPoints : qd->Info()->SizeLocations()), 1);
}

/*
 * Push a certain 'Matrix' value on Lua stack (or use regular 'new' operator if 'L'==0).
 *
 * Returns: pointer to the pushed object; tied to Lua GC if 'L'!=0. To be deleted by the caller if
 * 'L'==0. nullptr if the requested parameter is not available
 */
/*virtual*/ CONST_IF_SERVER Matrix *SQD_Data::push_NativeMatrix(
    lua_State *L,
    const JDay &vt,
    const NA_Level &lev,
    const NA_Param &p
    //
    // 22-Aug-2011 PKi: For fetching data using newbase interpolation, projection and scaling
    //
    ,
    const Projection *target_proj,
    const MatrixPos *target_gs,
    bool *target_ready,
    const DataIdList *dataIds) CONST_IF_SERVER throw(/*E_BUG*/)
{
  CONST_IF_SERVER Matrix *m;

  //---
  // Special case: if looking for parameter 'Z' (with no id) look first at ':2' (kFmiGeopHeight) and
  // then
  // ':3' (kFmiGeomHeight) transparently to the user. Newbase does similar.
  //
  // Note: Currently this processing is ONLY within the 'SQD_Data' adapter, thus not being applied
  // to
  //      i.e. MQD data.
  //
  // Note: Vili was surprised to hear of such. It seems not to be a generally known approach, and is
  //      potentially risky. We'll place this in the configuration file for now (disabled by
  //      default).
  //
  // Note: Parameters ':2' and ':3' actually have different units ("gpm" and "m"). This further
  // complicates
  //      things if we allow the silent cascade.
  //
  // 18-Oct-2011 PKi:
  //   Another special case: if looking for parameter 'W' (with no id) look first at
  //   ':43' (kFmiVerticalVelocityMMS) and then :39 (kFmiVelocityPotential).
  //
  //   The primary/secondary parameter selection for mapped parameters (currently Z and W)
  //   is now handled by mapParameter()
  //
  //#ifdef SQD_CASCADE_Z_ENABLED
  //    if (p.toString() == "Z") {
  //        m= push_NativeMatrix_e( L, vt, lev, kFmiGeopHeight, target_proj, target_gs, target_ready
  //        );     // id 2 if (!m) {
  //            m= push_NativeMatrix_e( L, vt, lev, kFmiGeomHeight, target_proj, target_gs,
  //            target_ready );     // id 3
  //        }
  //    } else
  //#endif
  //    {
  //        m= push_NativeMatrix_e( L, vt, lev, SQD_Tools::param_id(p), target_proj, target_gs,
  //        target_ready );
  //    }

  bool mapped = false;

  // Try p or the primary parameter if available
  m = push_NativeMatrix_e(
      L, vt, lev, mapParameter(p, true, &mapped), target_proj, target_gs, target_ready, dataIds);

  if ((!m) && mapped)
    // Try the secondary parameter
    m = push_NativeMatrix_e(
        L, vt, lev, mapParameter(p, false), target_proj, target_gs, target_ready, dataIds);

  return m;
}

/*
 * Get data for given data point id's or data points within given area. The data is returned as
 * [nPoints,1] matrix
 */
void pointValues(NFmiFastQueryInfo &info,
                 const NFmiAreaFactory::return_type area,
                 const DataIdList *dataIds,
                 bool retMissing,
                 MemMatrix &m,
                 bool soundingData)
{
  MatrixPos::offset_t n = 0;

  // With sounding data, return missing values for levels having missing pressure value;
  // read pressures first

  std::vector<float> pressures;
  auto queryParam = info.Param();
  bool getSoundingPressureValues =
      ((!retMissing) && soundingData && (queryParam.GetParamIdent() == kFmiPressure));

  if ((!retMissing) && soundingData)
  {
    if (!getSoundingPressureValues) info.Param(kFmiPressure);

    if (dataIds)
    {
      for (info.ResetLevel(); info.NextLevel();)
      {
        for (DataIdList::const_iterator it = dataIds->begin(); (it != dataIds->end()); it++)
        {
          float v = kFloatMissing;

          if (info.Location(*it)) v = info.FloatValue();

          if (getSoundingPressureValues)
            m[n++] = v;
          else
            pressures.push_back(v);
        }
      }
    }
    else if (area)
    {
      for (info.ResetLevel(); info.NextLevel();)
      {
        for (info.ResetLocation(); info.NextLocation();)
        {
          if (area->IsInside(info.LatLon()))
          {
            float v = info.FloatValue();

            if (getSoundingPressureValues)
              m[n++] = v;
            else
              pressures.push_back(v);
          }
        }
      }
    }

    if (!getSoundingPressureValues) info.Param(queryParam);
  }

  if (!getSoundingPressureValues)
  {
    auto itPressure = pressures.begin();
    n = 0;

    if (dataIds)
    {
      for (info.ResetLevel(); info.NextLevel();)
      {
        for (DataIdList::const_iterator it = dataIds->begin(); (it != dataIds->end()); it++)
        {
          float v = kFloatMissing;

          if ((!retMissing) && ((!soundingData) || (*itPressure != kFloatMissing)) &&
              info.Location(*it))
            v = info.FloatValue();

          m[n++] = ((v == kFloatMissing) ? NAN : v);

          if ((!retMissing) && soundingData) itPressure++;
        }
      }
    }
    else if (area)
    {
      for (info.ResetLevel(); info.NextLevel();)
      {
        for (info.ResetLocation(); info.NextLocation();)
        {
          if (area->IsInside(info.LatLon()))
          {
            float v = kFloatMissing;

            if ((!retMissing) && ((!soundingData) || (*itPressure != kFloatMissing)))
              v = info.FloatValue();

            m[n++] = ((v == kFloatMissing) ? NAN : v);

            if ((!retMissing) && soundingData) itPressure++;
          }
        }
      }
    }
  }

  if (n == 0) m[n++] = NAN;

  m.set_size(MatrixPos(n, 1));
}

/*
 * TBD:  Current handling is inefficient. Scripts regularily ask for say 50,60 grid of interpolated
 * values. We should calculate such right on, without pushing native resolution interpolation (s.a.
 * 500,600).
 */
CONST_IF_SERVER
Matrix *SQD_Data::push_NativeMatrix_e(
    lua_State *L,
    const JDay &vt,
    const NA_Level &lev,
    FmiParameterName e
    //
    // 22-Aug-2011 PKi: For fetching data using newbase interpolation, projection and scaling
    //
    ,
    const Projection *target_proj,
    const MatrixPos *target_gs,
    bool *target_ready,
    const DataIdList *dataIds_) CONST_IF_SERVER throw(/*E_BUG*/)
{
  // 07-Apr-2015 PKi: Handling point data (e.g. observations) too
  //
  bool pointData = (qd->Info()->Grid() == nullptr);
  const DataIdList *dataIds = ((dataIds_ && (dataIds_->size() > 0)) ? dataIds_ : nullptr);

  const MatrixPos native_gs = getGridSize((pointData && dataIds) ? dataIds->size() : 0);
  const Projection &native_proj = getProjection();

  const vector<JDay> &times_ = getTimes();
  const vector<NA_Level> &levels_ = getLevels();

  bool exact_time = find_it<JDay>(times_, vt) != times_.end();

  NA_Level::Type lt = lev.getType();
  bool exact_level =
      ((lt != NA_Level::HEIGHT_LEVEL) && ((!lev) || lev.isGroundLevel() || lev.isSoundingLevel() ||
                                          (find_it<NA_Level>(levels_, lev) != levels_.end())));

  if (target_gs && (*target_gs == MatrixPos::ZERO)) target_gs = nullptr;

  bool same_proj = ((!target_proj) || (*target_proj == native_proj));
  bool same_gs = (pointData || (!target_gs) || (*target_gs == native_gs));

  // 16-Apr-2015 PKi: Store origintime of the first used querydata as a lua global; it will be
  // returned in response headers
  //
  lua_getglobal(L, "first_origintime");
  string_or_null fstot = lua_tostring(L, -1);
  lua_pop(L, 1);

  if ((!fstot.c_str()) || (fstot == ""))
  {
    NFmiString otstr(qd->Info()->OriginTime().ToStr(kYYYYMMDDHH));

    lua_pushstring(L, otstr.CharPtr());
    lua_setglobal(L, "first_origintime");
  }

  bool relative_uv = getExtra_sqd_RelativeUV();

  //---
  // Direct connection to data - no interpolations needed (writable in command line mode)
  //
  if (exact_time && exact_level && same_proj && same_gs && ((!pointData) || (!dataIds)))
  {
    try
    {
      // Push a read-only proxy (for read matrices, caller does a copy most likely anyways,
      // to a MemMatrix if scaling and/or projection is applied).
      //
      SQD_Matrix *m = L ? new (L) SQD_Matrix(this, vt, lev, e) : ::new SQD_Matrix(this, vt, lev, e);

#ifdef USE_NEWBASE_PROJ
      // 22-Aug-2011 PKi: No further processing (interpolation, projection or scaling) to be done

      if (target_ready) *target_ready = true;
#endif

      // Attach grid; needed by PEEKXY
      //
      std::shared_ptr<NFmiGrid> wantedGrid(
          new NFmiGrid(qd->Info()->Area(), m->getSize().getXS(), m->getSize().getYS()));
      m->setGrid(wantedGrid);

      return m;
    }
    catch (const E_NO_MATCH &)
    {
      if (L)
      {
        // Didn't get created (but Lua did allocate the object AND attach its GC
        // to the C++ destructor. Nuke the object before it gets to GC.
        //
        LuaNew_base::nuke(L, -1);  // removes the link from Lua GC to C++ destructor
      }
      return nullptr;  // no such param
    }
  }

  //---
  // Time and/or level needs interpolation.
  //
  NFmiFastQueryInfo fi(qd);
#if 1
  fi.First();  // Just in case (faint memory this would be needed?) -- AKa 24-Mar-10
#endif

  // Set to the param
  //
  // Note: We may need to do this later on, within the loops, to keep the right param selected
  //       but it's good to get out here early if the param won't even be available.
  //
  if (!fi.Param(e))
  {
    return nullptr;
  }

  NFmiMetTime mt = SQD_Tools::jd2mt(vt);

  if (exact_time)
  {
    bool tmp = fi.Time(mt);
    L_ASSERT(tmp);
  }
  else
  {
    // Interpolate time (should be within time range if got here)
  }

  if (exact_level)
  {
    if ((!lev) || lev.isGroundLevel() || lev.isSoundingLevel())
    {
      fi.FirstLevel();
    }
    else
    {
      fi.Level(SQD_Tools::newbase_level(lev));
    }
  }
  else
  {
    // Interpolate level (should be within level range if got here)
  }

  double lv = lev.getValue();

  NA_Param::Unit unit = SQD_Tools::unit_by_id(e);

  // 22-Aug-2011 PKi: Fetch data using newbase interpolation, projection and scaling if requested
  //
  // 07-Apr-2015 PKi: If interpolation would be needed for point data, return missing values.
  //
  bool retMissing = pointData && (!(exact_time && exact_level));

  MemMatrix *m_ =
      retMissing ? L ? new (L) MemMatrix(
                           native_gs, NAN, lev, e, unit, target_proj ? *target_proj : native_proj)
                     : ::new MemMatrix(
                           native_gs, NAN, lev, e, unit, target_proj ? *target_proj : native_proj)
                 : L ? new (L) MemMatrix((!pointData) && target_gs ? *target_gs : native_gs,
                                         lev,
                                         e,
                                         unit,
                                         target_proj ? *target_proj : native_proj)
                     : ::new MemMatrix((!pointData) && target_gs ? *target_gs : native_gs,
                                       lev,
                                       e,
                                       unit,
                                       target_proj ? *target_proj : native_proj);

  NFmiDataMatrix<float> nm;
  const Projection *pr = nullptr;
  NFmiArea *area = nullptr;

  if (!(same_proj && same_gs))
  {
    // Nonnative target projection and/or gridsize; if native projection, clone grid's area.
    //
    // Set pr to projection to be used if area is created using NFmiAreaFactory (when nonnative
    // projection or getting/cloning area should fail)

    pr = same_proj ? &native_proj : target_proj;

    if (same_proj)
    {
      const NFmiGrid *grid = fi.Grid();
      if ((area = (grid ? grid->Area() : nullptr))) area = area->Clone();
    }
  }

  auto areaPtr = ((!(same_proj && same_gs)) && (!area)) ? NFmiAreaFactory::Create(pr->toString())
                                                        : (NFmiAreaFactory::return_type)area;

  if (pointData)
  {
    // 07-Apr-2015 PKi: Return point data within the given area
    //
    pointValues(
        fi, areaPtr, dataIds, retMissing, *m_, levels_[0].getType() == NA_Level::SOUNDING_LEVEL);
  }
  else
  {
    // Attach grid; needed by PEEKXY
    //
    if (!areaPtr.get()) areaPtr = NFmiAreaFactory::Create(native_proj.toString());

    std::shared_ptr<NFmiGrid> wantedGrid(
        new NFmiGrid(areaPtr.get(), m_->getSize().getXS(), m_->getSize().getYS()));
    m_->setGrid(wantedGrid);

    // Interpolate by time and/or level alone
    //
    if (exact_level)
    {
      // We know the time needs to be interpolated (and *is* interpolatable)

#if 1
      /* The 'NFmiDataMatrix' use causes unnecessary copying of values.
       */
      // NFmiDataMatrix<float> nm;

      if (!(same_proj && same_gs))
      {
        //			NFmiGrid wantedGrid(areaPtr.get(), m_->getSize().getXS(),
        // m_->getSize().getYS());
        fi.GridValues(nm, *(wantedGrid.get()), mt, relative_uv);
      }
      else
        fi.Values(nm, mt);

        // LOG_DEBUG( "NMatrix size: %d", (int)(nm.NX() * nm.NY()) );
        // LOG_DEBUG( "Grid size: %d", (int)native_gs.getN() );

        // matrix_fill( *m_, nm );
#else
      /* This did not give right results
       */
      unsigned xs = fi.GridXNumber();
      unsigned ys = fi.GridYNumber();

      for (unsigned y = 0; y < ys; ++y)
      {
        for (unsigned x = 0; x < xs; ++x)
        {
          float v = fi.PeekLocationValue(x, y, mt);
          if (v == kFloatMissing)
          {
            v = NAN;
          }

          MatrixPos xy(x, y);
          m_->set_value(xy, v);
        }
      }
#endif
    }
    else if (lt == NA_Level::PRESSURE_LEVEL)
    {
#ifdef USE_NEWBASE_PROJ

      if (!(same_proj && same_gs))
      {
        //			NFmiGrid wantedGrid(areaPtr.get(), m_->getSize().getXS(),
        // m_->getSize().getYS());
        fi.PressureValues(nm, *(wantedGrid.get()), mt, lv, relative_uv);
      }
      else
        fi.PressureValues(nm, mt, lv);

#else

      if (!(fi.PressureValueAvailable()  // Data has pressure parameter (useful for pressure level
                                         // calculation)
            || fi.PressureLevelDataAvailable()  // Data has pressure levels as such (can be
                                                // interpolated from)
            ))
      {
        throw E_LOG_BUG0("Data does not support .PressureLevelValue()");
      }

      // No need to route data via 'NFmiDataMatrix'. This also solves the crashing issue
      // with '::PressureValues()'.
      //
#if (!defined(NDEBUG)) || (!defined(SQD_AND_Q3_INDEXING_ARE_THE_SAME))
      unsigned xs = fi.GridXNumber();
#endif
      unsigned n;

      // This seems to matter only if we're right on time (see comments below)
      //
      if (exact_time)
      {
        fi.Time(mt);
      }

      for (fi.ResetLocation(); fi.NextLocation();)
      {
        n = fi.LocationIndex();

        // It seems '.PressureLevelValue()' does not work with interpolated times. It gives us NAN
        // whenever 'mt' is not an exact time in the data.

        // Note: Seems the 'fi.Time()' setting above does not cause time interpolation for
        // '::PressureLevelValue()'.
        //      There's another API that takes a time argument.
        //
        float v = exact_time ? fi.PressureLevelValue(lv) : fi.PressureLevelValue(lv, mt);

        if (v == kFloatMissing)
        {
          v = NAN;
        }

#ifdef SQD_AND_Q3_INDEXING_ARE_THE_SAME
        (*m_)[n] = v;
#else
        MatrixPos xy(n % xs, n / xs);
        m_->set_value(xy, v);
#endif

        // There's a bug in Newbase '::PressureLevelValue()'. If it does not find the right pressure
        // level, it changes the current parameter to P (pressure). If we got NAN, we must reset the
        // param to what we wanted. (No way for us to know if the NAN was because of the "give up"
        // condition or because of genuinly missing data only on that particular position).  --AKa
        // 21-Dec-2010
        //
#ifdef PRESSURELEVELVALUE_HAS_SIDE_EFFECTS
        if (isnanf(v))
        {
          fi.Param(e);
        }
#endif
      }

#ifndef NDEBUG
      unsigned ys = fi.GridYNumber();
      assert(n == xs * ys - 1);
#endif

#endif  // ifdef USE_NEWBASE_PROJ
    }
    else if (lt == NA_Level::HEIGHT_LEVEL)
    {
      // 23-Nov-2012 PKi: Height value query support
      //

      if (!(same_proj && same_gs))
      {
        //			NFmiGrid wantedGrid(areaPtr.get(), m_->getSize().getXS(),
        // m_->getSize().getYS());
        fi.HeightValues(nm, *(wantedGrid.get()), mt, lv, relative_uv);
      }
      else
        fi.HeightValues(nm, mt, lv);
    }
    else
    {
      throw E_LOG_BUG("Unexpected level type: %d", (int)lev.getType());
    }
  }

#if 1
  // Ensure we have interpolation info
  //
  assert(m_->getUnit().getMethod());
#endif

#ifdef USE_NEWBASE_PROJ
  if (!pointData) matrix_fill(*m_, nm);

  if (target_ready) *target_ready = true;
#endif

  //  m_->set_readonly();   // Make the matrix read-only
  return m_;
}

/*
 * 26-Sep-2011 PKi: Returns JDay as gm time_t or 0 if conversion is not possible.
 */

static time_t JDay2time_t(JDay const &my)
{
  unsigned year = my.year();
  if (year < 1970)
  {
    return 0;
  }

  struct tm tmp;
  //
  memset(&tmp, 0, sizeof(tmp));
  tmp.tm_year = my.year() - 1900;
  tmp.tm_mon = my.month() - 1;
  tmp.tm_mday = my.day();
  tmp.tm_hour = my.hour();
  tmp.tm_min = my.min();
  tmp.tm_sec = my.sec();

  return timegm(&tmp);
}

/*
 * 23-Sep-2011 PKi: Calculates time_t difference of 2 JDays. Returns false if the times are not
 *                  valid as time_t values or the difference is not what expected (times are
 *                  taken as subsequent elements of a constant step time serie)
 */

static bool timeDiff(JDay const &j1, JDay const &j2, time_t &tDiff, time_t &t1_, unsigned int tStep)
{
  // Times should be nonzero and in rising order. Use t1_ (the converted value of j2 in previous
  // call) if it is available

  time_t t1 = (t1_ ? t1_ : JDay2time_t(j1)), t2 = JDay2time_t(j2), tD = tDiff;

  if ((!t1) || (!t2) || (t1 >= t2))
    tDiff = 0;
  else
    tDiff = t2 - t1;

  t1_ = t2;  // t1 (= j1) for next call

  // Timestep should be constant nonzero and equal to or multiple of tStep

  return (tDiff && ((!tStep) || (!(tDiff % tStep))) && ((!tD) || (tDiff == tD)));
}

/*
 * 23-Sep-2011 PKi: Fetch cross using newbase. Uses native projection and gridsize
 */

const unsigned int NewBaseMinTimeStepInSecs = 60;

/*virtual*/ CONST_IF_SERVER Matrix *SQD_Data::push_NativeCross(
    lua_State *L,
    const std::vector<JDay> &vtVec,
    const std::vector<NA_Level> &levelVec,
    const LatLonList &locs,
    const NA_Param &p,
    bool flightRoute) const
{
  NFmiFastQueryInfo fi(qd);
#if 1
  fi.First();  // Just in case (faint memory this would be needed?) -- AKa 24-Mar-10
#endif

  // Set to the param
  //
  // 18-Oct-2011 PKi:
  //
  //   Special case: if looking for parameter 'Z' (with no id) look first at ':2' (kFmiGeopHeight)
  //   and then
  //   ':3' (kFmiGeomHeight) transparently to the user. Newbase does similar.
  //
  //   Note: Parameters ':2' and ':3' actually have different units ("gpm" and "m"). This further
  //   complicates
  //        things if we allow the silent cascade.
  //
  //   Another special case: if looking for parameter 'W' (with no id) look first at
  //   ':43' (kFmiVerticalVelocityMMS) and then :39 (kFmiVelocityPotential).
  //

  FmiParameterName e;
  bool mapped = false;

  bool setOk =
      fi.Param(e = mapParameter(p, true, &mapped));  // Try p or the primary parameter if available
  if (!setOk)
  {
    if (mapped) setOk = fi.Param(e = mapParameter(p, false));  // Try the secondary parameter

    if (!setOk) return nullptr;
  }

  // Convert call parameters and get the data
  //
  // When one location and multiple times with constant step, or multiple locations and one time,
  // TimeCrossSectionValues or CrossSectionValues are used respectively instead of
  // RouteCrossSectionValues

  NFmiDataMatrix<float> nm;

  std::vector<float> pressures_or_heights;
  std::vector<float> &pressures = pressures_or_heights, &heights = pressures_or_heights;
  std::vector<NFmiLevel> hLevels;
  unsigned int nLevels = levelVec.size(), nLocs = locs.size(), nTimes = vtVec.size(), i;
  time_t t1 = 0, tDiff = 0;
  bool timeCross = ((!flightRoute) && (nLocs == 1) && (nTimes > 1) &&
                    timeDiff(vtVec[0], vtVec[1], tDiff, t1, NewBaseMinTimeStepInSecs));
  bool cross = ((!flightRoute) && (nTimes == 1));
  bool hybrid = (levelVec[0].getType() == NA_Level::HYBRID_LEVEL);
  bool height = (levelVec[0].getType() == NA_Level::HEIGHT_LEVEL);
  bool ground = ((nLevels == 1) && (levelVec[0].getType() == NA_Level::GROUND_LEVEL));

  // Ground level is handled as hybrid level
  //
  if (ground) hybrid = (!(ground = false));

  if (hybrid)
    for (i = 0; (i < nLevels); i++)
      hLevels.push_back(SQD_Tools::newbase_level(levelVec[i]));
  else
    for (i = 0; (i < nLevels); i++)
      pressures_or_heights.push_back(levelVec[i].getValue());

  if (timeCross)
  {
    // Check for constant timestep

    for (i = 2; (i < nTimes); i++)
      if (!timeDiff(vtVec[i - 1], vtVec[i], tDiff, t1, NewBaseMinTimeStepInSecs))
      {
        timeCross = false;
        break;
      }
  }

  if (timeCross)
  {
    NFmiTimeBag validTimes(SQD_Tools::jd2mt(vtVec[0]),
                           SQD_Tools::jd2mt(vtVec[nTimes - 1]),
                           tDiff / NewBaseMinTimeStepInSecs);

    //    	if (ground)
    //    		fi.TimeCrossSectionValuesGround( nm, NFmiPoint(locs[0].getLon(),
    //    locs[0].getLat()), validTimes ); 	else
    if (hybrid)
      fi.TimeCrossSectionValuesHybrid(
          nm, hLevels, NFmiPoint(locs[0].getLon(), locs[0].getLat()), validTimes);
    else if (height)
      fi.TimeCrossSectionValues(
          nm, heights, NFmiPoint(locs[0].getLon(), locs[0].getLat()), validTimes);
    else
      fi.TimeCrossSectionValuesLogP(
          nm, pressures, NFmiPoint(locs[0].getLon(), locs[0].getLat()), validTimes);
  }
  else
  {
    std::vector<NFmiMetTime> validTimes;
    std::vector<NFmiPoint> locations;

    if (!flightRoute)
    {
      int locIdxIncr = 1, iLocs;

      if (nLocs == 1)
      {
        // To clone the single location for Route...() call
        locIdxIncr = 0;
      }

      if (cross)
      {
        // To loop the locations
        nTimes = nLocs;
      }

      for (i = iLocs = 0; (i < nTimes); i++, iLocs += locIdxIncr)
      {
        if ((!i) || (!cross)) validTimes.push_back(SQD_Tools::jd2mt(vtVec[i]));
        locations.push_back(NFmiPoint(locs[iLocs].getLon(), locs[iLocs].getLat()));
      }

      if (cross)
        //        	if (ground)
        //            	fi.CrossSectionValuesGround( nm, validTimes[0], locations );
        //        	else
        if (hybrid)
          fi.CrossSectionValuesHybrid(nm, validTimes[0], hLevels, locations);
        else if (height)
          fi.CrossSectionValues(nm, validTimes[0], heights, locations);
        else
          fi.CrossSectionValuesLogP(nm, validTimes[0], pressures, locations);
      //      else if (ground)
      //      	fi.RouteCrossSectionValuesGround( nm, locations, validTimes );
      else if (hybrid)
        fi.RouteCrossSectionValuesHybrid(nm, hLevels, locations, validTimes);
      else if (height)
        fi.RouteCrossSectionValues(nm, heights, locations, validTimes);
      else
        fi.RouteCrossSectionValuesLogP(nm, pressures, locations, validTimes);
    }
    else
    {
      for (i = 0; (i < nTimes); i++)
        validTimes.push_back(SQD_Tools::jd2mt(vtVec[i]));
      for (i = 0; (i < nLocs); i++)
        locations.push_back(NFmiPoint(locs[i].getLon(), locs[i].getLat()));

      if (hybrid)
        fi.FlightRouteValuesHybrid(nm, hLevels, locations, validTimes);
      else if (height)
        fi.FlightRouteValues(nm, heights, locations, validTimes);
      else
        fi.FlightRouteValuesLogP(nm, pressures, locations, validTimes);

      if (nm.NX() == 0)
        throw E_LOG_USAGE0(
            "FlightRouteValues() returned no data; must have equal number of locations, levels and "
            "times");

      nTimes = nm.NX();
      nLevels = 1;
    }
  }

  const MatrixPos native_gs(nTimes, nLevels);
  const Projection &native_proj = getProjection();
  NA_Param::Unit unit = SQD_Tools::unit_by_id(e);

  MemMatrix *m = L ? new (L) MemMatrix(native_gs, unit, native_proj)
                   : ::new MemMatrix(native_gs, unit, native_proj);

  matrix_fill(*m, nm);

  return m;
}

/*
 * 23-Sep-2011 PKi: Fetch grid locations
 */

/*virtual*/ void SQD_Data::getLocations(vector<LatLon> &locations,
                                        const Projection *target_proj) const
{
  NFmiFastQueryInfo fi(qd);

  if (fi.Grid())
  {
    NFmiDataMatrix<NFmiPoint> nm;

    fi.Locations(nm);

    typedef NFmiDataMatrix<float>::size_type sz_t;
    const sz_t xs = nm.NX();
    const sz_t ys = nm.NY();

    for (sz_t y = 0; y < ys; y++)
      for (sz_t x = 0; x < xs; x++)
      {
        const NFmiPoint &p = nm[x][y];
        locations.push_back(LatLon(p.Y(), p.X()));
      }
  }
  else
  {
    NFmiAreaFactory::return_type area =
        target_proj ? NFmiAreaFactory::Create(target_proj->toString()) : nullptr;

    for (fi.ResetLocation(); fi.NextLocation();)
    {
      NFmiPoint p(fi.LatLon());

      if ((!area) || (area->IsInside(p))) locations.push_back(LatLon(p.Y(), p.X()));
    }
  }

  return;
}

/*
 * 07-Apr-2015 PKi: Fetch data point (e.g. observation station) id's
 */

/*virtual*/ void SQD_Data::getDataIds(std::vector<unsigned long> &dataIds,
                                      const Projection *target_proj) const
{
  NFmiFastQueryInfo fi(qd);

  if (fi.Grid()) throw runtime_error("Can't query data point id's from gridded data");

  if (target_proj)
  {
    NFmiAreaFactory::return_type area = NFmiAreaFactory::Create(target_proj->toString());

    for (fi.ResetLocation(); fi.NextLocation();)
    {
      NFmiPoint p(fi.LatLon());

      if (area->IsInside(p)) dataIds.push_back(fi.Location()->GetIdent());
    }
  }
  else
    for (fi.ResetLocation(); fi.NextLocation();)
      dataIds.push_back(fi.Location()->GetIdent());

  return;
}

/*
 * 07-Apr-2015 PKi: Fetch data point (e.g. observation station) names
 */

/*virtual*/ void SQD_Data::getDataNames(vector<string> &dataNames,
                                        const Projection *target_proj) const
{
  NFmiFastQueryInfo fi(qd);

  if (fi.Grid()) throw runtime_error("Can't query data point names from gridded data");

  if (target_proj)
  {
    NFmiAreaFactory::return_type area = NFmiAreaFactory::Create(target_proj->toString());

    for (fi.ResetLocation(); fi.NextLocation();)
    {
      NFmiPoint p(fi.LatLon());

      if (area->IsInside(p))
        dataNames.push_back(
            string(reinterpret_cast<const char *>(fi.Location()->GetName().GetCharPtr())));
    }
  }
  else
    for (fi.ResetLocation(); fi.NextLocation();)
      dataNames.push_back(
          string(reinterpret_cast<const char *>(fi.Location()->GetName().GetCharPtr())));

  return;
}

/*
 * 13-Oct-2011 PKi: For checking the availability of height levels in data
 */
/*virtual*/ bool SQD_Data::providesHeightLevelsFromHybrid() const
{
  // 14-Oct-2011 PKi: Better to use query info instead
  //
  // ApiScalarParam geop(":2"),geom(":3");
  // const vector<NA_Param> &p = getParams();
  //
  // return (geop.covered_by(p) || geom.covered_by(p));

  NFmiFastQueryInfo fi(qd);
  return fi.HeightValueAvailable();
}

/*
 * 18-Oct-2011 PKi: Primary/secondary parameter mapping for virtual parameters (currently Z and W).
 *
 *                  Sets 'mapped' to true and returns the id of primary or secondary parameter as
 * requested if the input parameter 'p' is one of the mapped parameters. Otherwise 'mapped' is set
 *                  to false and returns the id of 'p'.
 *
 * 31-Oct-2011 PKi: Mapping now through virtual_params[] instead of coding parameterwise.
 */

#if 0
static const NA_Param virtual_Z( "Z", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, true );
static const NA_Param virtual_W( "W", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, true );
#endif

FmiParameterName SQD_Data::mapParameter(const NA_Param &p, bool primary, bool *mapped) const
{
  bool dummy;
  bool &isMapped = (mapped ? *mapped : dummy);

  isMapped = false;

#if 0
#if (defined(SQD_CASCADE_Z_ENABLED) || defined(SQD_CASCADE_W_ENABLED))
    string s = p.toString(false);
#endif

#ifdef SQD_CASCADE_Z_ENABLED
    if (s == virtual_Z.getNativeName_())
    {
    	isMapped = true;
    	return (primary ? SQD_PRIMARY_Z : SQD_SECONDARY_Z);
    }
    else
#endif
#ifdef SQD_CASCADE_W_ENABLED
    if (s == virtual_W.getNativeName_())
    {
    	isMapped = true;
    	return (primary ? SQD_PRIMARY_W : SQD_SECONDARY_W);
    }
    else
#endif
#endif

  // 31-Oct-2011 PKi
  //
  virtualParameterName v = NA_Param::virtual_param_native_id(p.toString(false).c_str());
  FmiParameterName id = (primary ? v.primaryId : v.secondaryId);

  if (id)
  {
    isMapped = true;
    return id;
  }

  return SQD_Tools::param_id(p);
}

/*
 * 31-Oct-2011 PKi: Virtual parameter mapping for native parameters; collects virtual parameters
 *                  to be added to the parameter set.
 */

void SQD_Data::mapParameter(const FmiParameterName id,
                            std::map<std::string, FmiParameterName> &virtualParams)
{
  string s = NA_Param::native_id_virtual_param(id);
  if (s != "") virtualParams.insert(make_pair(s, id));

  return;
}
