/*
* VECTORMATRIX.H                        Copyright (c) 2009-10, Ilmatieteen laitos
*
* Revised:  20-Oct-2010 AKa
*/
#ifndef VECTORMATRIX_H
#define VECTORMATRIX_H

#include "LuaNew.h"

#include "Matrix.h"
#include "Vector.h"

#include <cmath>


/*---=== VectorMatrix ===---*
*
* Matrix of vectors.
*/
class VectorMatrix;

struct VectorMatrixBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "VectorMatrix"; }
    static const char *env_mode() { return nullptr; }
    static const LuaNew_ID & id() { return ID; }
    typedef VectorMatrix CAST_T;

  private:
    static int index( lua_State *L );
    static int newindex( lua_State *L );

    static int unm( lua_State *L );

  public:   // for 'MatrixBind::setup()'
    static int add( lua_State *L );
    static int sub( lua_State *L );
    static int mul( lua_State *L );
    static int div( lua_State *L );
    static int tostring( lua_State *L );
};

/*
* Matrix of vector values.
*
* Values are stored in two separate 'Matrix' objects residing within our
* Lua environment table (shaded from the outside world). This eases
* calculations (we can utilize Lua operations and their SSE optimizations)
* as well as reduces the number of constructor/destructor/memory copy
* calls required. This is also why the constructors need to be given the
* Lua state handle.
*
* Subvectors are stored either as X/Y components, or abs/deg pairs,
* depending on the constructor (which in turn depends on parameter type). 
* This provides accuracy; no unnecessary conversions between the two are 
* ever needed.
*/
class VectorMatrix : public ApiMatrix, public LuaNew<VectorMatrixBind> {

  public:
    typedef MatrixPos::xy_t xy_t;
    typedef MatrixPos::offset_t offset_t;

    VectorMatrix( lua_State *L, bool polar );
    VectorMatrix( lua_State *L, const Matrix &a, const Matrix &b, bool polar );
    ~VectorMatrix() {}

    Vector operator[]( offset_t n ) const;

    Vector operator[]( const MatrixPos &mi ) const {
        MatrixPos p= mi - getSize().getTop();    // move coordinates so that (0,0) is [0]
        xy_t x= p.getX();
        xy_t y= p.getY();
        xy_t xs= getSize().getXS();
        xy_t ys= getSize().getYS();
    
        if ((x<0) || (y<0) || (x>=xs) || (y>=ys)) {
            throw E_OUTSIDE( getSize(), mi );
        }
        return (*this)[ ((offset_t)y)*xs + x ];
    }

    /*virtual*/ void asString( std::ostream &out, int decimals ) const;

    const MatrixSize &getSize() const { 
        assert( m1 );     // detect if 'getSize()' is being used in constructor (not allowed)
        return m1->getSize();
    }

    MatrixPos getGridSize() const { 
        assert( m1 );     // detect if 'getGridSize()' is being used in constructor (not allowed)
        return m1->getGridSize();
    }

    bool isPolar() const { return polar; }

    // Return direct pointers to the submatrices (or nullptr if needs reading via Vector
    // iteration and '.getX()', '.getY()', '.getAbs()' or '.getDeg()')
    //
    const Matrix *getMX_() const { return polar ? nullptr : m1; }
    const Matrix *getMY_() const { return polar ? nullptr : m2; }
    const Matrix *getAbs_() const { return polar ? m1 : nullptr; }
    const Matrix *getDeg_() const { return polar ? m2 : nullptr; }

    const Matrix *getM1() const { return m1; }
    const Matrix *getM2() const { return m2; }

    void push_cartesian_x( lua_State *L, int b_index ) const { push_cartesian( L, b_index, true /*x*/ ); }
    void push_cartesian_y( lua_State *L, int b_index ) const { push_cartesian( L, b_index, false /*y*/ ); }

    /*virtual*/ const Projection &getProjection() const {
        return m1->getProjection();     // same as m2's
    }

    /*virtual*/ bool is_2d() const { return true; }

#ifdef METQU
    void operator=( const VectorMatrix &o );
    void operator=( const Vector &v );
#endif

  private:
    void init( lua_State *L );

    void push_cartesian( lua_State *L, int index_of_this, bool choose_x ) const;

  private:
    VectorMatrix();   // no such

    friend class VectorMatrixBind;

    Matrix *m1;  // fast pointer to Lua objects stored in env.table
    Matrix *m2;  // (no deletion on these, they are handled by Lua GC automatically!)

    unsigned m1_key, m2_key;    // keys of the kept regular Matrices (needed to push
                                // references to them)
    
    bool polar;  // true: 'm1' and 'm2' are abs and deg
                 // false: 'm1' and 'm2' are x and y

#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const { 
        assert_invariant( m1 && m2 && m1_key && m2_key );
        assert_invariant( m1->getSize() == m2->getSize() );
        assert_invariant( m1->getProjection() == m2->getProjection() );
    }
#endif
};


#endif
    // VECTORMATRIX_H

