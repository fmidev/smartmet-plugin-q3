/*
 * MQD_MATRIX.CPP                          Copyright (c) 2009-10, Ilmatieteen
 * laitos
 */
#ifdef MQD_ENABLED

#include "MQD_Matrix.h"

// Constructor to read/write data
//
#ifdef METQU
MQD_Matrix::MQD_Matrix(MQD_Data *data_, const JDay &time, const NA_Level &level,
                       const NA_Param &p)
    : Matrix(MatrixPos::ZERO, p.getUnit(), data_->isReadOnly()),
      tile(data_->getTile(time, level, p)), proj(data_->getProjection()) {
  Matrix::resize(tile.getGridSize());
  INVARIANT();
}
#endif

// Constructor to a const data
//
MQD_Matrix::MQD_Matrix(const MQD_Data *data_, const JDay &time,
                       const NA_Level &level, const NA_Param &p)
    : Matrix(MatrixPos::ZERO, p.getUnit(), true /*read-only*/),
      tile(data_->getTile(time, level, p)), proj(data_->getProjection()) {
  Matrix::resize(tile.getGridSize());
  INVARIANT();
}

/*
 * Get a certain value from the matrix.
 */
/*virtual*/ float MQD_Matrix::get_value_n(offset_t n) const throw() {
  return tile.getPtr()[n]; // straight through
}

/*
 * Set a certain value
 */
#ifdef METQU
/*virtual*/ void MQD_Matrix::set_value_n(offset_t n, float v) throw() {
  assert(!isReadOnly()); // upper level should have checked

  tile.getPtr()[n] = v;
}
#endif

/*
 * Get an SSE aligned pointer for reading the matrix.
 */
/*virtual*/ const float *MQD_Matrix::getData() const throw() {
  return tile.getPtr(); // SSE aligned
}

/*
 * Get an SSE aligned pointer for writing the matrix.
 */
#ifdef METQU
/*virtual*/ float *MQD_Matrix::getData() throw() {
  assert(!isReadOnly()); // upper level should have checked

  return tile.getPtr(); // SSE aligned
}
#endif

#endif
// MQD_ENABLED
