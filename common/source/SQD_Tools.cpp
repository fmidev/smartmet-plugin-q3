/*
* SQD_TOOLS.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  4-Oct-2010
*/
#include "SQD_Tools.h"

#include "newbase/NFmiMetTime.h"
#include "newbase/NFmiAreaFactory.h"
#include "newbase/NFmiQueryInfo.h"
#include "newbase/NFmiFastQueryInfo.h"
#include "newbase/NFmiLevel.h"

#include "MemMatrix.h"

#include <string>
#include <map>
#include <fstream>

using namespace std;


/*---=== Helpers ===---
*/


/*---=== SQD_Tools ===---
*/

/*
* Make a Newbase 'NFmiLevel' from Q3 'NA_Level'
*/
NFmiLevel SQD_Tools::newbase_level( const NA_Level &lev ) /*throw(E_BUG)*/ {
    FmiLevelType lt;

    switch( lev.getType() ) {
        case NA_Level::GROUND_LEVEL:   lt= kFmiGroundSurface; break;
        case NA_Level::HYBRID_LEVEL:   lt= kFmiHybridLevel; break;
        case NA_Level::PRESSURE_LEVEL: lt= kFmiPressureLevel; break;
        default:
            throw E_LOG_BUG( "Unexpected NA_Level type: %d", (int)lev.getType() );
    }

    return NFmiLevel( lt, (long) lev.getValue() );   // Newbase has 'long' as level value
}


/*
* Make a 'NA_Level' object from Newbase level.
*/
NA_Level SQD_Tools::q3_level( FmiLevelType lt, double lv ) /*throw(E_BUG)*/ {

    if (lt==kFmiGroundSurface) {
        return NA_Level( NA_Level::GROUND_LEVEL );  // ignore 'lv'

    } else if (lt==kFmiHybridLevel) {
        return NA_Level( NA_Level::HYBRID_LEVEL, lv );
    
    } else if (lt==kFmiPressureLevel) {
        return NA_Level( NA_Level::PRESSURE_LEVEL, lv );

    } else if (lt==kFmiSoundingLevel) {
        return NA_Level( NA_Level::SOUNDING_LEVEL, lv  );
    }

    throw E_LOG_BUG( "Cannot make 'NA_Level' of Newbase level type %d", (int)lt );
}


/*
*/
NFmiMetTime SQD_Tools::jd2mt( const JDay &a ) {
    unsigned yyyy, mm, dd;
    a.gregorian( &yyyy, &mm, &dd );     // faster to convert them all at once
    
    // NOTE: Seconds are lost in conversion to 'NFmiMetTime()'
    //
    return NFmiMetTime( yyyy, mm, dd, a.hour(), a.min(), a.sec(), 1 /*timestep in minutes*/ );
}


/*
*/
JDay SQD_Tools::mt2jd( const NFmiMetTime &mt ) {

    // DAMMIT!!!  Newbase const-ness sucks.  ('NFmiStaticTime::IsMissing' should be const!)
    //
#if 1
    if (NFmiMetTime(mt).IsMissing())    // hack to bypass non-const
#else
    if (mt.IsMissing())     // this should work
#endif
    {
        return JDay();  // NAN (no date)
    } else {
        return JDay( mt.GetYear(), mt.GetMonth(), mt.GetDay(), mt.GetHour(), mt.GetMin(), mt.GetSec() );
    }
}


/*---=== Projections ===---
*/

/*
* Find out relative position of 'll' within the projection.
*
* 'dx' is written the relative x coordinate (0..1 if within the grid)
* 'dy' is written the relative y coordinate (0..1 if within the grid)
*
* Returns 'true' if 'proj' was a Newbase projection (whether point is within it or not).
*         'false' if the projection string was unknown to Newbase.
*/
#if 0
bool SQD_Tools::latlon_dx_dy( const char *proj, const LatLon &ll, double &dx, double &dy ) {
    assert(proj);
    
    const unsigned X_SIZE= 10000;
    const unsigned Y_SIZE= 10000;

    // TBD: What happens if 'proj' is not a valid Newbase projection (we should return
    //      'false' in that case)
    //
    auto_ptr<NFmiArea> ap( NFmiAreaFactory::Create( proj ) );
    NFmiGrid grid( ap.get(), X_SIZE, Y_SIZE );
    
    NFmiPoint p= grid.LatLonToGrid( ll.getLon(), ll.getLat() );     // yes, lon first

    dx= p.X() / X_SIZE;
    dy= p.Y() / Y_SIZE;
    
    return true;
}
#endif


/*---=== Parameter units ===---
*
* Newbase has parameter units tied to their id numbers.
*
* The units, in turn, define i.e. the interpolation method used by q3/MetQu.
*/

/*
* NOTE: We need to define the values here (not use the 'NA_Param::' globals) so that they are guaranteed
*       to be initialized prior to 'known_params' table.
*/
static const NA_Param::Unit UNIT_FLOAT_NEAREST( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_NEAREST );
static const NA_Param::Unit UNIT_UINT16_NEAREST( NA_Param::DATATYPE_UINT16, NA_Param::INTERPOLATE_NEAREST );
static const NA_Param::Unit UNIT_MAX14_ENUM( NA_Param::DATATYPE_HALFBYTE, NA_Param::INTERPOLATE_NEAREST );
static const NA_Param::Unit UNIT_MAX254_ENUM( NA_Param::DATATYPE_BYTE, NA_Param::INTERPOLATE_NEAREST );
static const NA_Param::Unit UNIT_BOOL( NA_Param::DATATYPE_BOOL, NA_Param::INTERPOLATE_NEAREST );
static const NA_Param::Unit UNIT_DEG( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR_DEG, "deg" );
static const NA_Param::Unit UNIT_PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "%" );
static const NA_Param::Unit UNIT_10PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "10%" );  // 0..10 (with fractions)
static const NA_Param::Unit UNIT_100PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "100%" ); // 0..1 (with fractions)
static const NA_Param::Unit UNIT_1( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "1" );   // anything (but "just numbers")

static const NA_Param::Unit UNIT_UNKNOWN( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_UNKNOWN );

#define U(s) NA_Param::Unit( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, (s) )

//---
// Note: UNIT_HOUR and UNIT_DAY are to be used for parameters where the value just
//       keeps going (i.e. past 24 hours, or 365 days). For parameters dealing with
//       limited upper range (calendar time) use 'UNIT_NEAREST'.
//
static const NA_Param::Unit UNIT_HOUR( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "h" );
static const NA_Param::Unit UNIT_DAY( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "h24" );
    //
    // data type: float (fractions allowed)

#define PARAM(id, unit )     { id, unit }

static struct { 
    const FmiParameterName id;    // id for the param in Newbase
    const NA_Param::Unit unit;
} known_params[]= {
    //---
    // Standard named scalar parameters. Using i.e. "T" points to any param with id 4 (unless there is a param
    // named "T" in the file).
    //
    PARAM( /*P*/     kFmiPressure, U("hPa") ),
    PARAM( /*Z*/     kFmiGeopHeight, U("gpm") ),    // Primary Z param
    PARAM(           kFmiGeomHeight,  U("m") ),     // Secondary Z param (used if 'kFmiGeopHeight' is not there)
    PARAM( /*T*/      kFmiTemperature, U("°C") ),
    PARAM( /*THETAW*/ kFmiPseudoAdiabaticPotentialTemperature, U("K") ),
    PARAM( /*DP*/     kFmiDewPoint, U("°C") ),
    PARAM( /*RH*/     kFmiHumidity, UNIT_PRC ),
    PARAM( /*W*/      kFmiVerticalVelocityMMS, U("mm/s") ),
    PARAM( /*RRCON*/  kFmiPrecipitationConv, U("mm") ),       // or "kg/m²" (same values, since 1l of water weighs a kg)
    PARAM( /*RRLAR*/  kFmiPrecipitationLarge, U("mm") ),
    PARAM( /*CAPE*/   kFmiCAPE, U("J/kg") ),
    PARAM( /*KIND*/   kFmiKIndex, UNIT_1 ),
    PARAM( /*POP*/    kFmiPoP, UNIT_PRC ),
    PARAM( /*TKE*/    kFmiTurbulentKineticEnergy, U("J/kg") ),
    PARAM( /*PSEUDOSATEL*/ kFmiRadiationNetTopAtmLW, U("W/m²") ),
    PARAM( /*LRAD*/   kFmiRadiationLW, U("W/m²") ),         // "pitkäaaltoinen säteily" / long wave radiation
    PARAM( /*SRAD*/   kFmiRadiationGlobal, U("W/m²") ),     // "auringon säteily" / sun radiation
    PARAM( /*VIS*/    kFmiVisibility, U("m") ),      // "visibility"
    PARAM( /*AVIVIS*/  kFmiAviationVisibility, U("m") ),
    PARAM( /*VERVIS*/  kFmiVerticalVisibility, U("m") ),
    PARAM( /*MIST*/    kFmiMist, UNIT_FLOAT_NEAREST ),      // TBD: unit? range?

    //---
    // 'WeatherAndCloudiness' derivatives
    //
    PARAM( /*N*/ kFmiTotalCloudCover,    UNIT_MAX14_ENUM ),    // 0..7
    PARAM( /*CL*/ kFmiLowCloudCover,     UNIT_MAX14_ENUM ),    // 0..10
    PARAM( /*CM*/ kFmiMediumCloudCover,  UNIT_MAX14_ENUM ),    // 0..10
    PARAM( /*CH*/ kFmiHighCloudCover,    UNIT_MAX14_ENUM ),    // 0..7 (Newbase v7)
    PARAM( /*RR*/ kFmiPrecipitation1h,   U("mm") ),
    PARAM( /*PRET*/ kFmiPrecipitationType, UNIT_MAX14_ENUM ),    // 0..2
    PARAM( /*PREF*/ kFmiPrecipitationForm, UNIT_MAX14_ENUM ),    // 0..6
        //
        // 0: drizzle (tihku)
        // 1: rain (sade)
        // 2: sleet (nuoska)
        // 3: snow (lumi)
        // 4: freezing drizzle (jäätävä tihku)
        // 5: freezing rain (jäätävä sade)
        // 6: hail (rakeita)

    PARAM( /*FOG*/ kFmiFogIntensity,     UNIT_MAX14_ENUM ),   // 0..2
    PARAM( /*THUND*/ kFmiProbabilityThunderstorm, UNIT_MAX14_ENUM ), // 0..10
    PARAM( /*HSADE*/ kFmiWeatherSymbol1, UNIT_MAX254_ENUM ),    // 0..99
    PARAM( /*HESSAA*/ kFmiWeatherSymbol3, UNIT_MAX254_ENUM ),    // 0..99
    PARAM( /*MiddleAndLowCloudCover*/ kFmiMiddleAndLowCloudCover, UNIT_PRC ),

    //---
    // 'TotalWindMS' derivatives
    //
    PARAM( /*WD*/ kFmiWindDirection,    UNIT_DEG ),    // 0 (no wind) .. 360 (north)
    PARAM( /*WS*/ kFmiWindSpeedMS,      U("m/s") ),    // 0..409.5 (== WIND.abs)

    // NOTE: GUST parameter is broken on SQD / Newbase. (TBD: check it out in detail, 
    //      at least report how exactly it's broken)
    //
    PARAM( /*GUST*/ kFmiHourlyMaximumGust, U("m/s") ),    // 0..409.5 (fixed one decimal)

    PARAM( /*U*/ kFmiWindUMS, U("m/s") ),
    PARAM( /*V*/ kFmiWindVMS, U("m/s") ),

    // WVEC: Old remains param that could be discontinued; the same can be acquired
    //       by simple calculation (kept alive for sake of compatibility, and for
    //       telling how to interpolate them; or rather NOT to interpolate).
    //
    PARAM( /*WVEC*/ kFmiWindVectorMS, UNIT_UINT16_NEAREST ),     // round(WS)*100 + WD/10  -> range 0..40960 (theoretically)
        //
        // Note: must not use interpolation for this: part rotary, part linear

    //---
    // Other params (not standard named, need to be accessed by their id numbers
    // or by the names provided in the particular SQD file).
    //
#ifdef TOPO_AS_PARAMS
    PARAM( kFmiElevationAngle,     UNIT_DEG ),
#endif

#ifdef TOPO_AS_PARAMS
    PARAM( kFmiTopoGraf,           U("m") ),        // ground height from sea level
    PARAM( kFmiLandSeaMask,   UNIT_100PRC ),  // value: 0..1 (fraction of land surface underneath)

    PARAM( kFmiTopoSlope,         UNIT_DEG ),
    PARAM( kFmiTopoAzimuth,      UNIT_DEG ),

    PARAM( kFmiTopoDistanceToSea, U("km") ),
    PARAM( kFmiTopoDirectionToSea, UNIT_DEG ),

    PARAM( kFmiTopoDistanceToLand, U("km") ),
    PARAM( kFmiTopoDirectionToLand, UNIT_DEG ),
#endif

#ifdef TIME_AS_PARAM
    // Note: Not sure if they carry fractions (thinking they don't)
    //
    PARAM( kFmiDay,               UNIT_UINT16_NEAREST ),       // value: 1..366
    PARAM( kFmiHour,              UNIT_BYTE_NEAREST ),       // 0..23
    PARAM( kFmiSecond,            UNIT_BYTE_NEAREST ),       // 0..23 (yes, don't mind the name)
    PARAM( kFmiForecastPeriod,    UNIT_FLOAT_NEAREST ),       // TBD: unit unsure; maybe minutes ? <http://tih.fmi.fi/www_sivut/tuotanto_ajot/ParamIdents.html>
    PARAM( kFmiDeltaTime,         UNIT_BYTE_NEAREST ),   // 0..23 time step used in modelling
#endif

    PARAM( kFmiMaximumTemperature, U("°C") ),
    PARAM( kFmiMinimumTemperature, U("°C") ),
    PARAM( kFmiTemperatureAnomaly, U("°C") ),
    PARAM( kFmiPotentialTemperature, U("°C") ),
    PARAM( kFmiDewPointDeficit, U("°C") ),
    PARAM( kFmiSpecificHumidity, UNIT_1 ),  // kg/kg
    PARAM( kFmiMixingRatio, UNIT_1 ),       // kg/kg
    PARAM( kFmiStabilityIndex, U("°C") ),
    PARAM( kFmiSaturationDeficit, U("hPa") ),
    PARAM( kFmiStreamFunction, U("10⁵ m²/s") ),
    PARAM( kFmiVorticityRelative, U("10⁻⁵/s") ),
    PARAM( kFmiVorticityAbsolute, U("10⁻⁵/s") ),
    PARAM( kFmiVorticityAdvectionRelative, U("10⁻⁹/s²") ),
    PARAM( kFmiVorticityAdvectionAbsolute, U("10⁻⁹/s²") ),
    PARAM( kFmiVelocityDivergenceHoriz, U("10⁻⁵/s²") ),
    PARAM( kFmiMoistureDivergenceHoriz, U("1/s") ),  // kg/kgs
    PARAM( kFmiVorticityGeostrophic, U("10⁻⁵/s") ),
    PARAM( kFmiVelocityPotential, U("m²/s") ),
    PARAM( kFmiVerticalVelocityCBS, U("cb/s") ),
    PARAM( kFmiVerticalVelocityCB12H, U("cb/12h") ),
    PARAM( kFmiVerticalVelocityHPAH, U("hPa/h") ),
    PARAM( kFmiWindShearMS, U("m/s") ),
    PARAM( kFmiWindShearKT, U("kt") ),
    PARAM( kFmiLapseRate, U("°C/m") ),
    PARAM( kFmiPrecipitableWater, U("kg/m²") ),
    PARAM( kFmiPrecipitationRate, U("kg/m²") ),
    PARAM( kFmiPrecipitationAmount, U("mm") ),
    PARAM( kFmiSnowDepth, U("cm") ),
    PARAM( kFmiRadiationOutLW, U("W/m²") ),
    PARAM( kFmiRadiationOutSW, U("W/m²") ),
    PARAM( kFmiRadiationInSW, U("W/m²") ),
    PARAM( kFmiGolfIndex, UNIT_FLOAT_NEAREST ),   // TBD: range; integer only?
    PARAM( kFmiSeaLevel, U("cm") ),
    PARAM( kFmiTemperatureSea, U("°C") ),
    PARAM( kFmiSalinity, UNIT_FLOAT_NEAREST ),    // TBD: range; integer only?
    PARAM( kFmiDensity, UNIT_FLOAT_NEAREST ),     // TBD: range; integer only?
    PARAM( kFmiSeaLevelMinimum, U("cm") ),
    PARAM( kFmiSeaLevelMaximum, U("cm") ),
    PARAM( kFmiThickness, U("m") ),
    PARAM( kFmiRadarProducer, UNIT_FLOAT_NEAREST ),    // TBD: range; integer only?
    PARAM( kFmiRadarParameter, UNIT_FLOAT_NEAREST ),   // TBD: range; integer only?
    PARAM( kFmiRadarLevelType, UNIT_FLOAT_NEAREST ),   // TBD: range; integer only?
    PARAM( kFmiCAPPIHeight, U("m") ),
    PARAM( kFmiRadarRadius, U("km") ),
    PARAM( kFmiRadarLayer, U("km") ),
    PARAM( kFmiAzimuthalLayer, UNIT_DEG ),
    PARAM( kFmiCrossSectionDirection, UNIT_DEG ),
    PARAM( kFmiAccumulationTime, UNIT_HOUR ),
    PARAM( kFmiMindBZ, U("dBZ") ),
    PARAM( kFmiMinRange, U("km") ),
    PARAM( kFmiMaxRange, U("km") ),
    PARAM( kFmiHorizontalResolution, U("m") ),   // =~ GSIZE.x
    PARAM( kFmiZResolution, U("m") ),
    PARAM( kFmiCorrectedReflectivity, U("dBZ") ),
    PARAM( kFmiEchoTop, U("km") ),
    PARAM( kFmiRainfallDepth, U("mm") ),
    PARAM( kFmiRawRadarData, UNIT_FLOAT_NEAREST ),     // TBD: range; integer only?
    PARAM( kFmiRadialWind, U("m/s") ),
    PARAM( kFmiWindAndReflectivity, U("m/s-dBZ") ),   // NULL: what is that unit?
    PARAM( kFmiMaxLayerHeight, U("km") ),
    PARAM( kFmiMaxElevation, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiProjectionID, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiGridOrientation, UNIT_FLOAT_NEAREST ),  // TBD: unit?
    PARAM( kFmiVelocityPotentialM2S, UNIT_FLOAT_NEAREST ), // TBD: unit?
    PARAM( kFmiVerticalVelocityHPAS, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiVerticalVelocityDPAS, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiVerticalVelocityMS, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiWindShearMS2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiLogNatOfPressure, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiLapseRate2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiPrecipitableWater2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiSnowDepth2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiRadiationOutLW2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiRadiationOutSW2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiRadiationInSW2, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiBatteryVoltage, U("V") ),
    PARAM( kFmiSeaLevelAnomaly, UNIT_FLOAT_NEAREST ),// TBD: unit?
    PARAM( kFmiTemperatureSea2, U("°C") ),
    PARAM( kFmiTemperatureSeaSurface, U("°C") ),
    PARAM( kFmiCurrentSpeed, U("m/s") ),
    PARAM( kFmiCurrentDirection, UNIT_DEG ),
    PARAM( kFmiSigWaveHeight, U("cm") ),
    PARAM( kFmiSigWavePeriod, U("1/s") ),
    PARAM( kFmiWaveDirection, UNIT_DEG ),
    PARAM( kFmiWaveSpread, UNIT_DEG ),
    PARAM( kFmiMaxWaveHeight, U("cm") ),
    PARAM( kFmiMaxWavePeriod, U("s") ),
    PARAM( kFmiWavePeriod, U("s") ),
    PARAM( kFmiSigWaveHeightBandB, U("cm") ),
    PARAM( kFmiSigWavePeriodBandB, U("s") ),
    PARAM( kFmiWaveDirectionBandB, UNIT_DEG ),
    PARAM( kFmiSigWaveHeightBandC, U("cm") ),
    PARAM( kFmiSigWavePeriodBandC, U("s") ),
    PARAM( kFmiWaveDirectionBandC, UNIT_DEG ),
    PARAM( kFmiUnidirectivityIndex, UNIT_FLOAT_NEAREST ),   // unit?
    PARAM( kFmiSigWaveLength, U("m") ),
    PARAM( kFmiMixedLayerDepth, U("m") ),
    PARAM( kFmiSigWaveHeightSwell0, U("cm") ),
    PARAM( kFmiSigWaveHeightSwell1, U("cm") ),
    PARAM( kFmiSigWaveHeightSwell2, U("cm") ),
    PARAM( kFmiSigWavePeriodSwell0, U("s") ),
    PARAM( kFmiSigWavePeriodSwell1, U("s") ),
    PARAM( kFmiSigWavePeriodSwell2, U("s") ),
    PARAM( kFmiWaveDirectionSwell0, UNIT_DEG ),
    PARAM( kFmiWaveDirectionSwell1, UNIT_DEG ),
    PARAM( kFmiWaveDirectionSwell2, UNIT_DEG ),

    PARAM( kFmiNO2Contents, U("mg/m³") ),
    //PARAM( kFmiCO2Contents, U("mg/m³") ),      // not known to Newbase
    PARAM( kFmiSO2Contents, U("mg/m³") ),
    PARAM( kFmiAQIndex, UNIT_FLOAT_NEAREST ),   // TBD: unit?
    PARAM( kFmiSmogIndex, UNIT_FLOAT_NEAREST ), // TBD: unit?
    
    PARAM( kFmiPrecipitationRateLarge, U("mm/h") ),
    PARAM( kFmiPrecipitationRateConv, U("mm/h") ),
    PARAM( kFmiPressureReduced, U("hPa") ),
    PARAM( kFmiPressureTendency, UNIT_FLOAT_NEAREST ),    // TBD: unit? range? integer only?
    PARAM( kFmiTemperatureVirtual, U("°C") ),
    PARAM( kFmiSnowWarning, UNIT_FLOAT_NEAREST ),         // TBD: unit?
    PARAM( kFmiPressureAnomaly, U("hPa") ),
    PARAM( kFmiGeopotentialHeightAnomaly, U("gpm") ),
    PARAM( kFmiDivergenceAbsolute, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiDivergenceRelative, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiWindShearU, U("m/s") ),
    PARAM( kFmiWindShearV, U("m/s") ),
    PARAM( kFmiVapourPressure, U("hPa") ),
    PARAM( kFmiEvaporation, U("mm") ),
    PARAM( kFmiFlashAccuracy, U("km") ),
    PARAM( kFmiFlashMultiplicity, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiFlashStrength, U("kA") ),
    PARAM( kFmiSnowfallRate, U("mm/h") ),
    PARAM( kFmiSnowAccumulation, U("mm") ),
    PARAM( kFmiWaterEquivalentOfSnow, U("mm") ),
    PARAM( kFmiConvectiveCloudCover, UNIT_PRC ),
    PARAM( kFmiCloudWater, UNIT_1 ),    // kg/kg
    PARAM( kFmiConvectiveSnow, U("mm") ),
    PARAM( kFmiLargeScaleSnow, U("mm") ),
    PARAM( kFmiForestFraction, UNIT_100PRC ),    // value: 0..1
    PARAM( kFmiSurfaceRoughness, UNIT_FLOAT_NEAREST ), // TBD: unit?
    PARAM( kFmiAlbedo, UNIT_100PRC ),    // value: 0..1
    PARAM( kFmiSoilTemperature, U("°C") ),
    PARAM( kFmiSoilMoistureContent, U("m") ),
    PARAM( kFmiVegetation, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiGroundTemperature, U("°C") ),
    PARAM( kFmiLocationId, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    // PARAM( kFmiModelLevel, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    // PARAM( kFmiTemperatureChgByParamn, U("Kkg/m²") ),
    // PARAM( kFmiHumidityChgByParamn, U("kg/m²") ),
    // PARAM( kFmiCloudWaterChgByParamn, U("kg/m²") ),
    PARAM( kFmiRadiationNetSurfaceSW, U("W/m²") ),
    PARAM( kFmiRadiationNetSurfaceLW, U("W/m²") ),
    PARAM( kFmiRadiationNetTopAtmSW, U("W/m²") ),
    PARAM( kFmiRadiationSW, U("W/m²") ),
    PARAM( kFmiRadiationReflected, U("W/m²") ),
    
    PARAM( kFmiLatentHeatFlux, U("J/m²") ),
    PARAM( kFmiSensibleHeatFlux, U("J/m²") ),
    PARAM( kFmiBoundaryLayerDissipation, UNIT_FLOAT_NEAREST ),  // TBD: unit?
    PARAM( kFmiUMomentumFlux, U("N/m²s") ),
    //PARAM( kFmiVMomontumFlux, U("N/m²s") ),    // not known to Newbase
    PARAM( kFmiCloudSymbol , UNIT_FLOAT_NEAREST ),      // TBD: unit?
    PARAM( kFmiLowCloudSymbol, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiMediumCloudSymbol, UNIT_FLOAT_NEAREST ), // TBD: unit?
    PARAM( kFmiHighCloudSymbol, UNIT_FLOAT_NEAREST ),   // TBD: unit?
    PARAM( kFmiConvCloudSymbol, UNIT_FLOAT_NEAREST ),   // TBD: unit?
    PARAM( kFmiFrontSymbol, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiFogSymbol, UNIT_FLOAT_NEAREST ),         // TBD: unit?
    PARAM( kFmiSmogSymbol, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiWeatherSymbol2, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiSimpleWeather, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiWindSpeedGeostr, U("m/s") ),
    PARAM( kFmiWindSpeed10M, U("m/s") ),
    PARAM( kFmiWindDirection10M, UNIT_DEG ),
    PARAM( kFmiTemperature2M, U("°C") ),
    PARAM( kFmiDewPoint2M, U("°C") ),
    PARAM( kFmiHumidity2M, UNIT_PRC ),
    PARAM( kFmiWindUMS10M, U("m/s") ),
    PARAM( kFmiWindVMS10M, U("m/s") ),
    PARAM( kFmiShipDirection, UNIT_DEG ),
    PARAM( kFmiShipSpeed, U("m/s") ),
    PARAM( kFmiChillFactor, U("°C") ),
    PARAM( kFmiDegreeDays, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiPrecipitation3h, U("mm") ),
    PARAM( kFmiPrecipitation6h, U("mm") ),
    PARAM( kFmiPrecipitation12h, U("mm") ),
    PARAM( kFmiPrecipitation24h, U("mm") ),
    PARAM( kFmiMaximumTemperature24h, U("°C") ),
    PARAM( kFmiMinimumTemperature24h, U("°C") ),
    PARAM( kFmiMaximumTemperature2m, U("°C") ),
    PARAM( kFmiMinimumTemperature2m, U("°C") ),
    PARAM( kFmiMinimumWind, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiSatoKierto, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiFrostProbability, UNIT_PRC ),        // 0..100 (#372)
    PARAM( kFmiSevereFrostProbability, UNIT_PRC),   // 0..100 (#373)
    PARAM( kFmiConvectiveSnowFallRate, U("mm/h") ),
    PARAM( kFmiLargeScaleSnowFallRate, U("mm/h") ),
    PARAM( kFmiSurfaceRoughnessAtSea, UNIT_FLOAT_NEAREST ),     // TBD: unit?

    PARAM( kFmiIceConcentration, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiIceThickness, UNIT_FLOAT_NEAREST ),     // TBD: unit?  // NB: has been 'kFmiWindInstantaneous'
    PARAM( kFmiIceMinThickness, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiIceMaxThickness, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiIceDegreeOfRidging, UNIT_FLOAT_NEAREST ),    // TBD: unit?

    PARAM( kFmiCanopyWater, UNIT_FLOAT_NEAREST ),           // TBD: unit?
    PARAM( kFmiSurfaceTypeFraction, UNIT_FLOAT_NEAREST ),   // TBD: unit?
    PARAM( kFmiSoilType, UNIT_FLOAT_NEAREST ),              // TBD: unit?
    PARAM( kFmiVegetationType, UNIT_FLOAT_NEAREST ),        // TBD: unit?

    PARAM( kFmiForecasterCode, UNIT_1 ),    // just a number (TBD: is it interpolatable or enum?)
    PARAM( kFmiWmoBlockNumber, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiWmoStationNumber, UNIT_FLOAT_NEAREST ),      // TBD: unit?
    PARAM( kFmiStationType, UNIT_FLOAT_NEAREST ),           // TBD: unit?
    PARAM( kFmiPressureChange, U("hPa") ),       // TBD: unit?
    PARAM( kFmiPresentWeather, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiPastWeather1, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiPastWeather2, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiLowCloudType, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiMiddleCloudType, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiHighCloudType, UNIT_FLOAT_NEAREST ),         // TBD: unit?
    PARAM( kFmiCloudHeight, U("m") ),
    PARAM( kFmiPrecipitationPeriod, UNIT_HOUR ),
    PARAM( kFmiMaximumWind, U("m/s") ),
    PARAM( kFmiWindGust, U("m/s") ),
    PARAM( kFmiOldVisibility, U("m") ),
    PARAM( kFmiVisibilityChange, U("m") ),
    PARAM( kFmiRainOnOff, UNIT_BOOL ),     // value: 0|1
    PARAM( kFmiSunOnOff, UNIT_BOOL ),      // value: 0|1
    PARAM( kFmiWetnessOnOff, UNIT_BOOL ),  // value: 0|1
    PARAM( kFmiRainMinutes, U("min") ),
    PARAM( kFmiStateOfGround, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiSnowDepth06, U("cm") ),
    PARAM( kFmiSnowDepth18, U("cm") ),
    PARAM( kFmiDailyMeanTemperature, U("°C") ),
    PARAM( kFmiMinimumTemperature06, U("°C") ),
    PARAM( kFmiMaximumTemperature06, U("°C") ),
    PARAM( kFmiMinimumGroundTemperature06, U("°C") ),
    PARAM( kFmiMinimumTemperature18, U("°C") ),
    PARAM( kFmiMaximumTemperature18, U("°C") ),
    PARAM( kFmiMonthlyMeanTemperature, U("°C") ),
    PARAM( kFmiMonthlyPrecipitation, U("mm") ),
    PARAM( kFmiSunHours, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiDailyGlobalRadiation, U("kJ/m²") ),
    PARAM( kFmiDailyDiffuseRadiation, U("kJ/m²") ),
    PARAM( kFmiDailyReflectedRadiation, U("kJ/m²") ),
    PARAM( kFmiDailyNetRadiation, U("kJ/m²") ),
    PARAM( kFmiPrecipitationReliability, UNIT_FLOAT_NEAREST ),  // TBD: unit?
    PARAM( kFmiTotalOzone, U("DU") ),
    PARAM( kFmiUVCumulated, U("MED") ),
    PARAM( kFmiUVMaximum, U("MED/h") ),
    PARAM( kFmiTimeOfUVMaximum, UNIT_FLOAT_NEAREST ),   // hhmm  (UNIT_xxx_NEAREST forces 'nearest point' interpolation; no custom interpolation for these values - if it were in minutes we'd be able to do linear)
    PARAM( kFmiDailyPrecipitationCode, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiHourlySunShine, UNIT_HOUR ),
    PARAM( kFmiHourlyGlobalRadiation, U("kJ/m²") ),
    PARAM( kFmiHourlyDiffuseRadiation, U("kJ/m²") ),
    PARAM( kFmiHourlyReflectedRadiation, U("kJ/m²") ),
    PARAM( kFmiHourlyNetRadiation, U("kJ/m²") ),
    PARAM( kFmiMonthlyMeanPrecipitation3160, U("mm") ),
    PARAM( kFmiMonthlyMeanTemperature3160, U("°C") ),
    PARAM( kFmiMonthlyMeanCloudiness3160, UNIT_1 ),     // value: 0..8
    PARAM( kFmiMonthly15SnowDepth3160, U("cm") ),
    PARAM( kFmiMonthlyMeanPrecipitation6190, U("mm") ),
    PARAM( kFmiMonthlyMeanTemperature6190, U("°C") ),
    PARAM( kFmiMonthlyMeanCloudiness6190, UNIT_1 ),     // value: 0..8
    PARAM( kFmiMonthly15SnowDepth6190, U("cm") ),
    PARAM( kFmiHourlyPressure, U("hPa") ),
    PARAM( kFmiHourlyTemperature, U("°C") ),
    PARAM( kFmiHourlyMinimumTemperature, U("°C") ),
    PARAM( kFmiHourlyMaximumTemperature, U("°C") ),
    PARAM( kFmiHourlyRelativeHumidity, UNIT_PRC ),
    PARAM( kFmiHourlyWindSpeed, U("m/s") ),
    PARAM( kFmiHourlyMinimumWindSpeed, U("m/s") ),
    PARAM( kFmiHourlyMaximumWindSpeed, U("m/s") ),
    PARAM( kFmiHourlyWindDirection, UNIT_DEG ),
    PARAM( kFmiHourlyMaxRainIntensity, U("mm/h") ),
    PARAM( kFmiRainIntensityWeather, UNIT_FLOAT_NEAREST ),   // TBD: unit?
    PARAM( kFmiHourlyWaterTemperature, U("°C") ),
    PARAM( kFmiPressureAtStationLevel, U("hPa") ),
    PARAM( kFmiPrecipitation06, U("mm") ),
    PARAM( kFmiPrecipitation18, U("mm") ),
    PARAM( kFmiFlAltitude, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiPhaseOfFlight, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiTurbulence, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiBaseOfTurbulence, U("m") ),
    PARAM( kFmiTopOfTurbulence, U("m") ),
    PARAM( kFmiIcing, UNIT_FLOAT_NEAREST ),             // TBD: unit?
    PARAM( kFmi1CloudCover, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmi1CloudBase, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmi2CloudCover, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmi2CloudBase, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmi3CloudCover, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmi3CloudBase, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmi4CloudCover, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmi4CloudBase, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiCbCloudCover, UNIT_FLOAT_NEAREST ),      // TBD: unit?
    PARAM( kFmiCbCloudBase, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiSolarElevation, UNIT_FLOAT_NEAREST ),        // TBD: unit?
    PARAM( kFmiWeatherSymbolTaf, UNIT_FLOAT_NEAREST ),      // TBD: unit?
    PARAM( kFmiLandCover, UNIT_100PRC ),             // value: 0..1
    PARAM( kFmiIceCover, UNIT_100PRC ),              // value: 0..1
    PARAM( kFmiDeepSoilTemperature, U("°C") ),
    PARAM( kFmiDeepSoilMoistureContent, UNIT_1 ),   // kg/kg
    PARAM( kFmiForestGroundHumidity, UNIT_PRC ),
    PARAM( kFmiFieldGroundHumidity, UNIT_PRC ),
    PARAM( kFmiPrecipitation5d, U("mm") ),
    PARAM( kFmiClusterTemperature1, U("°C") ),
    PARAM( kFmiClusterTemperature2, U("°C") ),
    PARAM( kFmiClusterTemperature3, U("°C") ),
    PARAM( kFmiClusterTemperature4, U("°C") ),
    PARAM( kFmiClusterTemperature5, U("°C") ),
    PARAM( kFmiClusterTemperature6, U("°C") ),
    PARAM( kFmiClusterGeopHeight1, U("gpm") ),
    PARAM( kFmiClusterGeopHeight2, U("gpm") ),
    PARAM( kFmiClusterGeopHeight3, U("gpm") ),
    PARAM( kFmiClusterGeopHeight4, U("gpm") ),
    PARAM( kFmiClusterGeopHeight5, U("gpm") ),
    PARAM( kFmiClusterGeopHeight6, U("gpm") ),
    PARAM( kFmiProbabilityOfTempLimit1, UNIT_PRC ),
    PARAM( kFmiProbabilityOfTempLimit2, UNIT_PRC ),
    PARAM( kFmiProbabilityOfTempLimit3, UNIT_PRC ),
    PARAM( kFmiProbabilityOfTempLimit4, UNIT_PRC ),
    PARAM( kFmiProbabilityOfPrecLimit1, UNIT_PRC ),
    PARAM( kFmiProbabilityOfPrecLimit2, UNIT_PRC ),
    PARAM( kFmiProbabilityOfPrecLimit3, UNIT_PRC ),
    PARAM( kFmiProbabilityOfPrecLimit4, UNIT_PRC ),
    PARAM( kFmiProbabilityOfWindLimit1, UNIT_PRC ),
    PARAM( kFmiProbabilityOfWindLimit2, UNIT_PRC ),
    PARAM( kFmiTopoRelativeHeight, U("m") ),
    PARAM( kFmiSensorOrdinal, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiRoadCondition, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiProbabilityDryRoad, UNIT_PRC ),
    PARAM( kFmiProbabilityWetRoad, UNIT_PRC ),
    PARAM( kFmiProbabilityMoistRoad, UNIT_PRC ),
    PARAM( kFmiProbabilitySnowyRoad, UNIT_PRC ),
    PARAM( kFmiProbabilityFrostyRoad, UNIT_PRC ),
    PARAM( kFmiProbabilityIcyRoad, UNIT_PRC ),
    PARAM( kFmiRoadTemperature, U("°C") ),
    PARAM( kFmiGrowthPeriodDeviationInDays, UNIT_DAY ),     // 1..365
    PARAM( kFmiGrowthPeriodDeviationInPrcnt, UNIT_PRC ),
    PARAM( kFmiGrowthPeriodPrecipitation, U("mm") ),
    PARAM( kFmiEffectiveTemperatureSum, U("°C") ),
    //PARAM( kFmiWindCode, UNIT_FLOAT_NEAREST ),         // not known to Newbase
    //PARAM( kFmiWindVoltage, UNIT_FLOAT_NEAREST ),      // not known to Newbase
    //PARAM( kFmiAverageWind, UNIT_FLOAT_NEAREST ),      // not known to Newbase
    //PARAM( kFmiWindDeviation, UNIT_FLOAT_NEAREST ),    // not known to Newbase
    PARAM( kFmiWindDirectionCode, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiWindDirectionVoltage, UNIT_FLOAT_NEAREST ),  // TBD: unit?
    PARAM( kFmiAverageWindDirection, UNIT_DEG ),
    PARAM( kFmiMaximumWindDirection, UNIT_DEG ),
    PARAM( kFmiMinimumWindDirection, UNIT_DEG ),
    PARAM( kFmiWindDirectionDeviation, UNIT_DEG ),
    PARAM( kFmiWindSpeedDeviation, U("m/s") ),
    PARAM( kFmiWindVerticalDeviation, U("m") ),
    PARAM( kFmiTemperatureCode, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiTemperatureVoltage, UNIT_FLOAT_NEAREST ),    // TBD: unit?
    PARAM( kFmiTemperatureDeviation, UNIT_FLOAT_NEAREST ),  // TBD: unit?
    PARAM( kFmiHumidityCode, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiHumidityVoltage, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiAverageHumidity, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiMaximumHumidity, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiMinimumHumidity, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiHumidityDeviation, UNIT_FLOAT_NEAREST ),     // TBD: unit?
    PARAM( kFmiPressureCode, UNIT_FLOAT_NEAREST ),          // TBD: unit?
    PARAM( kFmiPressureVoltage, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiAveragePressure, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiMaximumPressure, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiMinimumPressure, UNIT_FLOAT_NEAREST ),       // TBD: unit?
    PARAM( kFmiPressureDeviation, UNIT_FLOAT_NEAREST ),     // TBD: unit?

    PARAM( (FmiParameterName)0, UNIT_UNKNOWN )   // end marker
};

static map< FmiParameterName, NA_Param::Unit > unit_by_id_;

/*
* Init the maps
*
* Note: We *must* do this as a function since there is no guarantee we'd be initialized before 'UNIT_DEG'
*       and others referred to here.
*/
static struct JustOnce_SQD_Tools {  // note: must have unique name (otherwise runtime problems, linker mixes the two structs)
  JustOnce_SQD_Tools() { 
    for( unsigned i=0; known_params[i].id; i++ ) {
        FmiParameterName e= known_params[i].id;

        // Make sure all the entries in the table have a known interpolation method.
        //
#ifndef NDEBUG
        NA_Param::e_Interpolation method= known_params[i].unit.getMethod();
        if (method==NA_Param::INTERPOLATE_UNKNOWN) {
            throw E_LOG_BUG( "Built-in table has unknown interpolation: %d #%d", (int)e, i );
        }
#endif
        unit_by_id_.insert( make_pair( e, known_params[i].unit ) );
    }
  }
} just_for_init;


/*
* Provides a unit for the given Newbase parameter id.
*/
NA_Param::Unit SQD_Tools::unit_by_id( FmiParameterName p ) {

    map< FmiParameterName, NA_Param::Unit >::const_iterator it= unit_by_id_.find(p);
    if (it != unit_by_id_.end()) {
#if 0
        NA_Param::Unit unit= it->second;
        LOG_DEBUG( "found #%d: %d %s", (int)p, (int)unit.getMethod(), unit.getUnitName().c_str() );
#endif
        return it->second;

    } else {
        if (!p) { 
            return NA_Param::UNIT_UNKNOWN_;     // shouldn't happen
        }

        // For unknown params id's (can be i.e. custom id in a file) default to linear interpolation,
        // float range.
        //
        return NA_Param::UNIT_UNKNOWN_INTERPOLATABLE;
    }
}

/*
* Convert parameter names to Newbase id, base on the local parameters in 'info'.
*
* Returns: >0: found a local param by that name (id returned)
*          0: no such param
*/
#if 0
FmiParameterName SQD_Tools::newbase_id( const char *name, const NA_Info *info ) {
    assert(name);

    // Is it a name used for some parameter in the particular data?
    //
    if (info) {
        for( vector<NA_Param>::const_iterator it= info->getParamsBegin();
            it != info->getParamsEnd();
            ++it ) {
//LOG_DEBUG( "SQD file lists param: %s ~ %s:%d", it->toString().c_str(), it->getNativeName().c_str(), it->getNativeId() );

            FmiParameterName e= (FmiParameterName) it->getNativeId();

            // Note: Virtual params s.a. "Z" (for both ':2' and ':3') or derivatives of ':19' and/or ':326'
            //       combos give native id 0. Skip them (they're covered by standard name comparisons).
            //
            if (!e) continue;

            if (it->getNativeName() == name) {
                return e;
            }
        }
    }

    return (FmiParameterName) 0;
}
#endif


/*
* Cut 's' to a name part (before a colon) and id (integer after colon).
*
* If there is no colon, 's' is the name part and 'id' is 0.
*
* Returns 'true' for valid syntax,
*         'false' for strings with colon but no unsigned integer after it.
*/
bool SQD_Tools::cut_at_colon( const char *s, string &name_part, FmiParameterName &id ) {

    // Colon in the name marks an id (s.a. "Lämpötila:4"). 
    //
    const char *colon= strchr(s,':');

    if (colon) {
        name_part= string(s,0,colon-s);
        char *endp;
        id= (FmiParameterName) strtol( colon+1, &endp, 10 );
        if (*endp || endp==colon+1) {
            return false;    // bad name
        }
    } else {
        name_part= s;
        id= (FmiParameterName) 0;
    }

    return true;
}



