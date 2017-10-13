/*
* MATRIXPOS.H                   Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Revised:  20-Oct-2010 AKa
*/
#ifndef MATRIXPOS_H
#define MATRIXPOS_H

#include "LuaNew.h"

#include <string>
#include <ostream>


/*---=== MatrixPos ===---*
*/
class MatrixPos;

struct MatrixPosBind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "MatrixPos"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef MatrixPos CAST_T;

  private:
    static int index( lua_State *L );
    static int add( lua_State *L );
    static int sub( lua_State *L );
    static int mul( lua_State *L );
    static int unm( lua_State *L );
    static int eq( lua_State *L );
    static int tostring( lua_State *L );
};

/*
* Any (x,y) position; also negative offsets are allowed, i.e. a step may be
* (-1,0).
*/
class MatrixPos : public LuaNew<MatrixPosBind> {
  public:
    typedef int xy_t;   // we need signed to allow offset calculations
 
    xy_t getX() const { return x; }
    xy_t getY() const { return y; }

    typedef unsigned long offset_t;      // even 32-bit allows for 16GB data blocks (enough)
    offset_t getN() const { return ((offset_t)x) * y; }

    MatrixPos() : x(0), y(0) {}
    MatrixPos( xy_t x_, xy_t y_ ) : x(x_), y(y_) {}

    bool operator==( const MatrixPos &o ) const { return (x==o.x) && (y==o.y); }
    bool operator!=( const MatrixPos &o ) const { return !(*this==o); }

    MatrixPos operator+( const MatrixPos &o ) const { return MatrixPos( x+o.x, y+o.y ); }
    MatrixPos operator-( const MatrixPos &o ) const { return MatrixPos( x-o.x, y-o.y ); }
    
    MatrixPos operator-() const { return MatrixPos(-x,-y); }

    // Iteration order is x loops tighter, y in outer rounds.
    //
    // Note: This should really be in 'MatrixIter' but having it here allows
    //      us to use 'MatrixPos' in ways that require ordering with the C++
    //      container libraries.    --AKa 13-Mar-2009
    //
    bool operator<( const MatrixPos &o ) const { return (y<o.y) || ((y==o.y) && (x<o.x)); }
    bool operator>( const MatrixPos &o ) const { return (y>o.y) || ((y==o.y) && (x>o.x)); }

    bool operator<=( const MatrixPos &o ) const { return (y<o.y) || ((y==o.y) && (x<=o.x)); }
    bool operator>=( const MatrixPos &o ) const { return (y>o.y) || ((y==o.y) && (x>=o.x)); }

    // 'true' for anything having an area (both 'x' and 'y' nonzero)
    // 'false' for ZERO, DX, DY (anything not having an area)
    //
    operator bool() const { return (x!=0) && (y!=0); }

    static int new_MatrixPos( lua_State *L );

    std::string asString() const;

    static const MatrixPos ZERO;  // (0,0)
    static const MatrixPos DX;    // (1,0)
    static const MatrixPos DY;    // (0,1)
    static const MatrixPos DXDY;  // (1,1)

  protected:
  // data members
    xy_t x,y;

  private:    
    friend class MatrixPosBind;
    
#ifndef NDEBUG        
    void _INVARIANT( const char *, unsigned ) const {}
#endif
};

inline std::ostream & operator << ( std::ostream &os, const MatrixPos &pos ) {
    os << pos.getX() << "," << pos.getY();      // xx,yy
    return os;
}

#endif
    // MATRIXPOS_H
