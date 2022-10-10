/*
 * VECTORMATRIX.CPP                          Copyright (c) 2009-10, Ilmatieteen
 * laitos
 *
 * Matrix of vector values.
 *
 * Revised:  20-Oct-2010 AKa
 */
#include "VectorMatrix.h"
#include "Grid.h"
#include "LatLon.h"
#include "Matrix.h"
#include "MemMatrix.h"
#include "RegTools.h"
#include "Tools.h"
#include "Vector.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <sstream>

using namespace std;

LuaNew_ID VectorMatrixBind::ID;

/*---=== Helpers ===---*/

/*
 * Push the X and Y components of 'b', which is either 'VectorMatrix' or
 * 'Vector'. If some other type, return false.
 */
static bool push_cartesian(lua_State *L, int b_index) {

  L_ASSERT(b_index > 0);
  L_GROW(2);

  const VectorMatrix *bm = VectorMatrix::instance(L, b_index);
  if (bm) {
    bm->push_cartesian_x(L, b_index);
    bm->push_cartesian_y(L, b_index);
  } else {
    const Vector *bv = Vector::instance(L, b_index);
    if (!bv)
      return false;

    lua_pushnumber(L, bv->getX());
    lua_pushnumber(L, bv->getY());
  }

  return true;
}

/*---=== VectorMatrix ===---*/

/*
 * Create a new 'VectorMatrix' object of given size. Component matrices are
 * given on the stack, and popped away from the stack. They are used AS-IS by
 * 'VectorMatrix' and no other references to them MUST EXIST (the user is not
 * allowed to pass them, or have a copy).
 *
 * This constructor MUST be called with 'new(L)' operator.
 */
VectorMatrix::VectorMatrix(lua_State *L, bool polar_) /*throw(E_BUG)*/
    : ApiMatrix(), LuaNew<VectorMatrixBind>(), m1(0), m2(0), m1_key(0),
      m2_key(0), polar(polar_) {

  if (this != VectorMatrix::instance(L, -1)) {
    throw E_LOG_BUG0("Caller must use 'new(L)'");
  }

  init(L); // eat up components

  INVARIANT();
}

/*
 * Constructor that makes copies of the given matrices (they are some visible
 * to the user and cannot therefore be referred to directly).
 *
 * 'polar'==false: matrices are x and y components
 * 'polar'==true:  matrices are absolute and degree (0 to N, 90 to E) values
 */
VectorMatrix::VectorMatrix(lua_State *L, const Matrix &a, const Matrix &b,
                           bool polar_) /*throw(E_BUG)*/
    : ApiMatrix(), LuaNew<VectorMatrixBind>(), m1(0), m2(0), m1_key(0),
      m2_key(0), polar(polar_) {

  const MatrixSize &size = a.getSize();
  if (size != b.getSize()) {
    throw E_LOG_BUG0(
        "Cannot construct 'VectorMatrix' from matrices of different size");
  }

  // [-1]: us ('init()' will check it really is us

  L_GROW(3);

  // Push copies (not references) of the matrices, bind to them, and remove the
  // first references.
  //
  // Note: 'param' type is carried on to the new copies.
  //
  new (L) MemMatrix(a);
  new (L) MemMatrix(b);

  // Move 'VectorMatrix' (this) topmost
  //
  lua_pushvalue(L, -3);
  lua_remove(L, -4);

  init(L); // eats the sub-matrices, keeps 'VectorMatrix'

  INVARIANT();
}

/*
 * Initialize a vector matrix with two scalar matrices.
 *
 * Lua stack (before):
 *   [-1] this object
 *   [-2] 2nd component ('Matrix')
 *   [-3] 1st component ('Matrix')
 *
 * Lua stack (after):
 *   [-1] this object
 *
 * Note: The given matrices are taken to be AS-IS the components. They are
 *       not copied, and changes to the vector matrix will be affected in
 *       these. The matrices should be temporary results (as they often are,
 *       i.e. results of additions etc) and not something the user has more
 *       references to.
 */
void VectorMatrix::init(lua_State *L) /*throw(E_BUG)*/
{
  // Make sure 'this' really is at [-1] (that is; 'new(L)' was used)
  //
  if (this != VectorMatrix::instance(L, -1)) {
    throw E_LOG_BUG0("Caller must use 'new(L)'");
  }

  /*
   * Get fast access pointers to the values (they will remain the same)
   */
  m1 = Matrix::instance(L, -3);
  m2 = Matrix::instance(L, -2);
  L_ASSERT(m1 && m2);

  // Components must be same size with each other, and with us.
  //
  L_ASSERT(m1->getSize() == getSize());
  L_ASSERT(m2->getSize() == getSize());

  /*
   * Keep references to the objects, until we're out.
   */
  m1_key = LuaNew_base::keep_alive(L, -1, -3);
  m2_key = LuaNew_base::keep_alive(L, -1, -2);

  lua_remove(L, -2);
  lua_remove(L, -2); // yes, -2 (-3 just became -2)

  INVARIANT();
}

/*
 */
Vector VectorMatrix::operator[](offset_t n) const {

  // LOG_DEBUG( "%d: %s %f %f", (int)n, polar?"polar":"xy", (*m1)[n], (*m2)[n]
  // );

  return Vector((*m1)[n], (*m2)[n], polar);
}

/*
 * Set up metatable.
 */
void VectorMatrixBind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Metamethods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushliteral(L, "__unm");
  lua_pushcfunction(L, unm);
  lua_settable(L, -3);

  lua_pushliteral(L, "__add");
  lua_pushcfunction(L, add);
  lua_settable(L, -3);

  lua_pushliteral(L, "__sub");
  lua_pushcfunction(L, sub);
  lua_settable(L, -3);

  lua_pushliteral(L, "__mul");
  lua_pushcfunction(L, mul);
  lua_settable(L, -3);

  lua_pushliteral(L, "__div");
  lua_pushcfunction(L, div);
  lua_settable(L, -3);

  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, tostring);
  lua_settable(L, -3);
}

/*
 * [vector_ud]= __index( m2_ud, pos_ud )   -- m2[pos]
 * m_ud= __index( m2_ud, "x" )       -- m2.x
 * m_ud= __index( m2_ud, "y" )       -- m2.y
 * m_ud= __index( m2_ud, "abs" )     -- m2.abs
 * m_ud= __index( m2_ud, "deg" )     -- m2.deg
 * pos_ud= __index( m2_ud, "top" )   -- m2.top
 * pos_ud= __index( m2_ud, "size" )  -- m2.size
 * [str]= __index( m2_ud, "unit" )   -- m2.unit
 * [str]= __index( m_ud, "projection" )  -- m2.projection
 * [grid_ud]= __index( m2_ud, "grid" )   -- m2.grid
 * vector_ud|NAN= __index( m2_ud, {lat,lon}|loc_str )
 * { vector_ud|NAN [,...] }= __index( m2_ud, { {lat,lon}|loc_str [, ...] } )
 *
 * Returns nil for values outside of the matrix.
 */
int VectorMatrixBind::index(lua_State *L) {
  const VectorMatrix &me = *VectorMatrix::instance(L, 1);

  typedef VectorMatrix::offset_t offset_t;

  switch (lua_type(L, 2)) {
  case LUA_TSTRING: {
    const char *s = lua_tostring(L, 2);
    assert(s);

    if (strcmp(s, "x") == 0) {
      const Matrix *mm =
          me.getMX_(); // non-nullptr if data is stored as X/Y components
      if (mm) {
        new (L) MemMatrix(*mm); // it's a copy; changes won't affect us
      } else {
        // Matrix is polar; do conversion to cartesian coordinates
        //
        MemMatrix &m = *new (L) MemMatrix(me.getSize(), me.m1->getUnit(),
                                          me.getProjection());
        offset_t n = m.getN();
        for (offset_t i = 0; i < n; i++) {
          m[i] = me[i].getX();
        }
      }
      return 1;
    } else if (strcmp(s, "y") == 0) {
      const Matrix *mm =
          me.getMY_(); // non-nullptr if data is stored as X/Y components
      if (mm) {
        new (L) MemMatrix(*mm); // it's a copy; changes won't affect us
      } else {
        // Take the 'abs' component's (m1) unit for both X and Y
        //
        MemMatrix &m = *new (L) MemMatrix(me.getSize(), me.m1->getUnit(),
                                          me.getProjection());
        offset_t n = m.getN();
        for (offset_t i = 0; i < n; i++) {
          m[i] = me[i].getY();
        }
      }
      return 1;
    } else if (strcmp(s, "abs") == 0) {
      const Matrix *mm =
          me.getAbs_(); // non-nullptr if data is stored as polar coords
      if (mm) {
        new (L) MemMatrix(*mm); // it's a copy; changes won't affect us
      } else {
        // Both 'm1' and 'm2' should have the same unit
        //
        MemMatrix &m = *new (L) MemMatrix(me.getSize(), me.m1->getUnit(),
                                          me.getProjection());
        offset_t n = m.getN();
        for (offset_t i = 0; i < n; i++) {
          m[i] = me[i].getAbs();
        }
      }
      return 1;
    } else if (strcmp(s, "deg") == 0) {
      const Matrix *mm =
          me.getDeg_(); // non-nullptr if data is stored as polar coords
      if (mm) {
        new (L) MemMatrix(*mm); // it's a copy; changes won't affect us
      } else {
        MemMatrix &m = *new (L) MemMatrix(me.getSize(), NA_Param::UNIT_DEG,
                                          me.getProjection());
        offset_t n = m.getN();
        for (offset_t i = 0; i < n; i++) {
          m[i] = me[i].getDeg();
        }
      }
      return 1;
    } else if (strcmp(s, "top") == 0) {
      new (L) MatrixPos(me.m1->getSize().getTop());
      return 1;
    } else if (strcmp(s, "size") == 0) {
      new (L) MatrixPos(me.m1->getGridSize());
      return 1;
    } else if (strcmp(s, "unit") == 0) {
      lua_pushstring(L, me.m1->getUnitName().c_str()); // may be nullptr
      return 1;
    } else if (strcmp(s, "projection") == 0) {
      lua_pushstring(L, me.getProjection().toString().c_str());
      return 1;
    } else if (strcmp(s, "grid") == 0) {
      if (LuaNew_base::push_alive(L, 1, me.getGridKey())) {
        L_ASSERT(Grid::instance(L, -1));
        return 1;
      }
      return 0; // no grid for this matrix
    }
  } break; // go try location indices (s.a. 'Helsinki')

  case LUA_TUSERDATA: {
    const MatrixPos *mp = MatrixPos::instance(L, 2);
    if (mp) {
      try {
        new (L) Vector((*me.m1)[*mp], (*me.m2)[*mp], me.polar);
      } catch (...) { // "Reading outside of matrix"
        lua_pushnil(L);
      }
      return 1;
    }
  } break;
  }

  // Check for location indices
  //
  // Note: 'Matrix.cpp' has similar code (if you make changes, change both!)
  //
  LatLonList locs;
  LatLonList::e_state st = locs.init_from_ud(L, 2);

  if (st == LatLonList::NONE) {
    luaL_error(L, "Bad index (for matrix): %s",
               lua_isstring(L, 2) ? lua_tostring(L, 2) : L_typename(2));
  }

  // 'st' tells whether we should return the results in a wrapping table
  // or not ('{lat,lon}' pushes a number but '{ {lat,lon} }' pushes {number}).
  //
  if (st == LatLonList::PUSH_AS_VALUE) {
    double a = me.m1->at(locs[0]);
    double b = me.m2->at(locs[0]);

    new (L) Vector(a, b, me.isPolar());

  } else {
    lua_newtable(L);
    unsigned i = 1;
    for (vector<LatLon>::const_iterator it = locs.begin(); it != locs.end();
         ++it) {
      lua_pushinteger(L, i++);

      double a = me.m1->at(*it);
      double b = me.m2->at(*it);

      new (L) Vector(a, b, me.isPolar());

      lua_settable(L, -3);
    }
  }
  return 1;
}

/*
 * void= __newindex( m2_ud, pos_ud, vector_ud )     -- m2[ pos_ud ]= vector_ud
 */
int VectorMatrixBind::newindex(lua_State *L) {
  VectorMatrix &m2 = *VectorMatrix::instance(L, 1); // we'll modify it.

  MatrixPos *pos = MatrixPos::instance(L, 2);
  if (!pos) {
    luaL_error(L, "Bad index for matrix: %s", L_typename(2));
  }

  const Vector *v = Vector::instance(L, 3);
  if (!v) {
    luaL_error(L, "Bad value (not a vector): %s", L_typename(2));
  }

  bool polar = m2.isPolar();

  try {
    (*m2.m1).set_value(*pos, polar ? v->getAbs() : v->getX());
    (*m2.m2).set_value(*pos, polar ? v->getDeg() : v->getY());
  } catch (runtime_error e) {
    luaL_error(L, e.what());
  }

  return 0; // nothing pushed
}

/*
 * m2_ud= __unm( m2_ud )
 */
int VectorMatrixBind::unm(lua_State *L) {
  VectorMatrix *a = VectorMatrix::instance(L, 1);
  L_ASSERT(a);

  const int a_index = 1;
  bool polar = a->isPolar();

  if (polar) {
    // Abs/dir pairs are negated by turning the direction by 180 degrees
    //
    L_GROW(4);
    LuaNew_base::push_alive(L, a_index, a->m1_key); // abs remains as is

    lua_pushcfunction(L, Matrix::unm_deg);
    LuaNew_base::push_alive(L, a_index, a->m2_key);
    lua_call(L, 1 /*args*/, 1 /*results*/);
    //
    // [-1]: deg component
    // [-2]: abs component

  } else {
    L_GROW(4);
    lua_pushcfunction(L, Matrix::unm);
    LuaNew_base::push_alive(L, a_index, a->m1_key);
    lua_call(L, 1 /*args*/, 1 /*results*/);

    lua_pushcfunction(L, Matrix::unm);
    LuaNew_base::push_alive(L, a_index, a->m2_key);
    lua_call(L, 1 /*args*/, 1 /*results*/);
    //
    // [-1]: Y component (-a.y)
    // [-2]: X component (-a.x)
  }

  new (L) VectorMatrix(L, polar); // eat components
  return 1;
}

/*
 * m2_ud= __add( m2_ud, vector_ud|m2_ud|(any) )
 * m2_ud= __add( vector_ud|m2_ud|(any), m2_ud )
 */
int VectorMatrixBind::add(lua_State *L) {
  VectorMatrix *a = VectorMatrix::instance(L, 1);
  VectorMatrix *b = VectorMatrix::instance(L, 2);

  if (!a) {
    // Swap parameters around
    //
    L_ASSERT(lua_gettop(L) == 2); // 'add' cannot have more params
    lua_insert(L, 1);             // swaps [1] and [2]
    a = b;
    b = 0;
  }
  L_ASSERT(a);

  const int a_index = 1;
  const int b_index = 2;

  L_GROW(6);

  if (!push_cartesian(L, b_index)) {
    luaL_error(L, "Cannot add %s and %s", L_typename(1), L_typename(2));
  }
  // [-1]: B Y component to add (matrix or number)
  // [-2]: B X component to add (matrix or number)

  const int by_index = lua_gettop(L);
  const int bx_index = by_index - 1;

  lua_pushcfunction(L, Matrix::add);
  a->push_cartesian_x(L, a_index);
  lua_pushvalue(L, bx_index);
  lua_call(L, 2 /*args*/, 1 /*results*/);
  //
  // [-1]: X component to return

  lua_pushcfunction(L, Matrix::add);
  a->push_cartesian_y(L, a_index);
  lua_pushvalue(L, by_index);
  lua_call(L, 2 /*args*/, 1 /*results*/);
  //
  // [-1]: Y component to return
  // [-2]: X component to return
  // [-3]: (leftover; B Y)
  // [-4]: (leftover; B X)

  new (L) VectorMatrix(L, false /*cartesian*/); // eats up components

  // Note: We still have BX and BY components on the stack (at [-2] and [-3]).
  //       They can be left there, Lua clears them out automatically:
  //
  //       "[When exiting] any other value in the stack below the results will
  //       be properly discarded by Lua." (Lua 5.1 reference manual)

  return 1;
}

/*
 * m2_ud= __sub( m2_ud, m2_ud|vector_ud|(any) )
 * m2_ud= __sub( m2_ud|vector_ud|(any), m2_ud )
 *
 * Turn subtraction to 'a + (-b)'
 */
int VectorMatrixBind::sub(lua_State *L) {

  const int a_index = 1;
  const int b_index = 2;

  L_GROW(4);

  // Push '-b' onto the stack ('VectorMatrix' or 'Vector')
  //
  if (VectorMatrix::instance(L, b_index)) {
    lua_pushcfunction(L, VectorMatrixBind::unm);
  } else if (Vector::instance(L, b_index)) {
    lua_pushcfunction(L, VectorBind::unm);
  } else {
    luaL_error(L, "Cannot subtract %s and %s", L_typename(1), L_typename(2));
  }

  lua_pushvalue(L, b_index);
  lua_call(L, 1 /*args*/, 1 /*results*/);
  //
  // [-1]: -b ('VectorMatrix' or 'Vector')

  lua_pushcfunction(L, VectorMatrixBind::add);
  lua_pushvalue(L, a_index);
  lua_pushvalue(L, -3); // -b
  lua_call(L, 2 /*args*/, 1 /*results*/);

  return 1; // Lua will clean other pushed values
}

/*
 * m2_ud= __mul( m2_ud, any )
 * m2_ud= __mul( any, m2_ud )
 *
 * m_ud= __mul( m_ud, any )      --> relayed to 'MatrixBind'
 * m_ud= __mul( any, m_ud )
 *
 *   VectorMatrix * Matrix:      multiply each vector with the scalar of that
 * position Matrix * VectorMatrix:      -''- VectorMatrix * number: multiply
 * each vector with the global scalar number * VectorMatrix:      -''-
 *
 * Note: Also plain 'Matrix' has us as the metamethod, because the method
 *       is determined by the first parameter's data type. This way, we can
 *       get 'Matrix * VectorMatrix' and make it behave like 'VectorMatrix *
 * Matrix'.
 */
int VectorMatrixBind::mul(lua_State *L) {
  VectorMatrix *a = VectorMatrix::instance(L, 1);
  VectorMatrix *b = VectorMatrix::instance(L, 2);

  if (!a) {
    if (!b) {
      return Matrix::mul(L);
    }

    // Swap parameters around
    //
    L_ASSERT(lua_gettop(L) == 2); // 'mul' cannot have more params
    lua_insert(L, 1);             // swaps [1] and [2]
    a = b;
    b = 0;
  }
  L_ASSERT(a);

  // const int a_index= 1;
  const int b_index = 2;

  const MatrixPos gs = a->getGridSize();

  L_GROW(6);

  /**
      if (push_cartesian( L, b_index )) {
          // Multiplying with vectors ('VectorMatrix' or 'Vector'), use
  cartesian coordinates
          //
          // [-1]: B Y component (matrix or number)
          // [-2]: B X component (matrix or number)

          const int by_index= lua_gettop(L);
          const int bx_index= by_index-1;

          lua_pushcfunction( L, Matrix::mul );
          a->push_cartesian_x( L, a_index );
          lua_pushvalue( L, bx_index );
          lua_call( L, 2 /_*args*_/, 1 /_*results*_/ );

          lua_pushcfunction( L, Matrix::mul );
          a->push_cartesian_y( L, a_index );
          lua_pushvalue( L, by_index );
          lua_call( L, 2 /_*args*_/, 1 /_*results*_/ );
              //
              // [-1]: Y component to return
              // [-2]: X component to return
              // [-3]: (trash; B Y)
              // [-4]: (trash; B X)

          new(L) VectorMatrix(L, false, false);  // cartesian
          return 1;

      } else
  **/
  {
    // Multiplying with scalars ('Matrix' or number), use polar _or_ cartesian
    //
    const Matrix *mm = Matrix::instance(L, b_index);
    double d = 0.0;

    if (mm) {
      if (mm->getGridSize() != a->getGridSize()) {
        luaL_error(L, "Gridsize mismatch: (%d,%d) != (%d,%d)",
                   (int)mm->getGridSize().getX(), (int)mm->getGridSize().getY(),
                   gs.getX(), gs.getY());
      }
    } else if (lua_isnumber(L, b_index)) {
      d = lua_tonumber(L, b_index);
    } else {
      luaL_error(L, "Cannot multiply %s and %s", L_typename(1), L_typename(2));
    }

    bool polar = a->isPolar();

    MemMatrix *m_v1 =
        new (L) MemMatrix(gs, a->m1->getUnit(), a->getProjection());
    MemMatrix *m_v2 = new (L) MemMatrix(
        gs, polar ? a->m1->getUnit() : a->m2->getUnit(), a->getProjection());

    MatrixPos::offset_t n = gs.getN();
    for (MatrixPos::offset_t i = 0; i < n; i++) {
      // Using 'Vector' operator * takes care of polar / cartesian cases for us.
      //
      Vector v = (*a)[i] * (mm ? (*mm)[i] : d);
      (*m_v1)[i] = v.getV1(); // x or abs
      (*m_v2)[i] = v.getV2(); // y or deg
    }

    new (L) VectorMatrix(L, polar);
    return 1;
  }
}

/*
 * m2_ud= reciprocal( m2_ud )
 *
 * Note: Only '1/b' is currently needed and excercised; 'VectorMatrix' converts
 * any divisions 'a/b' to 'a*(1/b)' and calls us only to get the reciprocal.
 */
#if 0 // NOT NEEDED
int VectorMatrixBind::reciprocal( lua_State *L ) {
    const VectorMatrix* a= VectorMatrix::instance(L,1);
    assert(a);

    // Always reciprocate 'm1' (abs or x)
    //
    lua_pushcfunction( L, Matrix::reciprocal );
    new(L) MemMatrix( *(a->m1) );       // make a copy
    lua_call( L, 1 /*args*/, 1 /*retvals*/ );

    bool polar= a->isPolar();
    if (polar) {
        // We can use the same underlying 'deg' vector, without copying. These are not
        // modified and Lua GC handles the references.
        //
        lua_pushvalue( L, a->m2_key );

    } else {
        // Cartesian. Also 'y' needs to be separately reciprocated
        //
        lua_pushcfunction( L, Matrix::reciprocal );
        new(L) MemMatrix( *(a->m2) );       // make a copy
        lua_call( L, 1 /*args*/, 1 /*retvals*/ );
    }

    // [-1]: new m2 (or 2nd ref to unmodified original)
    // [-2]: new m1

    new(L) VectorMatrix(L, polar, false);
    return 1;
}
#endif

/*
 * m2_ud= __div( m2_ud, any )
 * m2_ud= __div( any, m2_ud )
 *
 * Turn division into 'a * 1/b' (this is the simplest way of dealing with
 * division, and gives the right results).
 *
 *   VectorMatrix / VectorMatrix|Vector:    divide each vector's components with
 * each other (used by 'grad()') VectorMatrix / Matrix:      divide each vector
 * with the scalar of that position VectorMatrix / number:      divide each
 * vector with the global scalar
 *
 * Division by zero causes 'Vector::Inf' as result (no exceptions thrown).
 *
 * Note: Also plain 'Matrix' has us as the metamethod, because the method
 *       is determined by the first parameter's data type. This way, we can
 *       get 'Matrix * VectorMatrix' and make it behave like 'VectorMatrix *
 * Matrix'.
 */
int VectorMatrixBind::div(lua_State *L) {

  if ((!VectorMatrix::instance(L, 1)) && (!Matrix::instance(L, 1))) {
    luaL_error(L, "Cannot divide %s by %s", L_typename(1), L_typename(2));
  }

  const int a_index = 1;
  const int b_index = 2;

  L_GROW(4);

  // Push '1/b' onto the stack ('Matrix' or number)
  //
  if (lua_isnumber(L, b_index)) {
    double d = lua_tonumber(L, b_index);
    lua_pushnumber(L, (d == 0.0) ? INFINITY : 1.0 / d);

  } else if (Matrix::instance(L, b_index)) {
    lua_pushcfunction(L, Matrix::reciprocal);
    lua_pushvalue(L, b_index);
    lua_call(L, 1 /*args*/, 1 /*results*/);
    //
    // [-1]: 1/b ('Matrix'); with INF at places of divide-by-zero

#if 0 // NOT NEEDED
    } else if (VectorMatrix::instance(L,b_index)) {
        lua_pushcfunction( L, VectorMatrixBind::reciprocal );
        lua_pushvalue( L, b_index );
        lua_call( L, 1 /*args*/, 1 /*results*/ );
#endif
  } else {
    luaL_error(L, "Cannot divide %s by %s", L_typename(1), L_typename(2));
  }

  lua_pushcfunction(L, VectorMatrixBind::mul);
  lua_pushvalue(L, a_index);
  lua_pushvalue(L, -3); // 1/b
  lua_call(L, 2 /*args*/, 1 /*results*/);

  return 1; // Lua will clean other pushed values
}

/*
 * Outputting a 'VectorMatrix' as string
 *
 * Format can be agreed upon (Q2 did not have matrix-of-vectors)
 *
 *   "<x_size>,<y_size>;(<x1> <y1>)[,(<x2> <y2>)[, ...]]"   (no linefeeds)
 */
void VectorMatrix::asString(ostream &out, int decimals) const {
  out << getSize().getXS() << "," << getSize().getYS() << ";";

  if (decimals >= 0) {
    out.precision(decimals);
    out << fixed;
  }

  offset_t n = getSize().getN();
  for (offset_t i = 0; i < n; i++) {
    if (i > 0)
      out << ',';

    // If either value is NAN, there is no vector
    //
    const Vector &v = (*this)[i];
    float vx = v.getX();
    float vy = v.getY();

    if (isnanf(vx) || isnanf(vy)) {
      // Output nothing
    } else {
      out << '(' << vx << ' ' << vy << ')';
    }
  }
}

/*
 * string= mt.__tostring( obj )
 */
int VectorMatrixBind::tostring(lua_State *L) {

  // Note: Malign client scripts can call this function with surprising
  // parameters;
  //      make sure we don't crash.
  //
  const VectorMatrix &me = *VectorMatrix::instance(L, 1);
  if (!&me) {
    throw E_LOG_USAGE("Bad parameter: %s",
                      L_typename(1)); // internal bug or deliberate hack attempt
  }

  unsigned decs = RegTools::get_Decimals(L);

  // TBD: If we need the stringification only here, use 'lua_pushfstring()' etc.
  // for doing it.
  //
  stringstream ss;
  me.asString(ss, decs);

  lua_pushstring(L, ss.str().c_str());
  return 1;
}

/*
 * Push an X or Y component.
 */
void VectorMatrix::push_cartesian(lua_State *L, int b_index,
                                  bool choose_x) const {
  L_ASSERT(b_index > 0);
  L_ASSERT(VectorMatrix::instance(L, b_index) == this);

  if (!isPolar()) {
    // Push a native component onto the stack (just reference)
    //
    LuaNew_base::push_alive(L, b_index, choose_x ? m1_key : m2_key);
  } else {
    // Need to create whole new Matrix, and fill with X or Y
    //
    MemMatrix *m = new (L)
        MemMatrix(getGridSize(), m1->getUnit() /*abs unit*/, getProjection());

    offset_t n = getGridSize().getN();
    for (offset_t i = 0; i < n; i++) {
      Vector v((*this)[i]);
      (*m)[i] = choose_x ? v.getX() : v.getY();
    }
  }
}

/*
 * Copy by value
 *
 * TBD: What to do if the projections and/or grid sizes are different?
 */
#ifdef METQU
void VectorMatrix::operator=(const VectorMatrix &o) {

  // Setting a matrix of vectors with another (but they may be polar/cartesian
  // different)
  //
  if ((polar && o.isPolar()) || ((!polar) && (!o.isPolar()))) {
    m1->copy_from(*(o.getM1())); // copies by value
    m2->copy_from(*(o.getM2())); // -''-

  } else {
    // Go through value by value and let 'Vector' do the conversion
    //
    MatrixSize gs = getSize();
    MatrixSize o_gs = o.getSize();

    if (gs != o_gs) {
      throw E_LOG_USAGE0("Trying to write vector matrices of differing size");
      //
      // NB. We could do fitting (but what about projections?)
    }

    for (MatrixIter mi(gs); mi.within(); ++mi) {
      Vector v = o[mi];
      m1->set_value(mi, polar ? v.getAbs() : v.getX());
      m2->set_value(mi, polar ? v.getDeg() : v.getY());
    }
  }
}
#endif

/*
 * Fill with a value
 */
#ifdef METQU
void VectorMatrix::operator=(const Vector &v) {

  // If necessary, 'Vector' methods take care of the calculations.
  //
  if (polar) {
    m1->fill_with(v.getAbs());
    m2->fill_with(v.getDeg());
  } else {
    m1->fill_with(v.getX());
    m2->fill_with(v.getY());
  }
}
#endif
