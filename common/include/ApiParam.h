/*
* APIPARAM.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  6-Oct-10 AKa
*/
#ifndef APIPARAM_H
#define APIPARAM_H

#include "Tools.h"
#include "NA_Param.h"

class NA_Info;
class NA_Data;


/*
* Class for handling parameters at the API level (scalar only)
*/
class ApiScalarParam {
  public:
    ApiScalarParam( const char *name_=0 ) : name( name_ ? name_:"" ) { INVARIANT(); }

    const std::string &getName() const { return name; }

    operator bool() const { return name!=""; }

    NA_Param covered_by( const std::vector<NA_Param> &vec ) const;

  private:
    std::string name;
    
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) {
        (void)file; (void)line;
    }
#endif
};


/*
* Class for handling parameters at the API level (vectors and scalars)
*/
class ApiParam {
  public:
    ApiParam( const char *name_=0 );

    ApiParam( const ApiScalarParam &o ) : name( o.getName() ), a(o), b(), polar(false) { INVARIANT(); }

    bool is_2d( ApiScalarParam &a_, ApiScalarParam &b_, bool &polar_ ) const {
        a_= a;
        b_= b;
        polar_= polar;
        return b;
    }

    bool is_2d( bool &polar_ ) const { 
        ApiScalarParam aa,bb;
        return is_2d( aa, bb, polar_ );
    }

    operator bool() const { return a; }      // empty or not

    const std::string &toString() const { return name; }

    //string_or_null getUnitName() const;

    bool covered_by( const std::vector<NA_Param> &vec, NA_Param &na, NA_Param &nb, bool &polar_ ) const;

    bool covered_by( const std::vector<NA_Param> &vec ) const {
        NA_Param na, nb;
        bool polar_;
        return covered_by( vec, na,nb, polar_ );
    }

    static std::vector<std::string> convert_to_api( const std::vector<NA_Param> &vec, bool prefer_standard_names );
// 25-Oct-2011 PKi: Now needed by server too
//#ifdef METQU
    static std::vector<NA_Param> convert_to_native( const std::vector<ApiParam> &vec, const NA_Data *data = nullptr );
//#endif

    // 25-Oct-2011 PKi: Returns data's parameters with virtual parameters stripped off
    static std::vector<NA_Param> realParams( const NA_Data *data );

  private:
    // Note: Use of 'vector<ApiParam>' requires an assignment operator. The automatic one is okay, but
    //       we cannot have any 'const' members.
    
    // data members
    //
    /*const*/ std::string name;       // name of param as seen in the script (i.e. "T", "WS", "WIND", ..)

    /*const*/ ApiScalarParam a;       // scalar param | first vector component (x or abs)
    /*const*/ ApiScalarParam b;       // second vector component (y or deg)
    /*const*/ bool polar;             // polar or cartesian (xy)
    
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) {
        if (polar) {
            assert_invariant(b);
        }
        if (name!="") {
            assert_invariant(a);
        } else {
            assert_invariant(!a);
        }
    }
#endif
};

#endif
    // APIPARAM_H
