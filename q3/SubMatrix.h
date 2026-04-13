/*
 * SUBMATRIX.H                      Copyright (c) 2009-10, Ilmatieteen laitos
 *
 * Light "peekhole" object that references an actual matrix object.
 */
#ifndef SUBMATRIX_H
#define SUBMATRIX_H

#include "Matrix.h"
#include "MatrixPos.h"

/*---=== Array2D ===---
 *
 * Helper template for making a 2D array (of any type) that can be indexed using
 * 'MatrixPos' (even negative indices on both X and Y).
 */
template <class T> class Array2D {
public:
  Array2D(const MatrixSize &gs)
      : arr(std::vector<T>(gs.getN())), top(gs.getTop()), width(gs.getXS()) {}

  // Note: 'std::vector' gives null references if we get out of range
  //
  const T &operator[](const MatrixPos &pos) const { return arr[index(pos)]; }
  T &operator[](const MatrixPos &pos) { return arr[index(pos)]; }

private:
  unsigned index(const MatrixPos &pos) const {
    MatrixPos dp = pos - top;
    int n = dp.getY() * width + dp.getX();
#ifndef NDEBUG
    assert(n >= 0 && n <= (int)arr.size());
#endif
    return n;
  }

  // data members
  //
  std::vector<T> arr; // 1D array
  const MatrixPos top;
  const unsigned width;
};

/*---=== SubMatrix ===---
 *
 * "Peekhole" to other matrices, on a limited viewing window.
 *
 * Note: We can rely on 'm' remaining non-GC'ed while we're alive
 * ('new_SubMatrix' takes care of this).
 *
 * Note: We need to do this as C++ class (derived from 'Matrix') so that the
 *       same functions s.a. 'min', 'max', 'avg' etc. would work for a submatrix
 *       as well as the usual ones.
 */
class SubMatrix;
struct SubMatrixBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "SubMatrix"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef SubMatrix CAST_T;

private:
  static int index(lua_State *L);
};

class SubMatrix : protected Matrix, public LuaNew<SubMatrixBind> {
public:
  SubMatrix(Matrix &m_, const MatrixPos &window_size_);
  /*virtual*/ ~SubMatrix();

  /*virtual*/ float get_value_n(offset_t n) const throw();
  /*virtual*/ void set_value_n(offset_t n, float v) throw();

  /*
   * Returning nullptr here means we won't be used by SSE. That is quite fine.
   */
  /*virtual*/ const float *getData() const throw() { return 0; }
  /*virtual*/ float *getData() throw() {
    assert(false); // upper level should have checked
    return 0;
  }

  // Projection changes at every step - we just don't give any projection.
  //
  /*virtual*/ const Projection &getProjection() const {
    return Projection::NONE;
  }

  // Since we inherit 'LuaNew::instance()' twice (via 'Matrix' and directly)
  // we need to set an ambiguity straight.
  //
  static SubMatrix *instance(lua_State *L, int index) {
    return LuaNew<SubMatrixBind>::instance(L, index);
  }

  // Same ambiguity with 'operator new(L)'
  //
  static void *operator new(size_t size, lua_State *L) {
    return LuaNew<SubMatrixBind>::operator new(size, L);
  }

  static int set_pos(lua_State *L);
  static int set_radius(lua_State *L);

private:
  // data members:
  //
  Matrix &m;       // stays alive throughout our lifespan
  MatrixPos m_pos; // current position in 'm', moved by ':setpos(pos)'

  double r_km; // Points outside of this radius is seen as NAN
               // (NAN for no limit)

  const Array2D<LatLon> *latlon;
  // Latlons for all grid points of 'm' (same projection and grid size)
  // (available iff 'r_km' is given)

  SubMatrix &operator=(const Matrix &); // no assigning (from any matrix)
  SubMatrix(const Matrix &o);           // no copy constructor (from any matrix)

  friend class SubMatrixBind;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    Matrix::_INVARIANT(file, line);

    if (isnanf(r_km)) {
      assert_invariant(latlon == 0);
    } else {
      assert_invariant(latlon != 0);
    }
  }
#endif
};

#endif
// SUBMATRIX_H
