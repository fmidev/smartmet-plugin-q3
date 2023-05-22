/*
 * MEMMATRIX.H                      Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Revised:  20-Oct-2010 AKa
 */
#ifndef MEMMATRIX_H
#define MEMMATRIX_H

#include "Matrix.h"
#include "Projection.h"

#include <boost/optional.hpp>

/*---=== MemMatrix ===---
 *
 * Read-write matrices used as work canvas, and for calculated (with projection)
 * matrices.
 */
class MemMatrix : public Matrix {
public:
  MemMatrix(const MatrixSize &size_, const NA_Param::Unit &unit_,
            const Projection &proj_) throw();
  MemMatrix(const MatrixPos &gridsize, const NA_Level &level_,
            FmiParameterName param_, const NA_Param::Unit &unit_,
            const Projection &proj_) throw();
  MemMatrix(const MatrixSize &size_, float v, const NA_Param::Unit &unit_,
            const Projection &proj_) throw();
  MemMatrix(const MatrixPos &gridsize, float v, const NA_Level &level_,
            FmiParameterName param_, const NA_Param::Unit &unit_,
            const Projection &proj_) throw();

  MemMatrix(const Matrix &o) throw(); // copy constructor from _any_ matrix
                                      // (also 'SQD_Matrix' and 'MQD_Matrix')
  /*virtual*/ ~MemMatrix();

  /*virtual*/ float get_value_n(offset_t n) const throw() { return data[n]; }

  // Non-virtual assignment operator: we use assignment to 'MemMatrix' rather
  // much and this makes the code look neater than extensive use of
  // 'set_value_n()'. We cannot use this for the base 'Matrix' type since
  // 'SQD_Matrix' cannot expose a memory location.
  //
  float &operator[](offset_t n) throw() {
    assert(!is_readonly); // script level must have checked it

    return data[n]; // reference to the float
  }
  float &operator[](const MatrixPos &mi) {
    assert(!is_readonly); // script level must have checked it
    return data[offset(mi - getSize().getTop())];
  }

  // Since non-const 'operator[]' are defined, seems we need to define the const
  // variants as well (otherwise gcc does not see through them to 'Matrix').
  //
  float operator[](offset_t n) const throw() { return data[n]; }
  float operator[](const MatrixPos &pos) const {
    return data[offset(pos)];
  }

  /*virtual*/ void set_value_n(offset_t n, float v) throw() {
    assert(!is_readonly); // script level must have checked it
    data[n] = v;
  }

  /*virtual*/ const float *getData() const throw() {
    return data;
  } // SSE aligned
  /*virtual*/ float *getData() throw() {
    assert(!is_readonly); // script level must have checked it
    return data;
  }

  /*virtual*/ const Projection &getProjection() const { return proj; }

  void fill(float v);

  // Make a matrix read-only after its initial data has been set. Used i.e. when
  // making interpolations of SQD data; changing such data is conceptually wrong
  // by apps.
  //
  void set_readonly() { is_readonly = true; }

  void scrap_projection() { proj = Projection::NONE; }

  // 10-Jan-2014 PKi: To get and store max data value; the max value is queried
  // repeatedly when contouring.
  //
  float reduce_and_store_max() {
    if (!maxValue) {
      maxValue = reduce_max();
    }
    return *maxValue;
  }

  void set_size(const MatrixPos &gs) {
    if ((gs.getX() * gs.getY()) > (int)getN())
      throw std::runtime_error("Cannot enlarge matrix");
    else
      resize(gs);
  }

private:
  void div_by_nonzero(const MemMatrix &a);

  friend class Matrix; // allow SSE functions to write 'data'

  float *const data; // read/write array (whose address does not change); SSE
                     // aligned (16 byte)
  Projection proj;   // not 'const' because of 'scrap_projection()'

  // 10-Jan-2014 PKi: The maximum data value
  //
  boost::optional<float> maxValue;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(data);
    Matrix::_INVARIANT(file, line); // checks SSE alignment
  }
#endif
};

#endif
// MEMMATRIX_H
