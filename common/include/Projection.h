/*
* PROJECTION.H                        Copyright (c) 2010, Ilmatieteen laitos
*
* Created   22-Oct-10 AKa
* Revised   28-Oct-10 AKa
*/
#ifndef PROJECTION_H
#define PROJECTION_H

#include "Tools.h"
#include "LatLon.h"

#include <string>
#include <vector>

/*
* Abstract base class (interface) for particular projections.
*/
class Projection_provider {
  public:
    Projection_provider() {}
    virtual ~Projection_provider() {}

    virtual bool at( const LatLon &ll, double &dx, double &dy ) const = 0;
    virtual LatLon latlon( double dx, double dy ) const = 0;

    virtual Projection_provider *clone_() const = 0;

  private:
    Projection_provider( const Projection_provider & );               // not allowed
    Projection_provider & operator=( const Projection_provider & );   // -''-
    
    // no data fields
};

/*
* Programming level interface to projections (either SQD or Proj4 kind, via 'Projection_provider')
*/
class Projection {
  public:
    Projection() : proj(0), creation_str("") { INVARIANT(); }    // empty (no) projection
    Projection( const char *s ) throw(E_USAGE);
    Projection( const Projection & );
    ~Projection();

    // Used when the particular type of the underlying projection is known for sure 
    // (i.e. 'SQD_Data' reading in data)
    //
#ifdef USE_NEWBASE
    Projection( const char *s, const Projection_provider &pb ) : proj(pb.clone_()), creation_str(s ? s : "") {
        INVARIANT();
    }
#endif

    Projection & operator=( const Projection & );

    bool at( const LatLon &ll, double &dx, double &dy ) const {
        assert(proj);
        return proj->at( ll, dx, dy );
    }

    LatLon latlon( double dx, double dy ) const {
        assert(proj);
        return proj->latlon( dx, dy );
    }

    LatLon latlon( const MatrixPos &pos, const MatrixPos &gs ) const;

    std::string toString() const { return creation_str; }
    
    const static Projection NONE;

    operator bool() const { return proj != nullptr; }

    bool operator==( const Projection &o ) const {
        return toString() == o.toString();
    }

  private:
    // data members
    //
    // Note: Cannot make 'def_str' 'const' because we have an assignment operator.
    //
    Projection_provider *proj;      // nullptr for no projection ('Projection::NONE')
    std::string creation_str;       // for reporting to API in string form
        
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
        assert_invariant( proj ? (creation_str!="") : (creation_str=="") );
    }
#endif
};

#endif
    // PROJECTION_H
