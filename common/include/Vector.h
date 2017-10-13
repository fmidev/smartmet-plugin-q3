/*
* VECTOR.H                        Copyright (c) 2009-10, Ilmatieteen laitos
*
* Vector values
*/
#ifndef VECTOR_H
#define VECTOR_H

#include "LuaNew.h"
#include "Tools.h"

#include <cmath>

class Point;    // at 'Contour.h'


/*---=== Vector ===---*
*
* Value of a 2D matrix.
*
* The value is stored either as X/Y components, or abs/deg pair. This provides
* best accuracy (no _unnecessary_ conversions).
*/
class Vector;

struct VectorBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Vector"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef Vector CAST_T;

  public:   // for 'VectorMatrixBind::sub'
    static int unm( lua_State *L );
  private:
    static int eq( lua_State *L );
    static int lt( lua_State *L );
    static int index( lua_State *L );
    static int add( lua_State *L );
    static int sub( lua_State *L );
    static int mul( lua_State *L );
    static int div( lua_State *L );
    static int tostring( lua_State *L );
};

/*
* Note: polar direction is internally stored as radians, range [0..2*pi), with
*       0 pointing NORTH and growing clockwise. This is for simplicity of 'getX()'
*       and 'getY()'.
*
* Note: Double precision is used in this class, though Newbase actually stores
*       values as float. This gives us some accuracy benefit and the operations
*       are equally fast (on modern CPUs) anyways. Also, Lua uses 'double' for
*       scalar numbers.
*/
class Vector : public LuaNew<VectorBind> {
  public:
    Vector( double x_or_abs, double y_or_deg, bool polar_ ) throw();
    
    Vector() throw() {}      // leave uninitialized (called when 'VectorMatrix' constructor does 'new')
    /*virtual*/ ~Vector() {}

    // Do operators here, since we need to manage polar/cartesian in a unified manner.
    //
    // Note: subtraction intentionally left out (not needed; Lua will handle it)
    //
    Vector operator - () const;
    Vector operator + ( const Vector& o ) const;
    Vector operator * ( double d ) const;

    // Note: Because our rads point NORTH (0,1) at 0 and C math lib points to (1,0)
    //       the use of 'cos' and 'sin' is essentially swapped (this is a 90 degree
    //       phase shift).
    //
    double getX() const { return (!polar) ? v1 : sin(v2)*v1; }
    double getY() const { return (!polar) ? v2 : cos(v2)*v1; }

    double getAbs() const { return polar ? v1 : hypot(v1,v2); }
    double getDeg() const;

    bool isPolar() const { return polar; }  // what is the native presentation

    // These are used by 'VectorMatrixBind::mul'
    //
    double getV1() const { return v1; }
    double getV2() const { return v2; }

    // A cartesian vector is NAN if either of its parts is. However, we allow polar
    // vectors to only carry strength or direction (and not be NAN as a vector).
    //
    bool isnan() const { 
        return polar ? (isnanf(v1) && isnanf(v2))
                     : (isnanf(v1) || isnanf(v2));
    }

    static const Vector Inf;

#ifndef NDEBUG
    static void selftest();
#endif

  private:
    double v1,v2;    // x,y or abs,rad (we store angles internally as radians)
    bool polar;

  private:    
    friend class VectorBind;
    
#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {
    
        // Cartesian: either 'v1' or 'v2' being NAN makes the whole vector (obviously) NAN.
        //
        // Polar:     'v1' and 'v2' be NAN independent of each other (i.e. we can have wind
        //            direction but not its speed)
        //
        if (polar) {
            assert_invariant( isnanf(v1) || (v1>=0.0) );
            assert_invariant( isnanf(v2) || (v2>=0.0 && v2 < 2.0*M_PI) );
        }
    }
#endif
};

#endif
    // VECTOR_H
