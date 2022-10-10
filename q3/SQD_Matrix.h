/*
 * SQD_MATRIX.H                      Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Revised:  22-Oct-2010 AKa
 */
#ifndef SQD_MATRIX_H
#define SQD_MATRIX_H

#include "Matrix.h"
#include "Projection.h"
#include "SQD_Data.h"
#include "SQD_Tools.h"

#ifndef USE_NEWBASE
#error "This file shouldn't be included when compiling without Newbase."
#endif

/*
 * To ensure coherence between WD and WS (WD must be 0 if WS is missing; 0+..360
 * if WS is provided) and WVEC and WS,WD (WVEC= 100*round(WS)+(WD/10)), we need
 * some extra administrational things.
 *
 * If this is defined, coherency is checked. If not defined, we keep the
 * parameters unaware of each other.
 */
#define SQD_WD_WS_TRACKING

class NA_Level;
class NFmiFastQueryInfo;

/*---=== SQD_Matrix ===---
 *
 * Matrix that is fixed to the grid points of an underlying SQD data (either
 * read-only data fetched from a file, or read-write data created from scratch
 * to memory).
 *
 * Presents a certain param, level and time combo of the Raw data.
 */
class SQD_Matrix : public Matrix {
public:
#ifdef METQU
  SQD_Matrix(SQD_Data *data, const JDay &vt, const NA_Level &lev,
             FmiParameterName e) throw(E_NO_MATCH);
#endif
  SQD_Matrix(const SQD_Data *data, const JDay &vt, const NA_Level &lev,
             FmiParameterName e) throw(E_NO_MATCH);

  /*virtual*/ ~SQD_Matrix();

  /*virtual*/ float get_value_n(offset_t n) const throw();

#ifdef METQU
  /*virtual*/ void set_value_n(offset_t n, float v) throw();
#else
  /*virtual*/ void set_value_n(offset_t n, float v) throw() {
    throw E_LOG_BUG0("Trying to write an SQD_Matrix");
  }
#endif

  /*virtual*/ const float *getData() const throw() {
    return 0; /* no SSE compatible block */
  }

  /*virtual*/ float *getData() throw() {
#ifdef METQU
    assert(!isReadOnly()); // upper levels should have taken care
    return 0; // no SSE compatible block (always use 'get_value_n()')
#else
    throw E_LOG_BUG0("Trying to write an SQD_Matrix");
#endif
  }

  /*virtual*/ const Projection &getProjection() const { return proj; }

private:
  typedef float (*conv_f)(
      float); // conversion function (for 'WD' range transforms)

  SQD_Matrix(const SQD_Matrix &);            // no copies
  SQD_Matrix &operator=(const SQD_Matrix &); // no assigning

  // data members
  //
  NFmiFastQueryInfo *fi; // iterator placed to our time, level and parameter

  const Projection proj;

  conv_f get_f;
  conv_f set_f;

  /* Extra fields that are non-nullptr only for 'WVEC' and 'WD' params (which
   * need to know about others.
   */
#ifdef SQD_WD_WS_TRACKING
  FmiParameterName my_e; // kind of parameter we're carrying

  NFmiFastQueryInfo *fi_WD; // non-nullptr only if 'fi' points to WVEC
  NFmiFastQueryInfo *fi_WS; // non-nullptr only if 'fi' points to WD or WVEC
#endif

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(fi);

    // Must have interpolation info
    //
    assert_invariant(getUnit().getMethod());

    Matrix::_INVARIANT(file, line);
  }
#endif
};

#endif
// SQD_MATRIX_H
