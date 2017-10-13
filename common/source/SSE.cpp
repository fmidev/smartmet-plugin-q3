/*
* SSE.CPP                    Copyright (c) 2008-2010, Ilmatieteen laitos
*
* References:
* <http://www.codeproject.com/KB/recipes/sseintro.aspx?display=PrintAll&fid=16168&df=90&mpp=25&noise=3&sort=Position&view=Quick&fr=26>
* <http://sseplus.sourceforge.net/fntable.html>
* <http://ds9a.nl/gcc-simd/fp-simd-builtins.html#id2465481>
*
* Bugs:
*   Ubuntu 32-bit (9.04) is seen to have problems when SSE is enabled. Unsure, why.
*       --AKa 27-Aug-2009
*/
#include "SSE.h"

#ifndef __SSE__
# error "Only for SSE enabled compilations."
#endif

#include "LogTools.h"
#include "Tools.h"

#include "MemMatrix.h"

#include <cmath>

#define HAS_NANS_8(vb) (!NO_NANS_8(vb))


/*---=== Helpers ===---*/

static bool mm_nan_comp( const __m128 &a, const __m128 &b ) {
    const float *pa= (const float*)&a;
    const float *pb= (const float*)&b;
    
    for( unsigned i=0; i<4; i++ ) {
        // nan!=nan always; needs an explicit test
        //
        if (isnanf(pa[i]) && isnanf(pb[i])) continue;   // ok

        if (pa[i] != pb[i]) return false;
    }
    return true;
}

# if 0
static void mm_print( const __m128 &a ) {
    const float *pa= (const float*)&a;
    fprintf( stderr, ">> %f %f %f %f\n", pa[0], pa[1], pa[2], pa[3] );
}
# endif

# if 0
static void mm_print_uint128( const __m128 &a ) {
    const unsigned *pa= (const unsigned*)&a;
    
    fprintf( stderr, ">> %x %x %x %x (%d)\n", pa[0], pa[1], pa[2], pa[3], _mm_movemask_ps(a) );
}
# endif

#define always_assert( eq ) \
    if (!(eq)) { \
        LOG_FATAL( "Assertion failed in %s:%d (%s)", __FILE__, __LINE__, #eq ); \
        abort(); \
    }

/*
* Assumptions we've done on 'min<float>()' and 'max<float>()' and 'sum'.
*
* To be run once.
*/
static struct JustOnce_SSE {
    JustOnce_SSE();
} just_once;

JustOnce_SSE::JustOnce_SSE() {
    float nan= NAN;
    float X= 10.0F;     // anything
    float v;

    always_assert( nan!=nan );     // property of NAN
    always_assert( isnanf(nan) );
    always_assert( !isnanf(X) );

    v= std::min<float>( nan,nan ); always_assert( isnanf(v) );
    v= std::min<float>( X, nan ); always_assert( v==X );
    //v= std::min<float>( nan, X ); always_assert( v==X );   // THIS FAILS

    v= std::max<float>( nan,nan ); always_assert( isnanf(v) );
    v= std::max<float>( X, nan ); always_assert( v==X );
    //v= std::max<float>( nan, X ); always_assert( v==X );   // THIS FAILS

    v= std::min<float>( INF_F, nan ); always_assert( v==INF_F );
    v= std::min<float>( INF_F, X ); always_assert( v==X );

    v= std::max<float>( -INF_F, nan ); always_assert( v==-INF_F );
    v= std::max<float>( -INF_F, X ); always_assert( v==X );

#ifdef __SSE__
    __m128 m_XnanXnan= (__m128)(__v4sf){ X, nan, X, nan };
    __m128 m_nanXnanX= (__m128)(__v4sf){ nan, X, nan, X };
    __m128 m_nannannannan= (__m128)(__v4sf){ nan, nan, nan, nan };
    __m128 m_XXXX= _mm_set1_ps(X);  // { X,X,X,X }

    __m128 m_ninf= _mm_set1_ps(-INF_F);  // { -INF,-INF,-INF,-INF }
    __m128 m_pinf= _mm_set1_ps(INF_F);  // { INF,INF,INF,INF }

    const __m128 A[2]= { m_XnanXnan, m_nannannannan };
    const __m128 B[2]= { m_XXXX, m_nannannannan };
    const __m128 C[2]= { m_XnanXnan, m_XXXX };
    const __m128 D[2]= { m_XXXX, m_XXXX };

    always_assert( HAS_NANS_8(A) );
    always_assert( HAS_NANS_8(B) );
    always_assert( HAS_NANS_8(C) );
    always_assert( !HAS_NANS_8(D) );

    //---
    // Min & max in SSE:
    //
    // NAN in RIGHT parameter forces a NAN in the result.
    // NAN in LEFT parameter surrenders to non-NAN in right.
    //
    // So, we must NEVER have NAN in the RIGHT parameter (this is exactly the
    // opposite of how 'std::min|max<>' works (above)).
    //
    for( unsigned loop=0; loop<2; loop++ ) {
        __m128 v1,v2,v3,v4,v5;

        if (loop==0) {
            v1= __builtin_ia32_maxps( m_XnanXnan, m_XnanXnan );  // -> XnanXnan
            v2= __builtin_ia32_maxps( m_XnanXnan, m_nanXnanX );  // -> nanXnanX
            v3= __builtin_ia32_maxps( m_nannannannan, m_XXXX );  // -> XXXX
            v4= __builtin_ia32_maxps( m_XXXX, m_nannannannan );  // -> nannannannan
            v5= __builtin_ia32_maxps( m_XnanXnan, m_ninf );      // -> { X, -INF, X, -INF }
        } else {
            v1= __builtin_ia32_minps( m_XnanXnan, m_XnanXnan );  // -> XnanXnan
            v2= __builtin_ia32_minps( m_XnanXnan, m_nanXnanX );  // -> nanXnanX
            v3= __builtin_ia32_minps( m_nannannannan, m_XXXX );  // -> XXXX
            v4= __builtin_ia32_minps( m_XXXX, m_nannannannan );  // -> nannannannan
            v5= __builtin_ia32_minps( m_XnanXnan, m_pinf );      // -> { X, INF, X, INF }
        }
        (void) v1; (void) v2; (void) v3; (void) v4;

# if 0
        mm_print_uint128( _mm_cmpunord_ps(v1,v3) );
        mm_print_uint128( _mm_cmpunord_ps(v2,v3) );
        mm_print_uint128( _mm_cmpunord_ps(v3,v3) );
        mm_print_uint128( _mm_cmpunord_ps(v4,v3) );
        mm_print( v5 );
#endif

#if 0
        always_assert( mm_nan_comp( v1, m_XnanXnan ) );
        //always_assert( mm_nan_comp( v2, m_XXXX ) ); // THIS FAILS
        always_assert( mm_nan_comp( v3, m_XXXX ) );
        //always_assert( mm_nan_comp( v4, m_XXXX ) ); // THIS FAILS
        always_assert( mm_nan_comp( v3, m_XXXX ) );
        (void)v2; (void)v4;
#endif        
        const float* f5= (const float*)&v5;
        always_assert( f5[0]==X );
        always_assert( f5[1]== (loop==0) ? -INF_F : INF_F );
        always_assert( f5[2]==X );
        always_assert( f5[3]== (loop==0) ? -INF_F : INF_F );
    }
    
    // Average & sum assumptions
    //
    __m128 va= __builtin_ia32_addps( m_XnanXnan, m_nanXnanX );
    always_assert( mm_nan_comp( va, m_nannannannan ) );
#endif

#if (!defined(METQU)) && (!defined(MQD2SQD))
    //LOG_DEBUG0( "min/max assumptions() passed." );
#endif
}


/*---=== SSE operations ===---*
*
* Here are operations which can be presented as functions (which don't take in
* an SSE 'OP' as parameter; such macros are in the header).
*/

/*
* Absolute of a matrix
*
* There is no 'abs' in SSE but we can do 'sqrt(x*x)'
*/
void SSE_abs( MemMatrix &c, const Matrix &a, MatrixPos::offset_t &i, MatrixPos::offset_t n ) {
    const float *a_data= a.getData();
    if (a_data) {
        float *c_data= c.getData();
        assert( c_data );
    
        __m128* vc= (__m128*)c_data;
        const __m128* va= (const __m128*)a_data;
    
        while( i+(16-1) < n ) {     // bulk handled here
            *(vc++)= __builtin_ia32_sqrtps(__builtin_ia32_mulps(*va,*va)); ++va;
            *(vc++)= __builtin_ia32_sqrtps(__builtin_ia32_mulps(*va,*va)); ++va;
            *(vc++)= __builtin_ia32_sqrtps(__builtin_ia32_mulps(*va,*va)); ++va;
            *(vc++)= __builtin_ia32_sqrtps(__builtin_ia32_mulps(*va,*va)); ++va;
            i += 16;
        }
        while( i+(4-1) < n ) {  // last 4..15 values (just an optimization)
            *(vc++)= __builtin_ia32_sqrtps(__builtin_ia32_mulps(*va,*va)); ++va;
            i += 4;
        }
        // last 0..3 values left for regular loop
    }
}


/*
* Count the sum of non-NAN fields, and the number of such fields.
*/
void SSE_reduce_sum( const Matrix &a, float &sum, unsigned &used, MatrixPos::offset_t &i, MatrixPos::offset_t n, reduce_sum_safe_f f ) {

    assert( sum==0.0f );
    assert( used==0 );

    const float *a_data= a.getData();
    if (a_data) {
        __m128* va= (__m128*)a_data;

        __m128 sum4= (__m128)(__v4sf){ 0.0f, 0.0f, 0.0f, 0.0f };
        while( i+(8-1) < n ) {     // bulk handled here
            if (NO_NANS_8(va)) {
                sum4= __builtin_ia32_addps( sum4, *va++ );
                sum4= __builtin_ia32_addps( sum4, *va++ );
                used += 8;
            } else {
                for( unsigned j=i; j<i+8; j++ ) { 
                    f( a_data[j], sum, used );
                }
                va += 2;
            }
            i += 8;
        }
        const float *f4= (const float*)&sum4;
        sum += f4[0] + f4[1] + f4[2] + f4[3];

        //last 0..7 values left for regular loop
    }
}


/*
* Count the number of non-NAN fields, only.
*/
void SSE_reduce_count( const Matrix &a, unsigned &used, MatrixPos::offset_t &i, MatrixPos::offset_t n, reduce_count_safe_f f ) {
    assert( used==0 );

    const float *a_data= a.getData();
    if (a_data) {
        __m128* va= (__m128*)a_data;

        while( i+(8-1) < n ) {     // bulk handled here
            if (NO_NANS_8(va)) {
                used += 8;
            } else {
                for( unsigned j=i; j<i+8; j++ ) { 
                    f( a_data[j], used );
                }
            }
            va += 2;
            i += 8;
        }
        //last 0..7 values left for regular loop
    }
}


/*
* Fill 'to' with 'n' floats of value 'v'
*/
void SSE_fill( float *to, float v, MatrixPos::offset_t n ) {
    assert( to && ((size_t)to)%16==0 );

    __m128* vc= (__m128*)to;
    const __m128 vvvv= (__m128)(__v4sf){ v,v,v,v };
    MatrixPos::offset_t i=0;

    while( i+(32-1) < n ) {     // bulk handled here
        *(vc++)= vvvv; *(vc++)= vvvv;
        *(vc++)= vvvv; *(vc++)= vvvv;
        *(vc++)= vvvv; *(vc++)= vvvv;
        *(vc++)= vvvv; *(vc++)= vvvv;
        i += 32;
    }
    while( i+(4-1) < n ) {  // last 4..31 values (just an optimization)
        *(vc++)= vvvv;
        i += 4;
    }
    while(i<n) {    // last 0..3 values
        to[i++]= v;
    }
}


/*
* Copy a block 'from' -> 'to', 'n' values.
*/
void SSE_copy( float *to, const float *from, MatrixPos::offset_t n ) {
    assert( to && ((size_t)to)%16==0 );
    assert( from && ((size_t)from)%16==0 );

    __m128* vc= (__m128*)to;
    const __m128 *va= (const __m128*)from;
    MatrixPos::offset_t i=0;

    while( i+(32-1) < n ) {     // bulk handled here
        *(vc++)= *(va++); *(vc++)= *(va++);
        *(vc++)= *(va++); *(vc++)= *(va++);
        *(vc++)= *(va++); *(vc++)= *(va++);
        *(vc++)= *(va++); *(vc++)= *(va++);
        i += 32;
    }
    while( i+(4-1) < n ) {  // last 4..31 values (just an optimization)
        *(vc++)= *(va++);
        i += 4;
    }
    while(i<n) {    // last 0..3 values
        to[i]= from[i]; 
        ++i;
    }
}


/*
* Finding a NAN among the floats (or not).
*/
bool SSE_hasnan( const float *p, MatrixPos::offset_t n ) {
    assert( p && ((size_t)p)%16==0 );

    const __m128* va= (const __m128*)p;
    MatrixPos::offset_t i=0;

    while( i+(16-1) < n ) {     // bulk handled here
        if ((_mm_movemask_ps( _mm_cmpord_ps(*va,*(va+1)) )
             | _mm_movemask_ps( _mm_cmpord_ps(*(va+2),*(va+3)) )) != 0) {
            return true;    // at least one NAN
        }
        i += 16;
    }
    while( i<n ) {  // last 0..15 values
        if (isnanf(p[i])) return true;
        ++i;
    }
    return false;   // no NANs
}




