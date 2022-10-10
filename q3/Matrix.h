/*
 * MATRIX.H                      Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * There are multiple kinds of matrices, all derived from 'Matrix' (which is the
 * abstract API).
 *
 *   - MemMatrix is a temporary matrix for calculations, or based on a
 *     projection (time and/or locationwise) from Query Data.
 *
 *   - SQD_Matrix is a 1-to-1 proxy to SQD file data. It is positioned to a
 *     certain level, time, parameter and provides data in native projection
 *     and gridsize.
 *
 *   - MQD_Matrix is similar proxy matrix to MQD file data.
 *
 *   - SubMatrix is a 'peekhole' to a portion of a bigger, host matrix.
 *     Used in iteration.
 *
 *   - VectorMatrix carries references to two scalar matrices (of any particular
 *     type, does not matter). To the script it looks like a matrix of vectors.
 *
 *   - ApiMatrix is either a derivative of 'Matrix' (scalar matrix) or
 * 'VectorMatrix'.
 *
 * Scripts deal with matrices either as 'Matrix' (scalars) or 'VectorMatrix'.
 * Many operations work alike for the both, but they are distinguisable i.e. by
 * their type names.
 *
 * 'MatrixPos', 'MatrixSize' and 'MatrixIter' are helper classes for iterating
 * through the matrices.
 *
 * Revised:  20-Oct-2010 AKa
 */
#ifndef MATRIX_H
#define MATRIX_H

#include "MatrixPos.h"
#include "Raw.h"

#include "Tools.h"
// string_or_null

#include "LatLon.h"

#include "newbase/NFmiLocation.h"
#include "newbase/NFmiPoint.h"

#include <ostream>

#include <memory>

class Projection;

/*---=== MatrixSize ===---
 */
class MatrixSize;

struct MatrixSizeBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "MatrixSize"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef MatrixSize CAST_T;

private:
  static int index(lua_State *L);
  static int eq(lua_State *L);
  static int tostring(lua_State *L);
};

/*
 * Gives two corners of a matrix; top and bottom (both inclusive).
 */
class MatrixSize : public LuaNew<MatrixSizeBind> {
public:
  MatrixSize(const MatrixPos &top_, const MatrixPos &bottom_)
      : top(top_), bottom(bottom_) {
    INVARIANT();
  }

  MatrixSize(const MatrixPos &gridsize)
      : top(MatrixPos::ZERO), bottom(gridsize - MatrixPos::DXDY) {
    INVARIANT();
  }

  MatrixSize(const MatrixSize &o)
      : LuaNew_base(), LuaNew<MatrixSizeBind>(), top(o.top), bottom(o.bottom) {}

  ~MatrixSize() {}

  const MatrixPos &getTop() const { return top; }
  const MatrixPos &getBottom() const { return bottom; }

  bool operator==(const MatrixSize &o) const {
    return (top == o.top) && (bottom == o.bottom);
  }
  bool operator!=(const MatrixSize &o) const { return !(*this == o); }

  MatrixPos::xy_t getXS() const { return bottom.getX() - top.getX() + 1; }
  MatrixPos::xy_t getYS() const { return bottom.getY() - top.getY() + 1; }
  MatrixPos::offset_t getN() const { return getXS() * getYS(); }

  std::string asString() const;

private:
  MatrixSize(); // not allowed

  // data members
  MatrixPos top, bottom; // either symmetrix (-x,-y)..(x,y)
                         // or (0,0)..(x,y)

  friend class MatrixSizeBind;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    // Either (0,0)..(x,y) or (-x,-y)..(x,y); x and/or y can be zero
    //
    if (top == MatrixPos::ZERO) {
      assert_invariant(bottom.getX() >= 0);
      assert_invariant(bottom.getY() >= 0);
    } else {
      assert_invariant(bottom == -top);
    }
  }
#endif
};

/*---=== MatrixIter ===---
 *
 * Iterator for going through a matrix.
 *
 * The iteration format is FIXED to be y in outer loop ([0] is northmost), x in
 * inner
 * ([0] is westmost). This order directly affects i.e. matrix output and must
 * not be changed! To change it, make another class.
 */
class Matrix;
class MatrixIter;

struct MatrixIterBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "MatrixIter"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef MatrixIter CAST_T;

private:
  static int index(lua_State *L);
};

class MatrixIter : public MatrixPos, public LuaNew<MatrixIterBind> {
public:
  // Iteration order is x loops tighter, y in outer rounds
  // (see also <,<=,>,>= operators in 'MatrixPos' if changing this!)
  //
  MatrixIter &operator++() {
    if (++x > size.getBottom().getX()) {
      x = size.getTop().getX();
      ++y;
    }
    return *this;
  }

  // Are we iterating within the matrix?
  //
  bool within() const { return y <= size.getBottom().getY(); }

  MatrixIter(const MatrixSize &size_);
  MatrixIter(const MatrixIter &o)
      : LuaNew_base(), MatrixPos(o), LuaNew<MatrixIterBind>(), size(o.size) {}
  ~MatrixIter() {}

  static int q3_points_iterator(lua_State *L);

  // Since we inherit 'LuaNew::instance()' twice (via 'MatrixPos' and directly)
  // we need to set an ambiguity straight.
  //
  static MatrixIter *instance(lua_State *L, int index) {
    return LuaNew<MatrixIterBind>::instance(L, index);
  }

  // Same ambiguity with 'operator new(L)'
  //
  static void *operator new(size_t size, lua_State *L) {
    return LuaNew<MatrixIterBind>::operator new(size, L);
  }

private:
  MatrixIter(); // not allowed

  // data members
  const MatrixSize size;

  friend class MatrixIterBind;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(x >= size.getTop().getX());
    assert_invariant(x <= size.getBottom().getX());

    assert_invariant(y >= size.getTop().getY());
    // 'y' can grow out of the valid area
  }
#endif
};

/*---=== ApiMatrix ===---
 *
 * Both 'Matrix' and 'VectorMatrix' have the '.grid' property to climb back to
 * their origin (and to sister matrices). This class encapsulates that
 * behaviour.
 *
 * Also, they have the 'getProjection()' method for doing '[loc]' indexing. Note
 * that projection can be supported for also calculated temporary matrices that
 * would not have 'grid'.
 */
class ApiMatrix {
protected:
  ApiMatrix() : grid_key(0) { INVARIANT(); }
  virtual ~ApiMatrix();

public:
  void setGridKey(unsigned key) const { grid_key = key; }
  unsigned getGridKey() const { return grid_key; }

  /* Note: Some matrices (s.a. 'SubMatrix') can have altering projections
   *       based on their internal state (location pointing to in another
   * matrix). Such matrices will return an empty projection.
   */
  virtual const Projection &getProjection() const = 0;

  void push_tostring(lua_State *L) const;

  virtual void asString(std::ostream &os, int decs) const = 0;

  // Even the binary format needs decimals.
  //
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  virtual void asBinary_q2_(std::ostream &, int) const {
    // Binary storage is only for scalar matrices (not matrix of vector).
    //
    throw E_LOG_USAGE0("Binary storage not supported for this matrix.");
  }
#endif

  virtual bool is_2d() const = 0;

private:
  // 'grid_key' is not known at construction time; we need to be able to set it
  // also for 'const' matrices, thus it is 'mutable' (does not change the
  // available matrix itself).
  //
  mutable unsigned
      grid_key; // reference for getting to 'Grid' and sister parameters

#ifndef NDEBUG
  void _INVARIANT(const char *, unsigned) const {
    // nothing to check
  }
#endif
};

/*---=== Matrix ===---
 *
 * Common Lua interface for 'MemMatrix', 'SQD_Matrix', 'MQD_Matrix',
 * 'MaskMatrix'. Derived for 'SubMatrix'.
 */
class Matrix;
struct MatrixBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "Matrix"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef Matrix CAST_T;

  // Keep public to allow 'SubMatrix::index()' to call it (we could also pick
  // the '__index' value in 'SubMatrix::setup()' and remember it, but that gets
  // kind of complex)
  //
public:
  static int index(lua_State *L);

private:
  static int newindex(lua_State *L);
  static int tostring(lua_State *L);
};

/*
 * Note: Some matrices are read-only by nature (if memory mapped from disk).
 * Others can be made read-only by setting 'is_readonly' to true. READ ONLY IS
 * NOT CHECKED at every write operation; instead the upper script interfacing
 * level is expected to check it. This is to avoid numerous unnecessary checks
 * at the low level. In other words, write operations should never be attempted
 * if 'is_readonly' is true.
 */
class NFmiGrid;
class Matrix : public ApiMatrix, public LuaNew<MatrixBind> {
public:
  /*virtual*/ ~Matrix() {}

protected:
  Matrix(const MatrixSize &size_, const NA_Param::Unit &unit_, bool ro) throw()
      : ApiMatrix(), LuaNew<MatrixBind>(), size(size_), unit(unit_),
        is_readonly(ro) { /* our invariant called by derived classes */
  }

  Matrix(const MatrixPos &gridsize, const NA_Level &level_,
         FmiParameterName param_, const NA_Param::Unit &unit_, bool ro) throw()
      : ApiMatrix(), LuaNew<MatrixBind>(), size(gridsize), level(level_),
        param(param_), unit(unit_),
        is_readonly(ro) { /* our invariant called by derived classes */
  }

  Matrix(const NA_Param::Unit &unit_, bool ro) throw()
      : ApiMatrix(), LuaNew<MatrixBind>(), size(MatrixPos::ZERO), unit(unit_),
        is_readonly(ro) { /* our invariant called by derived classes */
  }

  typedef MatrixPos::xy_t xy_t;
  typedef MatrixPos::offset_t offset_t;

  /*
   * Read operations
   */
public:
  const MatrixSize &getSize() const { return size; }
  offset_t getN() const { return size.getN(); } // shortens higher level code

  MatrixPos getGridSize() const {
    return MatrixPos(size.getXS(), size.getYS());
  }

  bool isReadOnly() const { return is_readonly; }

  // Called other than 'operator[]' to allow this to be virtual and
  // 'operator[](const Matrixpos&)' not (otherwise, if we are, it must, though
  // we don't wish it to be). Got it? :)
  //
  virtual float get_value_n(offset_t n) const throw() = 0;

  float operator[](offset_t n) const throw() {
    return get_value_n(n);
  } // forward to class specific function

  float operator[](const MatrixPos &mi) const throw(E_OUTSIDE) {
    MatrixPos p =
        mi - getSize().getTop(); // move coordinates so that (0,0) is [0]
    return get_value_n(offset(p));
  }

  void setGrid(std::shared_ptr<NFmiGrid> grid) { wantedGrid = grid; }
  std::shared_ptr<NFmiGrid> getGrid() { return wantedGrid; }

  /*
   * Write operations.
   */
  void copy_from(const Matrix &m) throw(E_READONLY);

  void fit_from_same_projection(const Matrix &m) throw(E_READONLY);
  void fit_from_(const Matrix &m) throw(E_READONLY);

  float at_(double dx, double dy) const throw();

#ifdef METQU
  void fill_with(float v) throw(E_READONLY);
#endif

  /*
   * Virtual function; the actual setter.
   *
   * Using different names to keep one non-virtual and the other virtual.
   *
   * Assumes: 'n' is a valid index (never outside).
   *
   * Returns: true if write succeeded. false if a read-only matrix.
   *
   * Note: Needs to be of different name than 'set_value()' because
   *       this is virtual and 'set_value()' is not.
   */
  virtual void set_value_n(offset_t n, float v) throw() = 0;

  /*
   * Ease of use - function that leads to (virtual) 'set_value_n()'.
   *
   * Returns 'true' if the write succeeds; 'false' if read-only matrix
   *       Throws an exception if 'pos' is outside of bounds.
   */
  void set_value(const MatrixPos &pos, float v) throw(E_OUTSIDE) {
    assert(!is_readonly);
    set_value_n(offset(pos - size.getTop()), v);
  }

  /*
   * Block access (for SSE enabled compilation must be 16-byte aligned)
   */
  virtual const float *getData() const throw() = 0;
  virtual float *getData() throw() = 0;

  FmiParameterName getParam() const { return param; }
  string_or_null getParamStr() const {
    if (param == 0 /*kFmiBadParam*/)
      return "";

    std::ostringstream s;
    s << ":" << (unsigned long)param;

    return s.str();
  }
  NA_Level getLevel() const { return level; }
  string_or_null getLevelTypeStr() const {
    if (level.isPressureLevel())
      return "hpa";
    else if (level.isHybridLevel())
      return "hybrid";
    else if (level.isHeightLevel())
      return "height";

    return "";
  }
  double getLevelValue() const { return level.getValue(); }

  const NA_Param::Unit &getUnit() const { return unit; }
  string_or_null getUnitName() const { return unit.getUnitName(); }

  virtual bool is_2d() const { return false; }

  // open for 'VectorMatrix'
public:
  static int add(lua_State *L);
  static int sub(lua_State *L);
  static int mul(lua_State *L);
  static int unm(lua_State *L);
  static int unm_deg(lua_State *L);
  static int reciprocal(lua_State *L);

private:
  static int mod(lua_State *L);
  static int pow(lua_State *L);

  // open for 'Q3Session'
public:
  static int q3_abs(lua_State *L);
  static int q3_ceil(lua_State *L);
  static int q3_cos(lua_State *L);
  static int q3_floor(lua_State *L);
  static int q3_fmod(lua_State *L);
  static int q3_log(lua_State *L);
  static int q3_log10(lua_State *L);
  static int q3_max(lua_State *L);
  static int q3_min(lua_State *L);
  static int q3_modf(lua_State *L);
  static int q3_sin(lua_State *L);
  static int q3_tan(lua_State *L);

  static int q3_count(lua_State *L);
  static int q3_sum_or_avg(lua_State *L);
  static int q3_set(lua_State *L);

  static int has_missing(lua_State *L);
  static int size_f(lua_State *L);

  /*virtual*/ void asString(std::ostream &out, int decimals) const;
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  /*virtual*/ void asBinary_q2_(std::ostream &out, int decimals) const;
#endif

  float at(const LatLon &latlon) const;

  // 12-Mar-2012 PKi: Made public
  float reduce_max() const;
  float reduce_min() const;

  int offsetPosition(lua_State *L, const NFmiPoint &location,
                     const MatrixPos &offset) const;
  int offsetPosition(lua_State *L, const Matrix &m, const MatrixPos &pos,
                     const MatrixPos &offset) const;
  int offsetPosition(lua_State *L, const NFmiLocation &location,
                     double xoffsetkm, double yoffsetkm) const;
  int offsetPosition(lua_State *L, const Matrix &m, const MatrixPos &pos,
                     double xoffsetkm, double yoffsetkm) const;

protected:
  offset_t offset(const MatrixPos &pos) const {
    xy_t x = pos.getX();
    xy_t y = pos.getY();
    xy_t xs = size.getXS();
    xy_t ys = size.getYS();

    if ((x < 0) || (y < 0) || (x >= xs) || (y >= ys)) {
      throw E_OUTSIDE();
    }
    return ((offset_t)y) * xs + x;
  }

  void resize(const MatrixPos &gs) {
    size = MatrixSize(gs);
  } // used by 'MQD_Matrix' constructor

private:
  // 12-Mar-2012 PKi: Made public
  //  float reduce_max() const;
  //  float reduce_min() const;
  float reduce_sum(bool avg_mode) const;
  unsigned reduce_count() const;

  Matrix();               // no such
  Matrix(const Matrix &); // no such

  MatrixSize size; // matrices are resizeable (used by 'MQD_Matrix' constructor)

  // Information of the level and parameter we're carrying
  //
  NA_Level level;
  FmiParameterName param;
  const NA_Param::Unit unit;

protected:
  /*const*/ bool
      is_readonly; // 'MemMatrix' can be made read-only after initialization

  std::shared_ptr<NFmiGrid> wantedGrid;

private:
  friend class MatrixBind;

#ifndef NDEBUG
protected:
  void _INVARIANT(const char *file, unsigned line) const {

    // Common check that all derived classes are expected to do.
    //
#ifdef __SSE__
    const float *p = getData();
    if (p) {
      assert_invariant(((size_t)p) % 16 == 0);
    }
#else
    (void)file;
    (void)line;
#endif
  }
#endif
};

#endif
// MATRIX_H
