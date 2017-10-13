/*
* APIPARAM.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  6-Oct-10 AKa
*
* Certain parameter names lead directly to other parameters:
*   UV      -> vector of U and V
*   WIND    -> vector of WS and WD
*   ...
*/
#include "ApiParam.h"

#ifdef USE_NEWBASE
# include "SQD_Tools.h"
#endif

#include <math.h>

using namespace std;


/*---=== Helpers ===---
*/

/*
* Does 'vec' have parameters with standard names 's1' and 's2'?
*/
static bool has_by_standard_name( const std::vector<NA_Param> vec, string s1, string s2= "" ) {
    assert(s1 != "");

    bool s1_match= false;
    bool s2_match= (s2=="");     // match if not given
    
    for( vector<NA_Param>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        string it_s= it->getStandardName();
        
        if (it_s==s1) {
            s1_match= true;
        } else if ((s2!="") && (it_s==s2)) {
            s2_match= true;
        }
        
        if (s1_match && s2_match) {
            return true;
        }
    }
    return false;
}


/*
* Match native parameter names.
*
* If 'USE_NEWBASE' is defined, the native names are interpreted as '[xxx][:nnn]' with optional
* name part and optional numeric trailer. Either name, id or both can cause a match.
*/
static bool native_match( const string &name_in_data, const string &name_to_match ) {

#ifndef USE_NEWBASE
    return name_in_data == name_to_match;   // straightforward, no id's
#else

    assert( name_in_data != "" );
    assert( name_to_match != "" );

    string a_name_part;
    FmiParameterName a_id;

    if (!SQD_Tools::cut_at_colon( name_in_data.c_str(), a_name_part, a_id )) {
        throw E_LOG_USAGE( "Bad parameter name (expecting 'xxx' or 'xxx:nnn'): '%s'", name_in_data.c_str() );
    }
    
    string b_name_part;
    FmiParameterName b_id;

    if (!SQD_Tools::cut_at_colon( name_to_match.c_str(), b_name_part, b_id )) {
        // 17-Oct-2011 PKi: Invalid name; do not throw (not catched), just return 'nonmatch'
        //
        // throw E_LOG_USAGE( "Bad parameter name (expecting 'xxx' or 'xxx:nnn'): '%s'", name_to_match.c_str() );

        return false;   // Invalid name
    }

    if ((b_name_part!="") && (a_name_part != b_name_part)) {
        return false;   // name part not as required
    }
    
    if (b_id && (a_id != b_id)) {
        return false;   // id part not as required
    }

    return true;    // either or both name and id match    
#endif
    // USE_NEWBASE
}


/*---=== ApiScalarParam ===---
*/

/*
* Returns the param within 'vec' that covers 'this', or empty 'NA_Param' if none.
*
* The function can be called either as a test or for getting a native param index to
* accessing the data.
*/
NA_Param ApiScalarParam::covered_by( const vector<NA_Param> &vec ) const {
    const static NA_Param NONE;     // no param ('operator bool' gives 'false')

    if (name=="") return NONE;     // empty param not covered - ever

    // Matching logic goes:
    //  1. if standard names match, it's a match
    //  2. if native name and/or id matches (both must match if given) it's a match
    //
    for( vector<NA_Param>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        string it_std_name= it->getStandardName();
        string it_native_name= it->getNativeName_();
        
        if (it_std_name == name) {
            return *it;    // standard name match
        }

        if (native_match( it_native_name, name )) {
            return *it;     // match by native name, native id (if SQD) or both
        }
    }
    return NONE;
}


/*---=== ApiParam ===---
*/

static const ApiScalarParam U( "U" );
static const ApiScalarParam V( "V" );
static const ApiScalarParam WS( "WS" );
static const ApiScalarParam WD( "WD" );

ApiParam::ApiParam( const char *name_ )
    : name( name_ ? name_:"" ), a(), b(), polar(false) { 

    if (name == "WIND") {
        a= WS;
        b= WD;
        polar= true;

    } else if (name=="UV") {
        a= U;
        b= V;
        polar= false;

    } else if (name_) {
        a= ApiScalarParam( name_ );
    }
    
    INVARIANT();
}


/*
* Given a set of native parameters, return the API level (including vector) params that can
* be provided out of these.
*/
vector<string> ApiParam::convert_to_api( const std::vector<NA_Param> &vec, bool prefer_standard_names ) {
    vector<string> ret;
    
    for( vector<NA_Param>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        ret.push_back( it->toString(prefer_standard_names) );
    }
    
    if (has_by_standard_name(vec,"U","V")) {
        ret.push_back( "UV" );
    }
    if (has_by_standard_name(vec,"WS","WD")) {
        ret.push_back( "WIND" );
    }

    return ret;
}


/*
* Given a set of 'ApiParam' parameters (including vectors), return the native params
* that are needed to provide these.
*/
// 25-Oct-2011 PKi: Now needed by server too
//#ifdef METQU
vector<NA_Param> ApiParam::convert_to_native( const std::vector<ApiParam> &vec, const NA_Data *data ) {

    // 25-Oct-2011 PKi: Take the params from data if not excplicitly given

    if (vec.size() < 1)
        return realParams(data);

    vector<NA_Param> ret;
    
    for( vector<ApiParam>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {
        assert(it->a);

        // 02-Sep-2011 PKi: Check for duplicate parameters. When creating a raw using template this
        //					avoids "expanding" vectors UV (when parameters contain U and V) and
        //					WIND (when parameters contain WS and WD) if the component scalars are
        //				    already included.
        //
        //					Check is made for all parameters (scalars too) to handle possible duplicate
        //				    parameter definitions in scripts too
        //
        // 25-Oct-2011 PKi: Ignore virtual parameters

        if (data)
        {
            bool mapped;
            data->mapParameter(NA_Param((it->a).getName().c_str(),NA_Param::UNIT_UNKNOWN_INTERPOLATABLE), true, &mapped);

            if (mapped)
                continue;
        }

        if (/* (! it->b) || */ (! has_by_standard_name(ret,(it->a).getName())))
        	ret.push_back( NA_Param( (it->a).getName().c_str() ) );

        if (it->b && (! has_by_standard_name(ret,(it->b).getName()))) {
            ret.push_back( NA_Param( (it->b).getName().c_str() ) );
        }
    }

    return ret;
}
//#endif

/*
* 25-Oct-2011 PKi: Returns data's parameters with virtual parameters stripped off
*/
vector<NA_Param> ApiParam::realParams( const NA_Data *data ) {
    vector<NA_Param> ret;
    vector<NA_Param> vec = (data ? data->getParams() : std::vector<NA_Param>());

    for( vector<NA_Param>::const_iterator it= vec.begin();
        it != vec.end();
        ++it ) {

        bool mapped;
        data->mapParameter(*it, true, &mapped);

        if (! mapped)
            ret.push_back( *it );
    }

    return ret;
}


/*
* Check if 'this' is covered by nartive parameters 'vec'.
*
* If returning 'true':
*   'na' is written the native param that covers 'a'
*   'nb' is written the native param that covers 'b' (if a vector param)
*   'polar' is set to 'true' if a vector is polar (otherwise false)
*/
bool ApiParam::covered_by( const vector<NA_Param> &vec, NA_Param &na, NA_Param &nb, bool &polar_ ) const {

    if (!a) {
        return false;   // empty param (no match)
    }
    
    na= a.covered_by( vec );
    if (!na) {
        return false;
    }

    if (b) {
        nb= b.covered_by( vec );
        if (!nb) {
            return false;
        }
        polar_= polar;
    } else {
        nb= NA_Param();
        polar_= false;
    }
    
    return true;
}




