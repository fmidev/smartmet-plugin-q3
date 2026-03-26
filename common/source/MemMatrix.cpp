/*
* MEMMATRIX.CPP                          Copyright (c) 2008-10, Ilmatieteen laitos
*
* Revised:  20-Oct-2010 AKa
*/
#include "MemMatrix.h"
#include "SSE.h"

/*---=== MemMatrix ===---*/

/*
* Note: 'data' needs to be 16 byte aligned for SSE access. Traditional C++ 'new' does not provide
*       alignment guarantees (at least not 16 bytes for an array of floats).
*/
MemMatrix::MemMatrix( const MatrixSize &size_, const NA_Param::Unit &unit_, const Projection &proj_ ) noexcept
    : Matrix(size_, unit_, false /*rw*/), 
      data( sse_alloc( sizeof(float)*getN() )), proj(proj_) { /*not initialized*/ INVARIANT(); }

MemMatrix::MemMatrix( const MatrixPos &gridsize, const NA_Level &level_, FmiParameterName param_, const NA_Param::Unit &unit_, const Projection &proj_ ) noexcept
    : Matrix(gridsize, level_, param_, unit_, false /*rw*/), 
      data( sse_alloc( sizeof(float)*getN() )), proj(proj_) { /*not initialized*/ INVARIANT(); }

MemMatrix::MemMatrix( const MatrixSize &size_, float v, const NA_Param::Unit &unit_, const Projection &proj_ ) noexcept
    : Matrix(size_, unit_, false /*rw*/), 
      data( sse_alloc( sizeof(float)*getN() )), proj(proj_) { fill(v); INVARIANT(); }

MemMatrix::MemMatrix( const MatrixPos &gridsize, float v, const NA_Level &level_, FmiParameterName param_, const NA_Param::Unit &unit_, const Projection &proj_ ) noexcept
    : Matrix(gridsize, level_, param_, unit_, false /*rw*/), 
      data( sse_alloc( sizeof(float)*getN() )), proj(proj_) { fill(v); INVARIANT(); }

MemMatrix::MemMatrix( const Matrix &o ) noexcept
    : Matrix(o.getSize(), o.getUnit(), false /*rw*/), 
      data( sse_alloc( sizeof(float)*getN() )), proj(o.getProjection()) {

    copy_from(o);
    INVARIANT();
}

MemMatrix::~MemMatrix() {
    INVARIANT();

    sse_free(data);
}


/*
* Filling the matrix with given value. 
*/
void MemMatrix::fill( float v ) {
    offset_t n= getN();

#ifdef __SSE__
    SSE_fill( data, v, n );     // as fast as 'memset()'
#else
    if (isnanf(v)) {
        // Filling with '0xff' causes the matrix to be all NANs
        //
        memset( data, 0xff, n*sizeof(*data) );
        assert( isnanf(*data) );
    } else {
        for( offset_t i=0; i<n; i++ ) { data[i]= v; }
    }
#endif
}


/*
* Division by a non-zero matrix.
*
* Used in average calculation, where we know the divisor will not have zeros
* (and thus we can safely use SSE optimizations).
*/
void MemMatrix::div_by_nonzero( const MemMatrix &a ) {
    assert( getSize() == a.getSize() );

    // Matrix divided by matrix (slot per slot)
    //
    offset_t n= getN();
    offset_t i=0;
#ifdef __SSE__
    SSE_OP_mm( __builtin_ia32_divps, *this, this, &a, i, n );
#endif            
    for( ; i<n; i++ ) { (*this)[i] /= a[i]; }
}
