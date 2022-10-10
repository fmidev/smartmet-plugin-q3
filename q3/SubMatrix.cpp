/*
 * SUBMATRIX.CPP                    Copyright (c) 2009-10, Ilmatieteen laitos
 *
 * "Peekhole" object used by 'foreach' to iterate a parent matrix.
 */
#include "SubMatrix.h"
#include "Vector.h"
#include "VectorMatrix.h"

#include <cstring>
#include <math.h>

using namespace std;

LuaNew_ID SubMatrixBind::ID;

/*
 */
SubMatrix::SubMatrix(Matrix &m_, const MatrixPos &ws)
    : Matrix(MatrixSize(-ws, ws), m_.getUnit(), true /*read-only*/), m(m_),
      m_pos(0, 0), r_km(NAN), latlon(nullptr) {
  INVARIANT();
}

/*
 */
SubMatrix::~SubMatrix() {
  INVARIANT();

  if (latlon) {
    delete latlon;
  }
}

/*
 */
/*virtual*/ float SubMatrix::get_value_n(offset_t n) const throw() {
  xy_t xs = getSize().getXS();
  xy_t y = n / xs;
  xy_t x = n % xs;

  assert(y < getSize().getYS()); // caller wouldn't iterate over the boundaries

  MatrixPos pos = m_pos + getSize().getTop() + MatrixPos(x, y);

  // Try reading first so we find out if 'pos' lead outside of the parent matrix
  //
  float v;
  try {
    v = m[pos];
  } catch (const E_OUTSIDE &) {
    return NAN; // was outside of the parent matrix
  }

  if (!isnanf(r_km)) {
    // 'm_pos' is the "central" position of iteration within 'm' (0,0)
    // 'pos' is the real position we're trying to read (x,y)
    //
    assert(latlon);

    const LatLon &a = (*latlon)[pos];   // position where we're asking the value
    const LatLon &b = (*latlon)[m_pos]; // current 'center' position

    double dist_km = a.spherical_great_circle_distance_km(b);
    if (dist_km > r_km) {
      return NAN; // outside of the radius
    }
  }

  return v;
}

/*
 * Setting values of the peekhole are proxied to the parent.
 *
 * Writes outside of the parent matrix are quietly ignored (allows use of the
 * peekhole also at edges).
 */
/*virtual*/ void SubMatrix::set_value_n(offset_t n, float v) throw() {
  (void)n;
  (void)v;
  assert(false); // upper levels should have checked
}

/*
 * void= set_pos( submatrix_ud, matrixpos_ud )
 */
int SubMatrix::set_pos(lua_State *L) {
  SubMatrix &my = *SubMatrix::instance(L, 1);

  const MatrixPos *pos = MatrixPos::instance(L, 2);
  L_ASSERT(pos);

  my.m_pos = *pos;

  return 0; // nothing pushed
}

/*
 * void= set_radius( submatrix_ud, r_km_num [, LONLAT_vectormatrix_ud] )
 *
 * Sets a radial limit to the distance of points visible in the submatrix.
 * Points outside of this limit are shown as NAN to applications.
 *
 * The LONLAT matrix has the longitude (y), latitude (x) coordinates of the
 * parent matrix. This is an optimization because such info is needed repeatedly
 * for distance calculations.
 */
int SubMatrix::set_radius(lua_State *L) {
  SubMatrix &my = *SubMatrix::instance(L, 1);

  my.r_km = lua_tonumber(L, 2);

  // Allowing resetting the limit by using NAN (we also don't want to cause
  // non-invariant states)
  //
  if (isnanf(my.r_km)) {
    if (my.latlon)
      delete my.latlon;
    my.latlon = 0;
  } else {
    // We take the LONLAT matrix from Lua, since they're most likely having it
    // ready.
    //
    const VectorMatrix *LONLAT = VectorMatrix::instance(L, 3);
    L_ASSERT(LONLAT);

    MatrixSize gs = my.m.getSize();
    L_ASSERT(gs == LONLAT->getSize());

    Array2D<LatLon> *a = new Array2D<LatLon>(gs);

    for (MatrixIter mi(gs); mi.within(); ++mi) {
      Vector v = (*LONLAT)[mi];
      (*a)[mi] = LatLon(v.getY(), v.getX());
    }
    my.latlon =
        a; // storing 'a' in a const pointer (not to be changed any more)
  }

  return 0; // nothing pushed
}

/*
 * pos_ud= __index( m_ud, "center" )
 *
 *   ... and all 'Matrix' indices ...
 */
int SubMatrixBind::index(lua_State *L) {
  SubMatrix &my = *SubMatrix::instance(L, 1);

  // [1]: not even needed (we don't do anything to it directly)
  //
  const char *s = lua_tostring(L, 2);

  if (s) {
    if (strcmp(s, "center") == 0) {
      new (L) MatrixPos(my.m_pos);
      return 1;
    }
  }

  return MatrixBind::index(L); // forward to parent class
}

/*
 */
void SubMatrixBind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  MatrixBind::setup(L);   // fill in with 'Matrix' entries first
  MatrixBind::ID.me2(ID); // recognize us as-a 'Matrix'

  // Replace '__index' with ours
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);
}
