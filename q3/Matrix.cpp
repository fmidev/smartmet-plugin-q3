/*
* MATRIX.CPP                    Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#include "MemMatrix.h"
#include "VectorMatrix.h"
#include "Grid.h"
#include "Tools.h"
#include "SubMatrix.h"
#include "NA_Data.h"
#include "Proto.h"
#include "Projection.h"
#include "RegTools.h"

#include "newbase/NFmiGrid.h"
#include "newbase/NFmiLocation.h"

#ifdef __SSE__
# include "SSE.h"
#endif

#include <math.h>

#include <sstream>
#include <cstring>

using namespace std;

LuaNew_ID MatrixPosBind::ID;
LuaNew_ID MatrixIterBind::ID;

LuaNew_ID MatrixSizeBind::ID;

// Matrix <-- MemMatrix
//        <-- RawMatrix
//
// Only 'Matrix' is available in Lua; both kind of matrices are pushed using
// the same metatable, and are non-distinguishable from Lua.
//
LuaNew_ID MatrixBind::ID;

#include <float.h>

ApiMatrix::~ApiMatrix() {}      // needs to be somewhere


/*---=== Helpers ===---*/

/*
* Checks that matrices 'a' and 'b' are of same size. If not, gives a Lua error
* (does not return).
*/
static void CHECK_SAME_SIZE( lua_State *L, const Matrix &a, const Matrix &b ) {

    if (a.getSize() != b.getSize()) {
        luaL_error( L, "Matrices are of different size: %s != %s", 
                        a.getSize().asString().c_str(), b.getSize().asString().c_str() );
    }
}


/*
* Take the min/max of two values, either of which can be NAN. Scalar overrides NAN.
*
* Returns:  op( nan, nan )  -> nan
*           op( A, nan )    -> A
*           op( nan, B )    -> B
*           op( A, B )      -> min(A,B) | max(A,B)
*
* NOTE: 'std::min' and 'std::max' DO NOT return 'x' if 'nan' is their first argument
*       (there's a check for this in 'SSE.cpp'). 'max(10,nan)' does give 10.
*       The 'isnanf()' below takes care of this.
*/
static inline float max_safe( float a, float b ) {
    return isnanf(a) ? b /*value or NAN*/ : std::max<float>( a,b );
}
static inline float min_safe( float a, float b ) {
    return isnanf(a) ? b /*value or NAN*/ : std::min<float>( a,b );
}


/*
* Take the min/max of two values, FIRST OF WHICH can be NAN. NAN OVERRIDES SCALAR.
*
* Returns:  op( nan, B )  -> nan
*           op( A, B )    -> min(A,B) | max(A,B)
*
* This function is used when doing 'min()' or 'max()' concerning one or more matrices
* and one or more scalars. The matrices are min/max'ed separately, so are the scalars.
* This logic combines the two and NANS OVERRIDING SCALARS is vital to use this
* effectively with areamasks (i.e. keep NANs as they are if i.e. doing 'min(m,0.0)'). 
*/
static inline float max_safe_keep_nans( float a, float b ) {
    return isnanf(a) ? a /*NAN*/ : std::max<float>( a,b );
}
static inline float min_safe_keep_nans( float a, float b ) {
    LOG_DEBUG("%f %f", a, b );

    return isnanf(a) ? a /*NAN*/ : std::min<float>( a,b );
}


/*
* Decide which projection (if any) a result matrix will get.
*
* If only either 'a' or 'b' is given, projection comes from that.
* If both are given, their projection must be the same to be carried
* further.
*/
static const Projection &common_proj( const Matrix *a, const Matrix *b ) {
    assert( a || b );

    if (a) {
        const Projection &proj_a= a->getProjection();
        if (b) {
            // Both are matrices - use projection only if they are the same.
            //
            return (proj_a == b->getProjection()) ? proj_a : Projection::NONE;
        }
        return proj_a;
    } else {
        return b->getProjection();
    }
}


/*---=== MatrixPos ===---*/

const MatrixPos MatrixPos::ZERO(0,0);
const MatrixPos MatrixPos::DX(1,0);
const MatrixPos MatrixPos::DY(0,1);
const MatrixPos MatrixPos::DXDY(1,1);

/*
*/
string MatrixPos::asString() const {
    return string_fmt( "(%d,%d)", (int)x, (int)y );
}


/*
* uint= __index( pos_ud, "x"|"y" )
*/
int MatrixPosBind::index( lua_State *L ) {
    const MatrixPos &my= *MatrixPos::instance(L,1);

    const char *s= lua_tostring(L,2);
    if (s) switch(*s) {
        case 'x':
            if (!s[1]) {
                lua_pushinteger( L, my.x );
                return 1;
            }
            break;
        case 'y':
            if (!s[1]) {
                lua_pushinteger( L, my.y );
                return 1;
            }
            break;
    }
    luaL_error( L, "Bad index (for matrixpos): %s", s ? s : L_typename(2) );
    return 0;   // never
}

/*
* pos_ud= __add( pos_ud, any )
* pos_ud= __add( any, pos_ud )
*
* Called for i.e. 'i+step(1,0)'
*/
int MatrixPosBind::add( lua_State *L ) {
    MatrixPos* a= MatrixPos::instance(L,1);
    MatrixPos* b= MatrixPos::instance(L,2);

    if ((!a) || (!b)) {
        luaL_error( L, "Adding %s and %s not defined", L_typename(1), L_typename(2) );
    }

    // Do NOT modify the original pair; that is how Lua addition works and
    // it's important at least in iterators; the iteration variable must NOT 
    // be modified.
    //
    // Note: Resulting index is allowed to go outside of the ranges, or even
    //      negative (they will simply cause NAN results.
    //
    new(L) MatrixPos(a->x+b->x, a->y+b->y);

    return 1;
}

/*
* pos_ud= __sub( pos_ud, any )
* pos_ud= __sub( any, pos_ud )
*
* Called for i.e. 'i-step(1,0)'
*/
int MatrixPosBind::sub( lua_State *L ) {
    MatrixPos* a= MatrixPos::instance(L,1);
    MatrixPos* b= MatrixPos::instance(L,2);

    if ((!a) || (!b)) {
        luaL_error( L, "Subtracting %s and %s not defined", L_typename(1), L_typename(2) );
    }

    // (see comments in 'add()')
    //
    new(L) MatrixPos(a->x-b->x, a->y-b->y);

    return 1;
}

/*
* pos_ud= __mul( pos_ud, any )
* pos_ud= __mul( any, pos_ud )
*
* Called for i.e. '2*step(1,0)'
*/
int MatrixPosBind::mul( lua_State *L ) {
    int ud_index;
    MatrixPos* a= MatrixPos::instance( L, ud_index=1 );

    if (!a) {
        a= MatrixPos::instance( L, ud_index=2 );
        L_ASSERT(a);    // either parameter must be 'MatrixPos'
    }

    int num_index= (ud_index==1) ? 2:1;

    if (!lua_isnumber(L,num_index)) {
        luaL_error( L, "Multiplying MatrixPos and %s not possible.", L_typename(num_index) );
    }

    int v= lua_tointeger(L,num_index);

    new(L) MatrixPos( a->x*v, a->y*v );
    return 1;
}


/*
* pos_ud= __unm( pos_ud )
*/
int MatrixPosBind::unm( lua_State *L ) {
    MatrixPos &a= *MatrixPos::instance( L, 1 );

    new(L) MatrixPos( -a.x, -a.y );
    return 1;
}


/*
* bool= __eq( pos_ud|any, pos_ud|any )
*/
int MatrixPosBind::eq( lua_State *L ) {
    MatrixPos* a= MatrixPos::instance(L,1);
    MatrixPos* b= MatrixPos::instance(L,2);

    bool eq= a && b && (*a==*b);
    lua_pushboolean( L, eq );

    return 1;
}

/*
*/
void MatrixPosBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__add"); lua_pushcfunction(L,add);
    lua_settable(L,-3);

    lua_pushliteral(L,"__sub"); lua_pushcfunction(L,sub);
    lua_settable(L,-3);

    lua_pushliteral(L,"__mul"); lua_pushcfunction(L,mul);
    lua_settable(L,-3);

    // No need for 'div' for MatrixPos

    lua_pushliteral(L,"__unm"); lua_pushcfunction(L,unm);
    lua_settable(L,-3);

    lua_pushliteral(L,"__eq"); lua_pushcfunction(L,eq);
    lua_settable(L,-3);

    lua_pushliteral(L,"__tostring"); lua_pushcfunction(L,tostring);
    lua_settable(L,-3);
}

/*
* pos_ud= new_MatrixPos( int_x, int_y )
*
* Allows the end user to make a matrix position pair. Used for 'i+step(x,y)'
* for seeing surroundings of an iterated location.
*/
int MatrixPos::new_MatrixPos( lua_State *L ) {

    xy_t x= lua_tointeger(L,1);
    xy_t y= lua_tointeger(L,2);

    new(L) MatrixPos(x,y);
    return 1;    
}

/*
* string= mt.__tostring( obj )
*
* Output the object, when returned from a script.
*/
int MatrixPosBind::tostring( lua_State *L ) {
    const MatrixPos *p= MatrixPos::instance(L,1);
    if (!p) {
        throw E_LOG_USAGE( "Bad parameter: %s", L_typename(1) );    // internal bug or deliberate hack attempt
    }

    lua_pushfstring( L, "%d,%d", (int) p->getX(), (int) p->getY() );
    return 1;
}


/*---=== MatrixSize ===---*/

/*
*/
string MatrixSize::asString() const {
    return string_fmt( "(%d,%d,%d,%d)", (int)top.getX(), (int)top.getY(), (int)bottom.getX(), (int)bottom.getY() );
}

/*
* ...
*/
int MatrixSizeBind::index( lua_State *L ) {
    const MatrixSize &my= *MatrixSize::instance(L,1);
    (void)my;

    const char *s= lua_tostring(L,2);
    if (s) switch(*s) {
/**
        case 'blahblah':
            if (!s[1]) {
                lua_pushinteger( L, my.x );
                return 1;
            }
            break;
**/
    }
    luaL_error( L, "Bad index (for matrixsize): %s", s ? s : L_typename(2) );
    return 0;   // never
}

/*
* bool= __eq( size_ud|any, size_ud|any )
*/
int MatrixSizeBind::eq( lua_State *L ) {
    MatrixSize* a= MatrixSize::instance(L,1);
    MatrixSize* b= MatrixSize::instance(L,2);

    bool eq= a && b && (*a==*b);
    lua_pushboolean( L, eq );
    return 1;
}

/*
*/
void MatrixSizeBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__eq"); lua_pushcfunction(L,eq);
    lua_settable(L,-3);

    lua_pushliteral(L,"__tostring"); lua_pushcfunction(L,tostring);
    lua_settable(L,-3);
}


/*
* string= mt.__tostring( obj )
*
* Output the object, when returned from a script.
*/
int MatrixSizeBind::tostring( lua_State *L ) {
    const MatrixSize *p= MatrixSize::instance(L,1);
    if (!p) {
        throw E_LOG_USAGE( "Bad parameter: %s", L_typename(1) );    // internal bug or deliberate hack attempt
    }
    const MatrixSize &me= *p;

    MatrixPos top= me.getTop();
    MatrixPos bottom= me.getBottom();

    lua_pushfstring( L, "%d,%d,%d,%d", (int)top.getX(), (int)top.getY(), (int)bottom.getX(), (int)bottom.getY() );
    return 1;
}




/*---=== MatrixIter ===---*/

/*
* Note: The code below does not work for 'SubMatrix' matrices, where a matrix
*       does not have (0,0) as its top-left cell. Users shouldn't do 'for points()'
*       with submatrices. 
*
*       The way to "fix" this would be either to have also SubMatrix have (0,0)
*       in top-left cell (which is unintuitive; better have (0,0) as the 
*       "middle" cell of focus). Or to allow all matrices to span from (-x1,-y1)
*       to (x2,y2) which is elaborate.
*       -- AKa 13-May-2009
*/

/*
*/
MatrixIter::MatrixIter( const MatrixSize &size_ ) 
    : MatrixPos(size_.getTop()), size(size_) {}

/*
*/
void MatrixIterBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    MatrixPosBind::setup(L); 
    MatrixPosBind::ID.me2(ID);   // derive from 'MatrixPos'

    // no features of our own (but we want 'MatrixIter' to be distinguishable 
    // at the Lua/C++ API so we go through all this trouble).
}


/*
* [mi_ud, num|nan] = q3_points_iterator( m_ud [, mi_ud] )
* [mi_ud, pair_ud] = q3_points_iterator( m2_ud [, mi_ud] )
*
* A regular Lua iterator function (see Lua 5.1 manual section 2.4.5). The 
* 'points()' iterator will tie this to the application's visibility.
*
* From our view:
*   If 'mi' parameter is nil, start iterating from the beginning.
*   Otherwise, add up the 'mi' iterator (can be modified in place) and return
*   it + its connected value.
*   Once all the matrix is iterated, return nothing (ends iteration).
*
* The calling convention (above) is fixed by how Lua works.
*/
int MatrixIter::q3_points_iterator( lua_State *L ) {

    L_GROW(2);

    // Note: Expect 'm_ud' to be faulty, i.e. if user has given 'points(xx)'
    //      with some non-Matrix userdata.
    //
    const Matrix *m= Matrix::instance(L,1);
    const VectorMatrix *m2= 0;

    if (!m) {
        m2= VectorMatrix::instance(L,1);
        if (!m2) {
            luaL_error( L, "Bad parameter: expects matrix, got %s", L_typename(1) );
        }
    }

    MatrixIter *mi= MatrixIter::instance(L,2);
    if (!mi) {
        mi= new(L) MatrixIter( m ? m->getSize() : m2->getSize() );     // same userdata throughout the loop (of course)

    } else {
        // Modify the iterator in place and push a 2nd reference to it
        //
        ++(*mi);
        if (!mi->within()) {
            return 0;  // end of iteration
        }
        lua_pushvalue(L,2);     // 2nd ref of the modified 'mi'
    }

    if (m) {
        lua_pushnumber( L, (*m)[*mi] );
    } else {
        new(L) Vector( (*m2)[*mi] );
    }

    return 2;
}


/*---=== Matrix ===---*/

/*
* m_ud= add(m_ud|num, m_ud|num)
*/
int Matrix::add( lua_State *L ) {
    const Matrix* a= Matrix::instance(L,1);
    const Matrix* b= Matrix::instance(L,2);
    const Matrix* a_or_b= a ? a:b;
    assert(a_or_b);

    MemMatrix &c= * new(L) MemMatrix( a_or_b->getSize(), a_or_b->getUnit(), common_proj(a,b) );   // uninitialized
    offset_t n= c.getN();

    if (a && b) {
        CHECK_SAME_SIZE( L, *a, *b );

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_mm( __builtin_ia32_addps, c, a, b, i, n );
#endif
        // Do the rest of it, or whole thing if SSE couldn't be used
        //
        for( ; i<n; i++ ) { c[i]= (*a)[i]+(*b)[i]; }

    } else {
        int v_idx= a ? 2:1;
        float v= lua_tonumber(L, v_idx);
        if ((v==0.0F) && (!lua_isnumber(L, v_idx)))
            luaL_error( L, "Cannot add matrix and %s", L_typename(v_idx) );

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_mf( __builtin_ia32_addps, c, a_or_b, v, i, n );
#endif
        // Do the rest of it, or whole thing if SSE couldn't be used
        //
        for( ; i<n; i++ ) { c[i]= (*a_or_b)[i]+v; }
    }

    return 1;   // 'c' pushed
}

/*
* m_ud= sub(m_ud|num, m_ud|num)
*/
int Matrix::sub( lua_State *L ) {
    const Matrix* a= Matrix::instance(L,1);
    const Matrix* b= Matrix::instance(L,2);
    const Matrix* a_or_b= a ? a:b;
    assert(a_or_b);

    MemMatrix &c= *new(L) MemMatrix( a_or_b->getSize(), a_or_b->getUnit(), common_proj(a,b) );    // uninitialized
    offset_t n= c.getN();

    if (a && b) {
        CHECK_SAME_SIZE( L, *a, *b );

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_mm( __builtin_ia32_subps, c, a, b, i, n );
#endif
        for( ; i<n; i++ ) { c[i]= (*a)[i]-(*b)[i]; }
    } else {
        int v_idx= a ? 2:1;
        float v= lua_tonumber(L, v_idx);
        if ((v==0.0F) && (!lua_isnumber(L, v_idx)))
            luaL_error( L, "Cannot subtract matrix and %s", L_typename(v_idx) );

        offset_t i=0;
        if (a) {
#ifdef __SSE__
            SSE_OP_mf( __builtin_ia32_subps, c, a, v, i, n );
#endif            
            for( ; i<n; i++ ) { c[i]= (*a)[i]-v; }
        } else {
#ifdef __SSE__
            SSE_OP_fm( __builtin_ia32_subps, c, v, b, i, n );
#endif            
            for( ; i<n; i++ ) { c[i]= v-(*b)[i]; }
        }
    }

    return 1;   // 'c' pushed
}

/*
* m_ud= mul(m_ud|num, m_ud|num)
*/
int Matrix::mul( lua_State *L ) {
    const Matrix* a= Matrix::instance(L,1);
    const Matrix* b= Matrix::instance(L,2);
    const Matrix* a_or_b= a ? a:b;
    assert(a_or_b);

    MemMatrix &c= *new(L) MemMatrix( a_or_b->getSize(), a_or_b->getUnit(), common_proj(a,b) );
    offset_t n= c.getN();

    if (a && b) {
        CHECK_SAME_SIZE( L, *a, *b );

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_mm( __builtin_ia32_mulps, c, a, b, i, n );
#endif            
        for( ; i<n; i++ ) { c[i]= (*a)[i] * (*b)[i]; }
    } else {
        int v_idx= a ? 2:1;
        float v= lua_tonumber(L, v_idx);
        if ((v==0.0F) && (!lua_isnumber(L, v_idx)))
            luaL_error( L, "Cannot multiply matrix and %s", L_typename(v_idx) );

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_mf( __builtin_ia32_mulps, c, a_or_b, v, i, n );
#endif            
        for( ; i<n; i++ ) { c[i]= (*a_or_b)[i] * v; }
    }

    return 1;   // 'c' pushed
}

/*
* m_ud= div( m_ud )
*
* Note: Only '1/b' is currently needed and excercised; 'VectorMatrix' converts any divisions
*       'a/b' to 'a*(1/b)' and calls us only to get the reciprocal.
*/
int Matrix::reciprocal( lua_State *L ) {
    const Matrix* a= Matrix::instance(L,1);
    assert(a);

    MemMatrix &c= *new(L) MemMatrix( a->getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a->getProjection() );
    offset_t n= c.getN();

    // Note: We don't use SSE here, because checking for zeros (in divisor) would
    //      be time-taking, and because division by matrix is rare, anyhow.
    //
    for( offset_t i=0; i<n; i++ ) { 
        // Using 'double' to get most accuracy in the division itself (not sure if
        // that matters)
        //
        double tmp= (*a)[i];
        c[i]= (tmp!=0.0f) ? (1.0/tmp) : INFINITY_F;
    }

    return 1;   // 'c' pushed
}


/*
* m_ud= unm(m_ud)
*/
int Matrix::unm( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );    // always retains the param (& unit)
    offset_t n= c.getN();

    offset_t i=0;
#ifdef __SSE__
    // There is no unary minus in '__builtin_ia32_*', right?
    //
    SSE_OP_mf( __builtin_ia32_mulps, c, &a, -1.0f, i, n );
#endif            
    for( ; i<n; i++ ) { c[i]= -a[i]; }

    return 1;   // 'c' pushed
}

/*
* m_ud= unm_deg(m_ud)
*
* Negate a polar coordinate system matrix, by adding 180 degrees and truncating to 
* [0..360) range.
*/
int Matrix::unm_deg( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );    // always retains the param (& unit)
    offset_t n= c.getN();
    for( offset_t i=0; i<n; i++ ) { 
        c[i]= fmodf( a[i]+180.0f, 360.0f );
    }

    return 1;   // 'c' pushed
}


/*
* m_ud= mod( m_ud, num )
*
* Used to force return values into certain range, i.e. '(HIR_WD - EC_WD) % 360'
* makes sure returned directions are within 0..359.99...
*/
int Matrix::mod( lua_State *L ) {
    const Matrix *a= Matrix::instance(L,1);
    float b= (float)lua_tonumber(L,2);

    if (!a) {
        luaL_error( L, "Cannot mod %s by a matrix", L_typename(1) );
    }
    else if ((b==0.0) && (!lua_isnumber(L,2))) {
        luaL_error( L, "Cannot mod matrix by %s", L_typename(2) );
    }

    MemMatrix &c= *new(L) MemMatrix( a->getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a->getProjection() );
    offset_t n= c.getN();

    if (b==0.0) {  /* mod by 0 gives nan */
        c.fill( NAN );
    } else {
        for( offset_t i=0; i<n; i++ ) { 
            // The Lua formula for mod is: "o1 - floor(o1/o2)*o2"
            //
            float ai= (*a)[i];
            c[i]= ai - floorf(ai/b) * b;
        }
    }

    return 1;   // 'c' pushed
}


/*
* m_ud= pow( m_ud, num )
*
* Optimized if calculating a square root (Lua 'math.sqrt()' takes here).
*/
int Matrix::pow( lua_State *L ) {
    const Matrix *a= Matrix::instance(L,1);
    float b= (float)lua_tonumber(L,2);

    if (!a) {
        // Since we came here, the 2nd param must be a matrix.
        //
        luaL_error( L, "Cannot power %s by a matrix", L_typename(1) );
    }
    else if ((b==0.0) && (!lua_isnumber(L,2))) {
        luaL_error( L, "Cannot power matrix by %s", L_typename(2) );
    }

    MemMatrix &c= *new(L) MemMatrix( a->getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a->getProjection() );
    offset_t n= c.getN();

    if (b==0.5) {  /* square root can do SSE optimizations */
        offset_t i=0;
#ifdef __SSE__
        SSE_OP_m( __builtin_ia32_sqrtps, c, a, i, n );
#endif            
        for( ; i<n; i++ ) { c[i]= sqrtf((*a)[i]); }
    } else {
        for( offset_t i=0; i<n; i++ ) { c[i]= powf((*a)[i],b); }
    }

    return 1;   // 'c' pushed
}

/*
* Matrix versions of 'sin', 'cos' etc. math operations (everything regular Lua
* 'math.*' has) so that they'll work with matrices (if possible). The 'q3.lua'
* code defines actual user space functions, splitting to us or the regular
* Lua math library, based on parameter type.
*/

/*
* m_ud= abs(m_ud)
*/
int Matrix::q3_abs( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );
    offset_t n= c.getN();

    offset_t i=0;
    
    // We can do 'abs' by 'sqrt(x*x)' in SSE (is it worth it?)
    //
#ifdef __SSE__
    SSE_abs( c, a, i, n );
#endif
    for( ; i<n; i++ ) { c[i]= fabsf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= ceil(m_ud)
*/
int Matrix::q3_ceil( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );
    offset_t n= c.getN();

    // There is no SSE for 'ceil()' is there?
    
    for( offset_t i=0; i<n; i++ ) { c[i]= ceilf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= cos(m_ud)
*/
int Matrix::q3_cos( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a.getProjection() );
    offset_t n= c.getN();
    
    for( offset_t i=0; i<n; i++ ) { c[i]= cosf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= floor(m_ud)
*/
int Matrix::q3_floor( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );
    offset_t n= c.getN();

    // There is no SSE for 'floor()' is there?
    
    for( offset_t i=0; i<n; i++ ) { c[i]= floorf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= fmod(m_ud, num)
*/
int Matrix::q3_fmod( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);
    float b= (float)lua_tonumber(L,2);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );    // retains the unit
    offset_t n= c.getN();

    offset_t i=0;
#ifdef __SSE__
    // '__builtin_ia32_rcpps' could be used to get recipropal of 'a' and 'b'
#endif
    for( ; i<n; i++ ) { c[i]= fmodf(a[i],b); }

    return 1;   // 'c' pushed
}

/*
* m_ud= log(m_ud)
*/
int Matrix::q3_log( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a.getProjection() );
    offset_t n= c.getN();

    for( offset_t i=0; i<n; i++ ) { c[i]= logf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= log10(m_ud)
*/
int Matrix::q3_log10( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a.getProjection() );
    offset_t n= c.getN();

    for( offset_t i=0; i<n; i++ ) { c[i]= log10f(a[i]); }

    return 1;   // 'c' pushed
}


/*
* Returns the maximum value (scalar) within a matrix.
*/
float Matrix::reduce_max() const {

    /* Important to have initial value non-NAN. Use of SSE minmax operations 
    * relies on not having a NAN here.
    */
    float v= -INF_F;
    offset_t i=0; 
    offset_t n= getN();
#ifdef __SSE__
    SSE_OP_minmax_1( __builtin_ia32_maxps, v, this, i, n, std::max<float> );
#endif
    for( ; i<n; i++ ) {
        // Can use regular 'std::max<float>' since the FIRST param is non-NAN
        //
        v= std::max<float>( v, (*this)[i] );
    }
    return (v > -INF_F) ? v : NAN;
}


/*
* Returns the minimum value (scalar) within a matrix.
*/
float Matrix::reduce_min() const {

    /* Important to have initial value non-NAN. Use of SSE minmax operations 
    * relies on not having a NAN here.
    */
    float v= INF_F;
    offset_t i=0;
    offset_t n= getN();
#ifdef __SSE__
    SSE_OP_minmax_1( __builtin_ia32_minps, v, this, i, n, std::min<float> );
#endif
    for( ; i<n; i++ ) {
        // Can use regular 'std::min<float>' since the FIRST param is non-NAN
        //
        v= std::min<float>( v, (*this)[i] );
    }
    return (v < INF_F) ? v : NAN;
}


/*
* Helper function for 'reduce_sum'
*/
static void reduce_sum_safe( float v, float &sum, unsigned &used ) {
    if (!isnanf(v)) {
        sum += v;
        used++;
    }
}

/*
* Returns the sum (or average if 'avg_mode' is true) of all non-NAN values of 
* a matrix (NAN if no values at all).
*/
float Matrix::reduce_sum( bool avg_mode ) const {
    float sum= 0.0F;
    unsigned used=0;
    offset_t n= getN();
    offset_t i=0;

#ifdef __SSE__
    SSE_reduce_sum( *this, sum, used, i, n, reduce_sum_safe );
#endif
    for( ; i<n; i++ ) {
        reduce_sum_safe( (*this)[i], sum, used );
    }
    return used ? (avg_mode ? (sum/used) : sum) : NAN;
}

/*
* Helper function for 'reduce_count'
*/
static void reduce_count_safe( float v, unsigned &used ) {
    if (!isnanf(v)) {
        used++;
    }
}

/*
* Returns the count of all non-NAN values of a matrix.
*/
unsigned Matrix::reduce_count() const {
    unsigned used=0;
    offset_t n= getN();
    offset_t i=0;

#ifdef __SSE__
    SSE_reduce_count( *this, used, i, n, reduce_count_safe );
#endif
    for( ; i<n; i++ ) {
        reduce_count_safe( (*this)[i], used );
    }
    return used;
}


/*
* matrix_ud = max( matrix_ud, matrix_ud|num )
* num|NAN = max( matrix_ud )
*
* Returns a matrix with memberwise maximum of given values, or the biggest
* value in the matrix (if only one parameter).
*/
int Matrix::q3_max( lua_State *L ) {
    const Matrix *a= Matrix::instance(L,1);
    L_ASSERT(a);

    if (lua_gettop(L)==1) {
        lua_pushnumber( L, a->reduce_max() );
        return 1;
    }

    MemMatrix &c= *new(L) MemMatrix(*a);    // clone of a (data, unit and projection also)
    offset_t n= c.getN();

    const Matrix* b= Matrix::instance(L,2);
    if (b) {
        if (b->getSize() != c.getSize()) {
            luaL_error( L, "Matrices are not the same size" );
        }
        
        // In order for the returned matrix to retain a projection, all component matrices
        // must have had the same one.
        //
        if (b->getProjection() != c.getProjection()) {
            c.scrap_projection();
        }

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_minmax( __builtin_ia32_maxps, c, b, i, n, max_safe );
#endif
        for( ; i<n; i++ ) { c[i]= max_safe(c[i],(*b)[i]); }

    } else if (lua_isnumber(L,2)) {
        float v= lua_tonumber(L,2);
        if (isnanf(v)) {
            // 'c' remains unchanged
        } else {
            // 'max()' between a matrix and a (non-NAN) scalar; apply the scalar to any
            // non-NAN members of the matrix, but DO NOT change NAN members. This behaviour
            // is VITAL i.e. for processing areamasks (same applies to 'min()').
            //
            // Note that this behaviour differs from what we want between matrix members
            // (if doing 'min()' between two matrices a number will overcome a NAN).
            //
            offset_t i=0;
#ifdef __SSE__
            SSE_OP_minmax_scalar( __builtin_ia32_maxps, c, v, i, n, max_safe_keep_nans );
#endif
            for( ; i<n; i++ ) { c[i]= max_safe_keep_nans(c[i],v); }
        }
    } else {
        luaL_error( L, "Cannot calculate max of matrix and %s", L_typename(2) );
    }
    return 1;   // pushed one
}

/*
* matrix_ud = min( matrix_ud, matrix_ud|num )
* num|NAN = min( matrix_ud )
*
* Returns a matrix with memberwise minimum of given values, or the least
* value in the matrix (if only one parameter).
*/
int Matrix::q3_min( lua_State *L ) {
    const Matrix *a= Matrix::instance(L,1);
    L_ASSERT(a);

    if (lua_gettop(L)==1) {
        lua_pushnumber( L, a->reduce_min() );
        return 1;
    }

    MemMatrix &c= *new(L) MemMatrix(*a);    // clone of a (data, unit and projection also)
    offset_t n= c.getN();

    const Matrix* b= Matrix::instance(L,2);
    if (b) {
        if (b->getSize() != c.getSize()) {
            luaL_error( L, "Matrices are not the same size" );
        }
        
        if (b->getProjection() != c.getProjection()) {
            c.scrap_projection();   // -> see 'q3_max' for comments
        }

        offset_t i=0;
#ifdef __SSE__
        SSE_OP_minmax( __builtin_ia32_minps, c, b, i, n, min_safe );
#endif
        for( ; i<n; i++ ) { c[i]= min_safe( c[i], (*b)[i] ); }

    } else if (lua_isnumber(L,2)) {
        float v= lua_tonumber(L,2);
        if (isnanf(v)) {
            // 'c' remains unchanged
        } else {
            // (see comments in 'q3_max()')
            //
            offset_t i=0;
#ifdef __SSE__
            SSE_OP_minmax_scalar( __builtin_ia32_minps, c, v, i, n, min_safe_keep_nans );
#endif
            // Can use regular 'std::min<float>' since the FIRST param is non-NAN
            //
            for( ; i<n; i++ ) { 
                //float ci_was= c[i];
                c[i]= min_safe_keep_nans( c[i], v ); 
                //LOG_DEBUG("%f %f -> %f", ci_was, v, c[i]); 
            }
        }
    } else {
        luaL_error( L, "Cannot calculate min of matrix and %s", L_typename(2) );
    }

    return 1;   // pushed one
}

/*
* m_integral_ud, m_fractions_ud= modf(m_ud)
*/
int Matrix::q3_modf( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    // 'modf' does retain the unit, for both returned values (the measurement stick does not change)
    //
    MemMatrix &c= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );
    MemMatrix &d= *new(L) MemMatrix( a.getSize(), a.getUnit(), a.getProjection() );
    offset_t n= c.getN();

    for( offset_t i=0; i<n; i++ ) { 
        d[i]= modff(a[i], &c[i] /*integer part*/);
    }

    return 2;   // 'c' and 'd' pushed
}

/*
* m_ud= sin(m_ud)
*/
int Matrix::q3_sin( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a.getProjection() );
    offset_t n= c.getN();

    for( offset_t i=0; i<n; i++ ) { c[i]= sinf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* m_ud= tan(m_ud)
*/
int Matrix::q3_tan( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);

    MemMatrix &c= *new(L) MemMatrix( a.getSize(), NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a.getProjection() );
    offset_t n= c.getN();
    
    for( offset_t i=0; i<n; i++ ) { c[i]= tanf(a[i]); }

    return 1;   // 'c' pushed
}

/*
* matrix_ud = sum( matrix_ud|nil, matrix_ud|nil [, ...] )
* num|NAN = sum( matrix_ud )
*
* matrix_ud = avg( matrix_ud|nil, matrix_ud|nil [, ...] )
* num|NAN = avg( matrix_ud )
*
* Sums up a number of matrices point-wise, or sums up the elements of a single
* matrix. Serves also for average calculation (the code is so similar, we
* differentiate with an upvalue).
*
* Unlike in addition (+ operator), NAN values do not cause the result of that
* particular location to be NAN (i.e. this is not the same as addition).
*
* Upvalues:
*   1: 'true' for calculating average (false or non-existing for sum)
*/
int Matrix::q3_sum_or_avg( lua_State *L ) {
    proto( L, "[Matrix],..." );

    const bool avg_mode= lua_toboolean( L, lua_upvalueindex(1) );

    unsigned args= lua_gettop(L);

    if (args==1) {
        const Matrix *a= Matrix::instance(L,1);
        if (a) {
            lua_pushnumber( L, a->reduce_sum(avg_mode) );
            return 1;
        }
    }

    /*
    * Use of 'MemMatrix' for 'avg_count' (and not 'std::vector<unsigned>' or something)
    * is intentional. Above all this frees us from having to worry who releases the
    * array if a Lua error happens.
    */
    MemMatrix *c= 0;           // need size of matrix before creating this
    MemMatrix *avg_count= 0;   // count of values used for summing 'c' (for 'avg_mode' only)

    for( unsigned i=1; i<=args; i++ ) {
        const Matrix *a= Matrix::instance(L,i);
        if (!a) {
            if (lua_isnil(L,i)) continue;   // skip
            L_ASSERT(false);  // 'proto()' should have tracked it
        }

        if (!c) {
            // First matrix: create result matrices
            //
            if (avg_mode) {
                // Init the whole 'avg_count' with 1 (even areas where 'c' carries NAN).
                // This matrix is only temporary, so no need for setting projection.
                //
                avg_count= new(L) MemMatrix( a->getSize(), 1.0f, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, nullptr );
            }
            c= new(L) MemMatrix(*a);    // clone of a (data, unit and projection also)
            
        } else {
            // Second and so on... gather the data
            //
            CHECK_SAME_SIZE( L, *a, *c );

            if (a->getProjection() != c->getProjection()) {
                c->scrap_projection();
            }

            offset_t n= c->getN();

            for( offset_t j=0; j<n; j++ ) { 
                float av= (*a)[j];
                float cv= (*c)[j];
                
                if (!isnanf(av)) {
                    if (isnanf(cv)) {
                        (*c)[j]= av;    // first value in this slot
                        assert( (*avg_count)[j] == 1.0f );  // already right
                    } else {
                        (*c)[j] += av;  // adding to already used slot
                        if (avg_count) ++((*avg_count)[j]);
                    }
                }
            }
        }
    }

    if (!c) luaL_error( L, "no parameters (matrix expected)" );

    //  [-1]: 'c' matrix (divident; NAN at non-existent places)
    // If 'avg_mode': 
    //  [-2]: 'avg_count' matrix (divisor; all values >= 1.0)

    if (avg_count) {
        c->div_by_nonzero( *avg_count );

        // Letting 'avg_count' remain on the Lua stack; it will be cleaned out
        // automatically.
    }
    
    return 1;   // 'c' pushed
}


/*
* matrix_ud = count( matrix_ud|nil, matrix_ud|nil [, ...] )     DISABLED
* uint = count( matrix_ud )
*
* Counts all non-NAN values of argument matrices.
*
* Like 'min', 'max', 'sum' and 'avg', counts the values location-wise if there
* are multiple parameters, or counts the values of the one matrix if there is only one.
*
* NOTE: The multiple params version is NOT BEING USED; 'utility.lua' defines another
*       kind of behaviour for 'count(...)'.
*/
#if 1
int Matrix::q3_count( lua_State *L ) {
    proto( L, "Matrix" );

    const Matrix &a= *Matrix::instance(L,1);

    lua_pushinteger( L, a.reduce_count() );
    return 1;
}
#else
int Matrix::q3_count( lua_State *L ) {
    proto( L, "[Matrix],..." );
    unsigned args= lua_gettop(L);

    if (args==1) {
        const Matrix *a= Matrix::instance(L,1);
        if (a) {
            lua_pushinteger( L, a->reduce_count() );
            return 1;
        }
    }

    /*
    * Use of 'MemMatrix' for 'count' (and not 'std::vector<unsigned>' or something)
    * is intentional. Above all this frees us from having to worry who releases the
    * array if a Lua error happens.
    */
    MemMatrix *c= 0;           // need size of matrix before creating this

    for( unsigned i=1; i<=args; i++ ) {
        const Matrix *a= Matrix::instance(L,i);
        if (!a) {
            if (lua_isnil(L,i)) continue;   // skip
            L_ASSERT(false);  // 'proto()' should have caught it
        }

        if (!c) {
            // First matrix: create result matrices
            //
            c= new(L) MemMatrix( a->getSize(), 0.0f, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a->getProjection() );

        } else {
            // Second and so on... gather the data
            //
            CHECK_SAME_SIZE( L, *a, *c );

            if (a->getProjection() != c->getProjection()) {
                c->scrap_projection();
            }
        }

        // Increment the value in 'c' for all positions where 'a' has non-NAN
        //
        offset_t n= c->getN();

        for( offset_t j=0; j<n; j++ ) { 
            if (!isnanf((*a)[j])) {
                (*c)[j] += 1.0f;
            }
        }
    }

    if (!c) luaL_error( L, "no parameters (matrix expected)" );

    return 1;   // 'c' pushed
}
#endif


/*
* matrix_ud = set( matrix_ud [, ...] )
*
* Sets each position of returned matrix to the *last* non-nan value in the parameter
* matrices.
*
* This can be used for collecting values i.e. when going through levels.
*/
int Matrix::q3_set( lua_State *L ) {
    proto( L, "Matrix, ..." );

    unsigned args= lua_gettop(L);

    MemMatrix *c= 0;           // need size of matrix before creating this

    for( unsigned i=1; i<=args; i++ ) {
        const Matrix *a= Matrix::instance(L,i);
        L_ASSERT(a);    // 'proto()' should have caught it

        if (!c) {
            // First matrix: create result matrices
            //
            c= new(L) MemMatrix( a->getSize(), NAN, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, a->getProjection() );

        } else {
            // Second and so on... gather the data
            //
            CHECK_SAME_SIZE( L, *a, *c );

            if (a->getProjection() != c->getProjection()) {
                c->scrap_projection();
            }
        }

        // Set the value of 'c' in all positions where 'a' has non-NAN
        //
        // NOTE: This could be optimized with SSE operations, most likely.
        //
        offset_t n= c->getN();

        for( offset_t j=0; j<n; j++ ) { 
            float v= (*a)[j];
            if (!isnanf(v)) {
                (*c)[j] = v;
            }
        }
    }

    if (!c) luaL_error( L, "no parameters (matrix expected)" );

    return 1;   // 'c' pushed
}


/*---=== Matrix methods ===---*/

/*
* bool= has_missing(m_ud)
*/
int Matrix::has_missing( lua_State *L ) {
    const Matrix &a= *Matrix::instance(L,1);
    MatrixPos::offset_t n= a.getN();
    bool found;
    
#ifdef __SSE__
    const float *a_data= a.getData();      // 0 if not SSE capable
    if (a_data) {
        found= SSE_hasnan( a_data, n );
        goto GOT_IT;
    }
#endif

    // Plain mode (no SSE or matrix does not support it)
    //
    found= false;
    for( offset_t i=0; i<n; i++ ) { 
        if (isnanf(a[i])) { found= true; break; }
    }
#ifdef __SSE__
GOT_IT:
#endif
    lua_pushboolean(L,found);
    return 1;
}

/*
* pos_ud= __index( m_ud, "top" )     -- m.top
* pos_ud= __index( m_ud, "size" )    -- m.size
* [str]= __index( m_ud, "unit" )     -- m.unit
* [str]= __index( m_ud, "projection" )  -- m.projection
* [grid_ud]= __index( m_ud, "grid" )   -- m.grid
* num|NAN= __index( m_ud, pos_ud )   -- m[pos]
* num|NAN= __index( m_ud, latlon_ud )
* { num|NAN [, ...] }= __index( m_ud, { latlon_ud [, ...] } )
*
* Returns NAN for values outside of the matrix (i.e. 'i-size(1,0)')
*/
int MatrixBind::index( lua_State *L ) {
    const Matrix &m= *Matrix::instance(L,1);

    switch( lua_type(L,2) ) {
        case LUA_TSTRING: {
            const char *s= lua_tostring(L,2);
            assert(s);
            
            // Note: it is intentional to push 'MatrixPos' and not a 'MatrixSize'
            //       mostly because the script level is otherwise not in touch
            //       with the size, at all.
            //
            if (strcmp(s,"top")==0) {
                new(L) MatrixPos( m.getSize().getTop() );
                return 1;
            }
            else if (strcmp(s,"size")==0) {
                new(L) MatrixPos( m.getSize().getXS(), m.getSize().getYS() );
                return 1;
            }
            else if (strcmp(s,"level")==0) {
            	// For ground level data getLevelTypeStr returns an empty string; nothing is returned
            	//
            	string lt = m.getLevelTypeStr();

            	if (!lt.empty()) {
                	lua_newtable( L );
                	lua_pushstring( L, lt.c_str() );

                	double lv = m.getLevelValue();

                	// hpa=nn.n, hybrid=nn or height=nn.n

                	if (lt != "hybrid")
                		lua_pushnumber( L, lv );
                	else {
                		lua_pushinteger( L, (int) (lv + 0.01));
                	}

                	lua_settable( L, -3 );

                    return 1;
            	}

                return 0;
            }
            else if (strcmp(s,"param")==0) {
            	if (!m.getParamStr().empty()) {
            		lua_pushstring( L, m.getParamStr().c_str() );
            		return 1;
            	}

                return 0;
            }
            else if (strcmp(s,"unit")==0) {
                lua_pushstring( L, m.getUnitName().c_str() );    // may be nullptr
                return 1;
            }
            else if (strcmp(s,"projection")==0) {
                lua_pushstring( L, m.getProjection().toString().c_str() );
                return 1;
            }
            else if (strcmp(s,"grid")==0) {
                unsigned key= m.getGridKey();
                if (LuaNew_base::push_alive( L, 1, key )) {
                    L_ASSERT( Grid::instance(L,-1) );
                    return 1;
                }
                return 0;   // no grid for this matrix
            }
        } break;  // go try location indices (s.a. 'Helsinki')

        case LUA_TUSERDATA: {
            /* 
            * Using 'MatrixPos' (and not 'MatrixIter') is essential; this allows
            * calculations with positions (s.a. 'iter+pos(1,0)').
            */
            MatrixPos *mp= MatrixPos::instance(L,2);
            if (mp) {
                float v;
                try {
                    v= m[*mp];
                } 
                catch(...) {    // "Reading outside of matrix"
                    v= NAN;
                }
                lua_pushnumber( L, v );
                return 1;
            }
        } break;
    }
    
    // Check for location indices (userdata or array of userdata)
    //
    // Note: 'VectorMatrix.cpp' has similar code (if you make changes, change both!)
    //        
    LatLonList locs;
    LatLonList::e_state st= locs.init_from_ud(L,2);

    if (st== LatLonList::NONE) {
        luaL_error( L, "Bad index (for matrix): %s", lua_isstring(L,2) ? lua_tostring(L,2) : L_typename(2) );
    }

    // 'st' tells whether we should return the results in a wrapping table
    // or not ('{lat,lon}' pushes a number but '{ {lat,lon} }' pushes {number}).
    //    
    if (st == LatLonList::PUSH_AS_VALUE) {
        lua_pushnumber( L, m.at( locs[0] ));

    } else {
        lua_newtable(L);
        unsigned i=1;
        for( vector<LatLon>::const_iterator it= locs.begin();
            it != locs.end();
            ++it ) {
            lua_pushinteger( L, i++ );
            lua_pushnumber( L, m.at( *it ));
            lua_settable( L, -3 );
        }
    }

    return 1;
}


/*
* void= __newindex( m_ud, pos_ud, num )     -- m[ pos_ud ]= num|NAN
*/
int MatrixBind::newindex( lua_State *L ) {
    Matrix &m= *Matrix::instance(L,1);   // we'll modify it.

    MatrixPos *pos= MatrixPos::instance(L,2);
    if (!pos) {
        luaL_error( L, "Bad index for matrix: %s", L_typename(2) );
    }
    float v= (float)lua_tonumber(L,3);
    if ((v==0.0) && (!lua_isnumber(L,3))) {
        luaL_error( L, "Bad value (not a number): %s", L_typename(2) );
    }

    try {
        m.set_value( *pos, v );
    }
    catch( const E_ANY &e ) {
        luaL_error( L, e.what_nosource() );    // outside or read-only
    }

    return 0;   // nothing pushed
}


/*
* Set up a metatable and bind it to the registry.
*/
void MatrixBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__index"); lua_pushcfunction(L,index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__newindex"); lua_pushcfunction(L,newindex);
    lua_settable(L,-3);

    // Note: Binary operations which can have 'VectorMatrix' and 'Matrix' values together
    //      need to be handled by 'VectorMatrixBind'. Lua decides the methods based on the
    //      left operands metatable (if there is a method there) so we would otherwise
    //      get 'Matrix * VectorMatrix' to us.
    //
    lua_pushliteral(L,"__add"); lua_pushcfunction(L,Matrix::add); 
    lua_settable(L,-3);

    lua_pushliteral(L,"__sub"); lua_pushcfunction(L,Matrix::sub);
    lua_settable(L,-3);

    lua_pushliteral(L,"__mul"); lua_pushcfunction(L,VectorMatrixBind::mul);
    lua_settable(L,-3);

    lua_pushliteral(L,"__div"); lua_pushcfunction(L,VectorMatrixBind::div);
    lua_settable(L,-3);

    lua_pushliteral(L,"__mod"); lua_pushcfunction(L,Matrix::mod);   // no common use with VectorMatrix
    lua_settable(L,-3);
    
    lua_pushliteral(L,"__pow"); lua_pushcfunction(L,Matrix::pow);   // unary = always Matrix
    lua_settable(L,-3);

    lua_pushliteral(L,"__unm"); lua_pushcfunction(L,Matrix::unm);   // no common use with VectorMatrix
    lua_settable(L,-3);
    
    lua_pushliteral(L,"__tostring"); lua_pushcfunction(L,MatrixBind::tostring);
    lua_settable(L,-3);
}


/*---=== Matrix ===---*/

/*
* Get the value of the matrix at a particular geographical location.
*
* Returns:
*       value if location is inside and has valid data
*       NAN if location is outside of matrix, or that neighbourhood has no valid data
*/
float Matrix::at( const LatLon &latlon ) const {

	bool useGrid;
    double dx,dy;

	if ((useGrid = (wantedGrid && (size.getXS() > 1) && (size.getYS() > 1)))) {
	    NFmiPoint p = wantedGrid->LatLonToGrid( latlon.getLon(), latlon.getLat() );
	    dx = p.X() / (size.getXS() - 1);
	    dy = p.Y() / (size.getYS() - 1);
	}

    if (useGrid || getProjection().at( latlon, dx, dy )) {
        return at_( dx, dy );    // maybe within the grid (but not guaranteed)
    } else {
        return NAN;     // definately outside the grid
    }
}


/*--- Matrix output ---*/

/*
* Outputting a 'Matrix' as string
*
* Format is similar to Q2 server's, apart from outputting NANs.
*
*   "<x_size>,<y_size>;<num>[,<num>[, ...]]"   (no linefeeds)
*/
/*virtual*/ void Matrix::asString( ostream &out, int decimals ) const {
    out << getSize().getXS() << "," << getSize().getYS() << ";";

    if (decimals >= 0) {
        out.precision( decimals );
        out << fixed;
    }

    offset_t n= getN();
    for( offset_t i=0; i<n; i++ ) {
        if (i>0) out << ',';

        float v= (*this)[i];
        if (!isnanf(v)) {   // leaves ",," for a missing value
            out << v;
        }
    }
}


/*
* Convert a float value to q2 binary format integer.
*/
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
static int32_t q2_float2int( float v, int32_t scale, int32_t scaled_offset ) {
    return lround( ((double)v)*scale-scaled_offset );
}
#endif


/*
* Outputting a 'Matrix' as binary data.
*
* The format is courtesy Q2 server (and earlier):
*
*   BYTES   VALUES      DESCRIPTION
*   [0..3]: 0           length of "extra info" section right after this entry)
*           ...         extra info section (we don't use one)
*   [4..7]: 1|2         data is signed 16-bit (1) or signed 32-bit (2)
*   [8..11]: int32      'scale' used in float<->int transform (10^decimals, decimals=0..9)
*   [12..15]: int32     rows of data (>=1)
*   [16..19]: int32     columns of data (>=1)
*   [20..23]: int32     'scaled_offset' used in float<->int transform
*   [24..27]: int32     value used for missing data (usually 32700; here -2^15 or -2^31)
*
*   Data section follows, in usual Newbase iteration (y outermost South to North;
*   x innermost West to East) in either 16 or 32-bit entries.
*/
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
/*virtual*/ void Matrix::asBinary_q2_( ostream &out, int decimals ) const {

    float min_v= reduce_min();
    float max_v= reduce_max();

    xy_t rows= getSize().getYS();
    xy_t columns= getSize().getXS();

    if ((decimals<0) || (decimals>9)) {
        throw E_LOG_USAGE( "Decimals 0..9 must be given for binary output (was %d)", decimals );
    }

    //---
    // Initial values used if the matrix is empty (all values missing)
    //    
    // v_int= round( (v_float*scale)-offset )     (i.e. scale=10, offset=-80 -> 1.3 -> 93)
    // v_float= (v_int+offset)/scale
    //
    bool data_as_short= true;
    int32_t scale= (int32_t) powf(10.0f, decimals);   // 1,10,100, ..., 10^9
    int32_t scaled_offset= 0;
    
    if (!isnanf(min_v)) {
        // NOTE: In 'q2' original source, decision on 16/32-bit data type was made _before_
        //      calculating the precise offset. This would cause corruption of data if the
        //      range just fit in 16-bits and the selected offset was rounded to be slightly
        //      off-center of the range. This has been corrected here, by calculating the
        //      precise offset first.   --AKa 2-Feb-2010

        //---
        // WARNING: Following logic is a 'bit' fuzzy, but let's leave it as it.
        //          (it comes from q2 server code, without comments)
        //
        double offset_= (min_v+max_v)/2.0;   // median
        int pwr= static_cast<int>( log10(fabs(offset_)) )-1;  // (not sure why the '-1' is there)
        if (pwr >= 1) {
            double tmp= ::pow(10.0, (double)pwr);
            offset_= round(offset_/tmp)*tmp;
        }
        scaled_offset= lroundf(offset_) * scale;

        //---
        // Select the necessary data type (16 or 32-bit)
        //
        // NOTE: All values (with given decimals) are expected to fit at least the
        //       32-bit signed data. Data corruption will occur if this is not the
        //       case.
        //
        data_as_short= (q2_float2int(max_v, scale, scaled_offset) <= 32767) &&
                       (q2_float2int(min_v, scale, scaled_offset) >= -32767);  // -32768 marks missing values
    }

    const int32_t missing_value= data_as_short ? 0x8000 : 0x80000000;   // -32768 | -2147483648

    const int32_t arr[7]= {
        0,
        data_as_short ? 1:2, 
        scale,
        rows,
        columns,
        scaled_offset,
        missing_value
    };
    out.write( (const char *)arr, sizeof(arr) );

    //---
    // Use of 'MatrixIter' iterates in the Newbase native way: y in the outer loop and 
    // x in inner, starting from SW.
    //
    // NOTE: It is VITAL to keep the iteration order this way (part of the format).
    //
    for( MatrixIter pos(getSize()); pos.within(); ++pos ) {
        float v= (*this)[pos];
        int32_t vi= isnanf(v) ? missing_value : q2_float2int(v, scale, scaled_offset);
    
        if (data_as_short) {
            int16_t vs= (int16_t)vi;
            out.write( (const char *) &vs, sizeof(vs) );
        } else {
            out.write( (const char *) &vi, sizeof(vi) );
        }
    }
}
#endif


/*
* string= mt.__tostring( obj )
*/
int MatrixBind::tostring( lua_State *L ) {

    // Note: Malign client scripts can call this function with surprising parameters;
    //      make sure we don't crash.
    //
    const Matrix *p= Matrix::instance(L,1);
    if (!p) {
        throw E_LOG_USAGE( "Bad parameter (expecting matrix): %s", L_typename(1) );    // internal bug or deliberate hack attempt
    }

    p->push_tostring( L );
    return 1;
}


/*
* Copy contents from another matrix.
*
* Note: Size of the target matrix is not changed.
*/
void Matrix::copy_from( const Matrix &o ) {

    if (isReadOnly()) {
        throw E_READONLY();
    }

    assert( getGridSize() == o.getGridSize());

    offset_t n= getN();
    float *data= getData();
    const float *o_data= o.getData();
    if (data && o_data) {
#ifdef __SSE__
        SSE_copy( data, o_data, n );    // about the same speed as 'memcpy'
#else
        memcpy( data, o_data, n*sizeof(*data) );
#endif
    } else {
        for( offset_t i=0; i<n; i++ ) { 
            set_value_n( i, o[i] );
        }
    }

    param = o.param;
    level = o.level;
}


/*
* Fill a matrix with one value (without changing its size).
*/
#ifdef METQU
void Matrix::fill_with( float v ) {
    if (isReadOnly()) {
        throw E_READONLY();
    }

    offset_t n= getN();
    float *data= getData();
# ifdef __SSE__
    if (data) {
        SSE_fill( data, v, n );
    } else 
# endif
    {
        for( offset_t i=0; i<n; i++ ) { 
            set_value_n( i, v );
        }
    }
}
#endif


/*--- Matrix interpolation ---*/

/*
* Interpolate degrees.
*
* 'ABCD' are directions, in degrees (0..360-ε)
* 'prob_...' are weights 0.0..1.0
*
* Returns: 0..360-ε
*/
static double interpolate_deg( double A, double prob_A, double B, double prob_B, double C, double prob_C, double D, double prob_D ) {

    /* We do this by adding the values together as vectors. Result is the angle of the average 
    * of the four vectors (we don't care of the length; it only tells how unidirectional the values were).
    */
    Vector a( prob_A, A, true );    // polar
    Vector b( prob_B, B, true );
    Vector c( prob_C, C, true );
    Vector d( prob_D, D, true );

    Vector sum= a+b+c+d;

    // Well, if the length is 0.0, it tells the directions cancelled each other. Better give NAN in such
    // a case.
    //
    if (sum.getAbs() < 1e-6) {
        return NAN;             // no clear direction to give
    } else {
        return sum.getDeg();    // direction of sum is the same as direction of average
    }
}

static inline double interpolate_deg( double A, double prob_A, double B, double prob_B, double C, double prob_C ) {
    return interpolate_deg( A, prob_A, B, prob_B, C, prob_C, 0.0, 0.0 );    // zero probability leaves the fourth entry out
}
static inline double interpolate_deg( double A, double prob_A, double B, double prob_B ) {
    return interpolate_deg( A, prob_A, B, prob_B, 0.0, 0.0, 0.0, 0.0 );    // zero probability leaves the fourth entry out
}

/*
* Scale a 0..359.99 return from 'interpolate_deg' (on longitude values) back to longitude -179.99 .. 180.0 scale 
* (just twist the upper part down by one round; no offset was applied so 0 deg means 0 lon).
*/
static inline double deg_to_lon( double deg ) {
    return (deg > 180.0) ? deg-360.0 : deg;
}

/*
* Interpolate on a line (either of the corners may be missing)
*
*  a--b
*/
static double within_line( double a, double b, double dist_ab, NA_Param::e_Interpolation method ) {

    if (std::isnan(a)) {
        return dist_ab==1.0 ? b : NAN;
    } else if (std::isnan(b)) {
        return dist_ab==0.0 ? a : NAN;
    }
    
    switch( method ) {
        case NA_Param::INTERPOLATE_LINEAR:
        case NA_Param::INTERPOLATE_LINEAR_DEG:
        case NA_Param::INTERPOLATE_LINEAR_LON: {
            double a_prob= 1.0-dist_ab;
            double b_prob= dist_ab;
            
            if (method==NA_Param::INTERPOLATE_LINEAR) {
                return a*a_prob + b*b_prob;

            } else if (method==NA_Param::INTERPOLATE_LINEAR_DEG) {
                return interpolate_deg( a, a_prob, b, b_prob );

            } else {
                assert( method==NA_Param::INTERPOLATE_LINEAR_LON);
                return deg_to_lon( interpolate_deg( a, a_prob, b, b_prob ) );
            }
        }
        case NA_Param::INTERPOLATE_NEAREST:
        default:
            return (dist_ab < 0.5) ? a : b;
    }
}

/*
* Interpolate a value within a triangle (any of the corners may be missing)
*
*  a--b
*  | /
*  c
*
* 'dist_ab' and 'dist_ac' are distances to travel in that direction (0..1).
*/
static double within_triangle( double a, double b, double dist_ab, double c, double dist_ac, NA_Param::e_Interpolation method ) {
    if (std::isnan(a)) {
        // TBD: Calculate 'dist_bc' properly (using trigonometry)
        //
        double dist_bc= 0.5;

        return within_line( b, c, dist_bc, method );
    } else if (std::isnan(b)) {
        return within_line( a, c, dist_ac, method );
    } else if (std::isnan(c)) {
        return within_line( a, b, dist_ab, method );
    }

    switch( method ) {
        case NA_Param::INTERPOLATE_LINEAR:
        case NA_Param::INTERPOLATE_LINEAR_DEG:
        case NA_Param::INTERPOLATE_LINEAR_LON: {
            // TBD: These could be adjusted for more accurate results (using trigonometry)
            //
            double a_prob= (1.0-dist_ab)*(1.0-dist_ac);
            double b_prob= dist_ab*(1.0-dist_ac);
            double c_prob= (1.0-dist_ab)*dist_ac;

            if (method==NA_Param::INTERPOLATE_LINEAR) {
                return a*a_prob + b*b_prob + c*c_prob;

            } else if (method==NA_Param::INTERPOLATE_LINEAR_DEG) {      // 0 .. 359.999...
                return interpolate_deg( a, a_prob, b, b_prob, c, c_prob );

            } else {
                assert( method==NA_Param::INTERPOLATE_LINEAR_LON );     // -179.999... .. 180.0
                return deg_to_lon( interpolate_deg( a, a_prob, b, b_prob, c, c_prob ) );
            }
        }

        case NA_Param::INTERPOLATE_NEAREST:
        default:
            if (dist_ac >= 0.5) {
                return c;
            } else if (dist_ab >= 0.5) {
                return b;
            } else {
                return a;
            }
    }
}


/*
* Interpolation of values _anywhere_ within the matrix area (0..1, 0..1) coordinates.
*/
float Matrix::at_( double fx, double fy ) const {

    NA_Param::e_Interpolation method= unit.getMethod();

#ifndef NDEBUG
    if (method == NA_Param::INTERPOLATE_UNKNOWN) {
        throw E_LOG_BUG0( "Shouldn't be here with unknown interpolation" );
    }
#endif

    if ((fx<0.0) || (fx>1.0) || (fy<0.0) || (fy>1.0)) {
        return NAN;
    }

    xy_t xs= getGridSize().getX();
    xy_t ys= getGridSize().getY();

    // Find the surrounding box (the values we know)

    // Note: 'modf()' is the fastest way to get both integer part and fraction, 
    // at once (it can use FPU instructions to actually do this simultaneously).
    //
    double tmp;
    double dx= modf( fx*(xs-1), &tmp );     // 1.0 becomes 'xs-1' (= last column)
    xy_t x= (xy_t) tmp;

    double dy= modf( fy*(ys-1), &tmp );
    xy_t y= (xy_t) tmp;

    assert( x<xs && y<ys );
    assert( dx>= 0.0 );
    assert( dy>= 0.0 );

    /*
    *  A---B    (x,y) is the point A
    *  |   |    (dx,dy) is (0..1-, 0..1-), giving the position within the cube
    *  C---D
    *
    * Note: Using doubles for A..D for least fault in calculation itself.
    */
    double A= (*this)[ MatrixPos(x,y) ];
    double B= (x+1<xs) ? (*this)[ MatrixPos(x+1,y) ] : NAN;
    double C= (y+1<ys) ? (*this)[ MatrixPos(x,y+1) ] : NAN;

    // Simple cases first (the generic code below should give same results,
    // but handling this separately is faster and simplifies the thinking).
    //
    if (dx==0.0) {
        return within_line( A, C, dy, method );
    } else if (dy==0.0) {
        return within_line( A, B, dx, method );
    }

    assert( x+1<xs && y+1<ys );
    double D= (*this)[ MatrixPos(x+1,y+1) ];

    // We are within the square and need interpolation from triangles, or
    // the square (bilinear interpolation).
    //
    if (std::isnan(A)) {
        return within_triangle( D, C, 1.0-dx /*D->C*/, B, 1.0-dy /*D->B*/, method );
    }
    else if (std::isnan(B)) {
        return within_triangle( C, A, 1.0-dy /*C->A*/, D, dx /*C->D*/, method );
    }
    else if (std::isnan(C)) {
        return within_triangle( B, D, dy /*B->D*/, A, 1.0-dx /*B->A*/, method );
    } 
    else if (std::isnan(D)) {
        return within_triangle( A, B, dx /*A->B*/, C, dy /*A->C*/, method );
    }

    // All corners exist
    //
    switch( method ) {
        case NA_Param::INTERPOLATE_LINEAR:
        case NA_Param::INTERPOLATE_LINEAR_DEG: {
            // Use bilinear interpolation:
            // <http://en.wikipedia.org/wiki/Bilinear_interpolation>
            //
            double a_prob= (1.0-dx)*(1.0-dy);
            double b_prob= dx*(1.0-dy);
            double c_prob= (1.0-dx)*dy;
            double d_prob= dx*dy;
            
            if (method==NA_Param::INTERPOLATE_LINEAR) {
                return A*a_prob + B*b_prob + C*c_prob + D*d_prob;
            } else {
                return interpolate_deg( A, a_prob, B, b_prob, C, c_prob, D, d_prob );
            }
        }
        case NA_Param::INTERPOLATE_NEAREST:
        default:
            if (dy<0.5) {
                return (dx<0.5) ? A : B;
            } else {
                return (dx<0.5) ? C : D;
            }
    }
}

/*
* Fit the contents from another matrix, either shrinking or stretching as appropriate.
*
* Note: The matrices must have same projection.
*/
void Matrix::fit_from_same_projection( const Matrix &o ) {

#ifndef NDEBUG
    NA_Param::e_Interpolation o_method= o.getUnit().getMethod();
    
    if (o_method == NA_Param::INTERPOLATE_UNKNOWN) {
        throw E_LOG_BUG0( "Shouldn't be here with unknown interpolation" );
    }
    if (getUnit() != o.getUnit()) {
        throw E_LOG_BUG( "Units of target and source (for interpolation) should be the same. (%s != %s)", getUnit().getUnitName().c_str(), o.getUnit().getUnitName().c_str() );
    }
#endif

    if (isReadOnly()) {
        throw E_READONLY();
    }

    assert( getProjection() == o.getProjection() );

    if (getGridSize() == o.getGridSize()) {
        copy_from(o);  // same size; no stretch

    } else {

        // Edge of area is 'xs'-1; make it 1.0
        //
        double x_scale= 1.0 / ((double) getGridSize().getX()-1);
        double y_scale= 1.0 / ((double) getGridSize().getY()-1);
    
        for( MatrixIter mi(getSize()); mi.within(); ++mi ) {
            double dx= mi.getX()*x_scale;
            double dy= mi.getY()*y_scale;
    
            set_value( mi, o.at_( dx, dy ) );
        }
    }
}


/*
* Fill the matrix from another, with projection information applied.
*/
void Matrix::fit_from_( const Matrix &o ) {

    if (isReadOnly()) {
        throw E_READONLY();
    }

    Projection pr= getProjection();
    Projection o_pr= o.getProjection();

    if (pr == o_pr) {
        fit_from_same_projection( o );
    
    } else {
        MatrixSize gs= getSize();

        double fx= 1.0 / (gs.getXS()-1);
        double fy= 1.0 / (gs.getYS()-1);
        
        for( MatrixIter mi(getSize()); mi.within(); ++mi ) {
            double dx= mi.getX() * fx;
            double dy= mi.getY() * fy;

            LatLon ll= pr.latlon( dx, dy );
#if 1
            set_value( mi, o.at(ll) );
#else
            double o_dx, o_dy;
            if (o_pr.at( ll, o_dx, o_dy )) {
                set_value( mi, o.at_( o_dx, o_dy ) );
            } else {
                set_value( mi, NAN );   // outside of the 'o' projection
            }
#endif
        }
    }
}


/*
* Note: Binary output mode does NOT apply to 'tostring()'. This is always text.
*/
void ApiMatrix::push_tostring( lua_State *L ) const {
    stringstream ss;

    int decs= RegTools::get_Decimals(L);
    this->asString( ss, decs );
    
    lua_pushlstring( L, ss.str().c_str(), ss.str().size() );
}

/*
 * Push location (latlon_ud) relative to given gridpoint.
 */
int Matrix::offsetPosition(lua_State *L,const NFmiPoint &location,const MatrixPos &offset) const {

	NFmiGrid * grid = (wantedGrid ? wantedGrid.get() : nullptr);

	if (grid) {
		NFmiPoint xyPoint(grid->Area()->ToXY(location));
		double itsGridXDiff = grid->Area()->Width() / (grid->XNumber() - 1);
		double itsGridYDiff = grid->Area()->Height() / (grid->YNumber() - 1);
		xyPoint.X(xyPoint.X() + itsGridXDiff * offset.getX());
		xyPoint.Y(xyPoint.Y() - itsGridYDiff * offset.getY());	// Note: reverse y -direction
		NFmiPoint p(grid->Area()->ToLatLon(xyPoint));

		new(L) LatLon( p.Y(), p.X() );

        return 1;
	}

	return 0;
}
int Matrix::offsetPosition(lua_State *L,const Matrix &m,const MatrixPos &pos,const MatrixPos &offset) const {

	NFmiGrid * grid = (wantedGrid ? wantedGrid.get() : nullptr);
	int x = pos.getX(),y = pos.getY();

	if (grid && (x >= 0) && (x < size.getXS()) && (y >= 0) && (y < size.getYS()))
		return m.offsetPosition(L,grid->GridToLatLon(x,y),offset);

	return 0;
}
int Matrix::offsetPosition(lua_State *L,const NFmiLocation &location,double xoffsetkm,double yoffsetkm) const {

	NFmiGrid * grid = (wantedGrid ? wantedGrid.get() : nullptr);

	if (grid) {
		NFmiLocation loc(location);

		if(xoffsetkm > 0)
			loc.SetLocation(90., xoffsetkm*1000., grid->Area()->PacificView());
		else if(xoffsetkm < 0)
			loc.SetLocation(270., xoffsetkm*1000., grid->Area()->PacificView());
		if(yoffsetkm != 0)
			loc.SetLocation(360., yoffsetkm*1000., grid->Area()->PacificView());

		NFmiPoint p(loc.GetLocation());

		new(L) LatLon( p.Y(), p.X() );

        return 1;
	}

	return 0;
}
int Matrix::offsetPosition(lua_State *L,const Matrix &m,const MatrixPos &pos,double xoffsetkm,double yoffsetkm) const {

	NFmiGrid * grid = (wantedGrid ? wantedGrid.get() : nullptr);
	int x = pos.getX(),y = pos.getY();

	if (grid && (x >= 0) && (x < size.getXS()) && (y >= 0) && (y < size.getYS()))
		return m.offsetPosition(L,grid->GridToLatLon(x,y),xoffsetkm,yoffsetkm);

	return 0;
}
