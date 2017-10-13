/*
* SQD_TOOLS.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Newbase specific tools; only to be included by 'SQD_*.cpp' files (since this 
* includes Newbase headers)
*
* Revised:  4-Oct-2010
*/
#ifndef SQD_TOOLS_H
#define SQD_TOOLS_H

#ifndef USE_NEWBASE
# error "This file shouldn't be included when compiling without Newbase."
#endif

#include "Tools.h"
#include "MatrixPos.h"
#include "NA_Level.h"
#include "NA_Data.h"

#include "LatLon.h"
#include "JDay.h"

class VectorMatrix;

#include <string>

#include "newbase/NFmiGrid.h"
#include "newbase/NFmiLevel.h"
#include "newbase/NFmiMetTime.h"
#include "newbase/NFmiParameterName.h"

class NFmiQueryData;

/*
*/
struct SQD_Tools {   // just as a namespace

    static NFmiLevel newbase_level( const NA_Level &lev );
    static NA_Level q3_level( FmiLevelType lt, double lv );

    static JDay mt2jd( const NFmiMetTime &mt );
    static NFmiMetTime jd2mt( const JDay &jd );
    
    static NA_Param::Unit unit_by_id( FmiParameterName e );

    //static FmiParameterName newbase_id( const char *name, const NA_Info *info= 0 );
    
    static std::string standard_param_name( FmiParameterName e );
    static FmiParameterName standard_param_id( const char *s );

    static bool latlon_dx_dy( const char *proj, const LatLon &ll, double &dx, double &dy );

    static LatLon convert( const NFmiPoint &p ) {
        return LatLon( p.X(), p.Y() );
    }
    
    static bool cut_at_colon( const char *s, std::string &name_part, FmiParameterName &e );

    static FmiParameterName param_id( const NA_Param &p ) {
        std::string ignore;
        FmiParameterName e;

        if (!cut_at_colon( p.getNativeName_().c_str(), ignore, e )) {
            return (FmiParameterName)0;
        } else {
            return e;
        }
    }
};

#endif
    // SQD_TOOLS_H
