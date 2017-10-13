/*
* MQD_MATRIX.H                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Matrices linked to a memory mapped MQD file.
*
* Revised:  6-Oct-2010 AKa
*/
#ifndef MQD_MATRIX_H
#define MQD_MATRIX_H

#ifndef MQD_ENABLED
# error "Should not have read this header."
#endif

#include "Matrix.h"

#include "MQD_Data.h"
#include "NA_Level.h"


/*---=== MQD_Matrix ===---
*
* Matrix that is fixed to the grid points of an underlying MQD file data,
* and fetches its values directly from there.
*
* Note: The 'data' given to 'MQD_Matrix' in constructor is expected to remain valid
*       throughout the lifespan of this object (this is on the caller to guarantee!).
*/
class MQD_Matrix : public Matrix {
  public:
#ifdef METQU
    MQD_Matrix( MQD_Data *data_, const JDay &time, const NA_Level &level, const NA_Param &p );
#endif
    MQD_Matrix( const MQD_Data *data_, const JDay &time, const NA_Level &level, const NA_Param &p );

    /*virtual*/ ~MQD_Matrix() {}

    /*virtual*/ float get_value_n( offset_t n ) const throw();

#ifdef METQU
    /*virtual*/ void set_value_n( offset_t n, float v ) throw();
#else
    /*virtual*/ void set_value_n( offset_t n, float v ) throw() {
        throw E_LOG_BUG0( "Trying to write an MQD_Matrix" );
    }
#endif

    /*virtual*/ const float *getData() const throw();

#ifdef METQU
    /*virtual*/ float *getData() throw();
#else
    /*virtual*/ float *getData() throw() {
        throw E_LOG_BUG0( "Trying to write an MQD_Matrix" );
    }
#endif

    /*virtual*/ const Projection &getProjection() const { return proj; }

  private:
    // data members

    MQD_Data::Tile tile;
    const Projection proj;

    MQD_Matrix( const MQD_Matrix & );   // no copies
    MQD_Matrix& operator=( const MQD_Matrix & );    // no assigning

#ifndef NDEBUG        
    void _INVARIANT( const char *file, unsigned line ) const {

        // Must have interpolation info
        //
        assert_invariant( getUnit().getMethod() );

        Matrix::_INVARIANT( file, line );   // check 'getData()' alignment
    }
#endif
};

#endif
    // MQD_MATRIX_H
