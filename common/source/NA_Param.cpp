/*
* NA_PARAM.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  17-Oct-10 AKa
*/
#include "NA_Param.h"

#ifdef USE_NEWBASE
# include "SQD_Tools.h"
#endif

#include <string>
#include <map>

using namespace std;

/*
* Note: Don't use these in initialization of other globals; the order of initialization is undefined, and
*       these will only be 'there' once 'main()' starts.
*
*       'SQD_Tools.cpp' has a similar set for its own needs (for the above reason). AKa 15-Nov-10
*/
const NA_Param::Unit NA_Param::UNIT_FLOAT_NEAREST( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_NEAREST );
const NA_Param::Unit NA_Param::UNIT_UINT16_NEAREST( NA_Param::DATATYPE_UINT16, NA_Param::INTERPOLATE_NEAREST );

const NA_Param::Unit NA_Param::UNIT_MAX14_ENUM( NA_Param::DATATYPE_HALFBYTE, NA_Param::INTERPOLATE_NEAREST );
const NA_Param::Unit NA_Param::UNIT_MAX254_ENUM( NA_Param::DATATYPE_BYTE, NA_Param::INTERPOLATE_NEAREST );
const NA_Param::Unit NA_Param::UNIT_BOOL( NA_Param::DATATYPE_BOOL, NA_Param::INTERPOLATE_NEAREST );

const NA_Param::Unit NA_Param::UNIT_DEG( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR_DEG, "deg" );

const NA_Param::Unit NA_Param::UNIT_LON( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR_LON, "deg" );   // -179.99 .. 180.0
const NA_Param::Unit NA_Param::UNIT_LAT( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "deg" );   // -90..90 (no wrap-around)

const NA_Param::Unit NA_Param::UNIT_PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "%" );
const NA_Param::Unit NA_Param::UNIT_10PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "10%" );  // 0..10 (with fractions)
const NA_Param::Unit NA_Param::UNIT_100PRC( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "100%" ); // 0..1 (with fractions)
const NA_Param::Unit NA_Param::UNIT_1( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, "1" );   // anything (but "just numbers")

/*
* Unknown unit, and interpolation not allowed.
*/
const NA_Param::Unit NA_Param::UNIT_UNKNOWN_( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_UNKNOWN, NULL );

/*
* Unit used for results of mathematical operations.
*/
const NA_Param::Unit NA_Param::UNIT_UNKNOWN_INTERPOLATABLE( NA_Param::DATATYPE_FLOAT, NA_Param::INTERPOLATE_LINEAR, NULL );


/*---=== Standard param names to SQD file id mapping ===---
*/
#ifdef USE_NEWBASE

static struct { 
    const char * const name;        // standard name (i.e. 'T') 
    const FmiParameterName id;      // native id in SQD file
} standard_params[]= {
    { "P",      kFmiPressure },
#ifndef SQD_CASCADE_Z_ENABLED                    // 24-Oct-2011 PKi: Only if cascade not enabled
    { "Z",      SQD_PRIMARY_Z },                 //                  Instead of kFmiGeopHeight
#endif                                           //
    { "T",      kFmiTemperature },
    { "THETAW", kFmiPseudoAdiabaticPotentialTemperature },
    { "DP",     kFmiDewPoint },
    { "RH",     kFmiHumidity },
#ifndef SQD_CASCADE_W_ENABLED                    // 24-Oct-2011 PKi: Only if cascade not enabled
    { "W",      SQD_PRIMARY_W },                 //                  Instead of kFmiVerticalVelocityMMS
#endif                                           //
    { "RRCON",  kFmiPrecipitationConv },
    { "RRLAR",  kFmiPrecipitationLarge },
    { "CAPE",   kFmiCAPE },
    { "KIND",   kFmiKIndex },
    { "POP",    kFmiPoP },
    { "TKE",    kFmiTurbulentKineticEnergy },
    { "PSEUDOSATEL", kFmiRadiationNetTopAtmLW },
    { "LRAD",   kFmiRadiationLW },
    { "SRAD",   kFmiRadiationGlobal },
    { "VIS",    kFmiVisibility },
    { "AVIVIS",  kFmiAviationVisibility },
    { "VERVIS",  kFmiVerticalVisibility },
    { "MIST",    kFmiMist },

    //---
    // 'WeatherAndCloudiness' derivatives
    //
    { "N", kFmiTotalCloudCover },
    { "CL", kFmiLowCloudCover },
    { "CM", kFmiMediumCloudCover },
    { "CH", kFmiHighCloudCover },
    { "RR", kFmiPrecipitation1h },
    { "PRET", kFmiPrecipitationType },
    { "PREF", kFmiPrecipitationForm },
        //
        // 0: drizzle (tihku)
        // 1: rain (sade)
        // 2: sleet (nuoska)
        // 3: snow (lumi)
        // 4: freezing drizzle (jäätävä tihku)
        // 5: freezing rain (jäätävä sade)
        // 6: hail (rakeita)

    { "FOG", kFmiFogIntensity },
    { "THUND", kFmiProbabilityThunderstorm },
    { "HSADE", kFmiWeatherSymbol1 },
    { "HESSAA", kFmiWeatherSymbol3 },
    { "MiddleAndLowCloudCover", kFmiMiddleAndLowCloudCover },

    //---
    // 'TotalWindMS' derivatives
    //
    { "WD", kFmiWindDirection },
    { "WS", kFmiWindSpeedMS },

    // NOTE: GUST parameter is broken on SQD / Newbase. (TBD: check it out in detail, 
    //      at least report how exactly it's broken)
    //
    { "GUST", kFmiHourlyMaximumGust },
    
    // WVEC is essentially useless on q3/metqu, but we support it to cover all combo params
    // (derivatives of ':19' and ':326').
    //
    { "WVEC", kFmiWindVectorMS },

    { "U", kFmiWindUMS },
    { "V", kFmiWindVMS },
    
    { NULL, (FmiParameterName)0 }   // end marker
};

static map< string, FmiParameterName > id_by_name;
static map< FmiParameterName, string > name_by_id;

// 31-Oct-2011 PKi: Parameter mapping for virtual parameters
//
static map< string, virtualParameterName > id_by_virtualname;
static map< FmiParameterName, string > virtualname_by_id;

static const NA_Param virtual_Z( "Z", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE );
static const NA_Param virtual_W( "W", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE );

static struct {
    const string standardName;	// standard name (i.e. 'Z')
    const string nativeName;	// native name (i.e. 'Z:xxx')
    virtualParameterName v;		// primary/secondary parameter id:s
} virtual_params[]= {
#ifdef SQD_CASCADE_Z_ENABLED
    {
      virtual_Z.toString(true), virtual_Z.getNativeName_(),
      virtualParameterName(  (FmiParameterName)SQD_PRIMARY_Z, (FmiParameterName)SQD_SECONDARY_Z )
    },
#endif
#ifdef SQD_CASCADE_W_ENABLED
    {
      virtual_W.toString(true), virtual_W.getNativeName_(),
      virtualParameterName(  (FmiParameterName)SQD_PRIMARY_W, (FmiParameterName)SQD_SECONDARY_W )
    },
#endif
    {
      "", "",
      virtualParameterName( (FmiParameterName)0,(FmiParameterName)0 )
    }
};

static struct JustOnce_NA_Param {  // note: must have unique name (otherwise runtime problems, linker mixes the two structs)
  JustOnce_NA_Param() { 
    for( unsigned i=0; standard_params[i].id; i++ ) {
        const char *name= standard_params[i].name;
        FmiParameterName e= standard_params[i].id;

        // 'id_by_name[name]= ...' did not compile
        //
        id_by_name.insert( make_pair( name, e ) );
        name_by_id.insert( make_pair( e, name ) );
    }

    // 31-Oct-2011 PKi: Setup mapping for virtual parameters
    //
    for( unsigned i=0; virtual_params[i].v.primaryId; i++ ) {
        id_by_virtualname.insert( make_pair( virtual_params[i].nativeName, virtual_params[i].v ) );
        virtualname_by_id.insert( make_pair( virtual_params[i].v.primaryId, virtual_params[i].standardName ) );
        virtualname_by_id.insert( make_pair( virtual_params[i].v.secondaryId, virtual_params[i].standardName ) );
        virtualname_by_id.insert( make_pair( virtual_params[i].v.primaryId, virtual_params[i].nativeName ) );
        virtualname_by_id.insert( make_pair( virtual_params[i].v.secondaryId, virtual_params[i].nativeName ) );
    }
  }
} just_for_init;

/*
*/
FmiParameterName NA_Param::standard_param_native_id( const char *s ) {
    if (!s) return (FmiParameterName) 0;

    map< string, FmiParameterName >::const_iterator it= id_by_name.find( s );
    if (it != id_by_name.end()) {
        return it->second;    // the enum
    } else {
        return (FmiParameterName) 0;      // not a standard name
    }
}

/*
* Standard param name (s.a. "T" for id 4).
*
* Returns "" (empty string) if 'id' is not known, or does not have a standard name.
*/
static string standard_param_name( FmiParameterName e ) {
    
    map< FmiParameterName, string >::const_iterator it= name_by_id.find( e );
    if (it != name_by_id.end()) {
        return it->second;    // the name
    } else {
        return "";      // no standard name
    }
}

/*
* 31-Oct-2011 PKi: Parameter mapping for virtual parameters; returns primary/secondary
*                  parameter id:s for virtual parameter
*/
virtualParameterName NA_Param::virtual_param_native_id( const char *s ) {

    if (s)
    {
        map< string, virtualParameterName >::const_iterator it= id_by_virtualname.find( s );

        if (it != id_by_virtualname.end())
            return it->second;							// the id:s
    }

    return virtualParameterName( (FmiParameterName)0, (FmiParameterName)0 );	// not a virtual param
}

/*
* 31-Oct-2011 PKi: Virtual parameter mapping for native parameters; returns virtual parameter name
*                  for native parameter id
*
*                  Returns "" (empty string) if 'id' is not known, or does not have a virtual name.
*/
string NA_Param::native_id_virtual_param( FmiParameterName e ) {

    map< FmiParameterName, string >::const_iterator it= virtualname_by_id.find( e );
    if (it != virtualname_by_id.end()) {
        return it->second;    // the name
    } else {
        return "";      // no virtual name
    }
}
#endif
    // USE_NEWBASE


/*---=== NA_Param ===---
*/

/*
* Constructed from just a (standard) name
*
* Used by 'Test_Data.cpp' only.
*
* 20-Oct-2011 PKi: Now needed by server too (used to create virtual parameters)
*/
//#ifdef USE_TESTRAW
NA_Param::NA_Param( const char *s, const Unit &unit_, bool nonStd ) : std_name(), native_name(), unit(unit_)
{
# ifdef USE_NEWBASE
    if (strchr( s, ':' )) {
        native_name= s;
    
    } else {
        // Note: 'USE_NEWBASE' makes our invariant require a native name, and id. As long as we don't
        //       save these params to a file, the id need not be unique (in practise, of course it should).
        //
        // 20-Oct-2011 PKi: Currently used for Z and W; although not saving them into a file, embed name's
        //                  first character into the id to keep them unique.
        //
        //                  Native name is used to detect virtual parameters when needed.
        int pId = s[0];
        native_name= string_fmt( "%s:9000%03d", s, pId );
        if (! nonStd)
          std_name= s;
    }
# else
    native_name= s;     // anything goes
# endif

    INVARIANT();
}
//#endif


/*
* Constructor used when defining a set of native parameters to be used in create-from-the-scratch
* Raw file.
*
* 's' can be either:
*       "XXX:NNN" where both the native name and id are provided
*       "XXX" where a *standard* parameter name is provided
*
*       If 'USE_NEWBASE' is defined, one is not allowed to give unknown names (i.e. all params
*       must carry a nonzero id). 
*
*       If 'USE_NEWBASE' is not defined we can deal with any names as such (no need for id's).
*/
// 25-Oct-2011 PKi: Now needed by server too
//#ifdef METQU
NA_Param::NA_Param( const char *s ) : std_name(), native_name(), unit( UNIT_UNKNOWN_ )
{
    assert(s);

# ifdef USE_NEWBASE
    if (strchr(s,':')) {    // native name (i.e. "Lämpötila:4")
        string ignored;   // name part
        FmiParameterName id;

        if (!SQD_Tools::cut_at_colon( s, ignored, id )) {
            throw E_LOG_USAGE( "Bad parameter name (expecting 'xxx' or 'xxx:nnn'): '%s'", s );
        }
        (void)ignored;

        native_name= s;     // including colon and all
    
        // Set 'std_name' to a standard name based on the id
        //
        std_name= standard_param_name( id );

        // 12-Sep-2011 PKi: Set unit too (jira-178)
        unit= SQD_Tools::unit_by_id( id );
    } else {

        // If the name is "WS", "WD", "T" or some other standard name, act accordingly
        // (we even know how to set the unit for those).
        //
        FmiParameterName e= standard_param_native_id(s);

        if (e) {
            // Keep the provided standard name as native name, with the id (i.e. "N:79" instead of just ":79").
            // This is to give users of SQD data (i.e. via an editor application) a hint on what the
            // parameters carry.
            //
# ifdef CONFIG_APPLY_STANDARD_NAMES_TO_SQD
            native_name= string_fmt( "%s:%d", s, (int)e );
# else
            native_name= string_fmt( ":%d", (int)e );
# endif
            std_name= s;
            unit= SQD_Tools::unit_by_id( e );
        } else {
            throw E_LOG_USAGE( "Bad parameter name (not a standard name, no ':nnn' id tail): '%s'", s );
        }
    }
# else
    // Newbase not used - any native name is fine.
    //
    // TBD: We should have a list of standard names. If 's' is some of those (i.e. "WS", "WD", "T") we
    //      could store it as 'std_name' instead of (or in addition to) 'native_name'.
    //
    native_name= s;
# endif

    INVARIANT();
}
//#endif


/*
* Creation of parameters from SQD file.
*
* Note: SQD file always has a non-zero id on its parameters. They may or may not have a name part.
*/
#ifdef USE_NEWBASE
NA_Param::NA_Param( FmiParameterName e, const std::string &sqd_name_, const Unit &unit_, const std::string & interpolation_name_, const std::string & precision_ )
    : std_name( standard_param_name(e) ),   // i.e. "T" for id 4
      native_name( string_fmt( "%s:%d", sqd_name_.c_str(), (int)e ) ), 
      unit(unit_),
      interpolation_name(interpolation_name_),
      precision(precision_) {

    assert(e);  // SQD data always has an id on all params

    INVARIANT();
}
#endif


/*
*/
#ifdef USE_NEWBASE
NA_Param::NA_Param( FmiParameterName e, const std::string & interpolation_name_, const std::string & precision_ )
    : std_name( standard_param_name(e) ),   // i.e. "T" for id 4
      native_name( string_fmt( ":%d", (int)e ) ), 
      unit( SQD_Tools::unit_by_id(e) ),
      interpolation_name(interpolation_name_),
      precision(precision_) {

    assert(e);  // SQD data always has an id on all params

    INVARIANT();
}
#endif


/*
* What is presented of a parameter to the user, in i.e. 'r.params' (and 'r.native_params') list.
*/
string NA_Param::toString( bool prefer_standard_names ) const {

    if (prefer_standard_names && (std_name != "")) {
        return std_name;

    } else if (native_name != "") {
        return native_name;

    } else {
        assert( std_name != "" );
        return std_name;
    }
}

