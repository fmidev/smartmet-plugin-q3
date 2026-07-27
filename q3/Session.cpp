/*
 * SESSION.CPP                            Copyright (c) 2008-2010, Ilmatieteen
 * laitos
 *
 * Handling one query; initialization, progress query, optional cancellation,
 * result passing and cleanup of Lua state (common for server and command line).
 *
 * Revised: 12-Nov-2010
 */
#include "Session.h"

#include "ApiParam.h"
#include "Contour.h"
#include "Grid.h"
#include "JDay.h"
#include "Labelizer.h"
#include "LatLon.h"
#include "MemMatrix.h"
#include "Projection.h"
#include "RegTools.h"
#include "SubMatrix.h"
#include "Tools.h"
#include "TronHints.h"
#include "Vector.h"
#include "VectorMatrix.h"

#include "newbase/NFmiEnumConverter.h"

#ifdef METQU
#include "TrackedDataSet.h"
#endif

#ifdef USE_TESTRAW
#include "TestRaw.h"
#endif

#include "Proto.h"

#include "Q3Engine.h"

// Entry point of the baked-in 'newcairo' C module (defined in NewCairo.cpp).
// Registered into 'package.preload' so that a script's `require "newcairo"` is
// resolved in-process instead of via the dlsym()-based C loader; see the
// registration in Session::init() for the rationale.
extern "C" int luaopen_newcairo(lua_State *L);

#define UNIT_UNKNOWN NA_Param::UNIT_UNKNOWN

#ifdef CONFIG_BINARY_OUTPUT_ENABLED
static const char *const MIME_BINARY_MATRIX = "binary/octet-stream";
#endif

using namespace std;

// Name of the object's metatable field, used for output formatting.
//
static const char *METAFIELD_TYPE = "__type";

// Use a special value to help debug; this should NOT be NAN.
//
// Filled in places where the code should rewrite the blocks; this value should
// never get through to the user.
//
#ifndef NDEBUG
#define UNINITIALIZED_VALUE (123.456e6)
#endif

/*---=== Helpers ===---*/

/*
 * [jday_ud]= parse_jday( jday_ud|time_str|time_num|any )
 *
 * Returns the time as JDay, or nothing for bad string syntax (or bad types).
 */
static int q3_parse_jday(lua_State *L) {
  // Don't use 'proto()'; assume any input

  JDay t(L, 1);
  if (!t)
    return 0;

  new (L) JDay(t);
  return 1;
}

/*---=== ... ===---*/

static const string NO_DATA_WITH_MSG(
    "No data with given dataType, producerId and/or originTime.");

static unsigned char type_chunk[] =
#include "type.lch"

    static unsigned char assert_chunk[] =
#include "assert.lch"

        static unsigned char prepare_chunk[] =
#include "prepare.lch"

            static unsigned char q3_chunk[] =
#include "q3.lch"

                static unsigned char util_chunk[] =
#include "utilities.lch"

                    static unsigned char gauss_chunk[] =
#include "gauss.lch"

                        static unsigned char cross_chunk[] =
#include "cross.lch"

#ifdef USE_TRON
                            static unsigned char contour_chunk[] =
#include "contour.lch"
#endif

                                static unsigned char json_chunk[] =
#include "json.lch"

                                    // Registry keys to fetch prepared Lua side
                                    // functions
                                    //
    static /*const*/ void *REG_JSON_FUNC =
        (void *)&REG_JSON_FUNC; // address of itself (unique)

static unsigned char latlon_chunk[] =
#include "latlon.lch"

    // Registry keys to fetch prepared Lua side functions
    //
    /*const*/ void *REG_LATLON_FUNC =
        (void *)&REG_LATLON_FUNC; // address of itself (unique)

/*---=== ... ===---*/

/*
 * matrix_ud | vectormatrix_ud = new_ApiMatrix( size_matrixpos_ud, [num|vector],
 * [param_name_str], [projection_str] )
 *
 * Creates a new 'MemMatrix' or 'VectorMatrix' object.
 *
 * 'param_name' and value (if vector) decide whether the returned matrix is
 * matrix of scalars ('MemMatrix') or of vectors ('VectorMatrix'), and if vector
 * whether it's stored as polar or cartesian.
 *
 * Initial value 'nan' guarantees initialization to NANs. With no initial value,
 * the matrices may not be initialized (optimization in case the script will
 * write over them).
 *
 * NOTE: All matrices produced are getting 'UNIT_UNKNOWN' which makes them
 * unsuitable to be used in interpolations.
 */
static int new_ApiMatrix(lua_State *L) {

  proto(L, "MatrixPos, [number|Vector|true], [string], [string]");

  const MatrixPos &gs = *MatrixPos::instance(L, 1);

  const Vector *v2 = Vector::instance(L, 2);
  const char *param_name = lua_tostring(L, 3);

  bool param_is_2d = false;
  bool param_is_polar = false;

  if (param_name) {
    param_is_2d = ApiParam(param_name).is_2d(param_is_polar);
  }

  const char *proj = lua_tostring(L, 4);
  bool dont_init = lua_isnil(L, 2);

  const static NA_Param::Unit a_unit(NA_Param::UNIT_UNKNOWN_);

  if (param_is_2d || v2) {
    // Push matrix of vectors
    //
    L_GROW(2);

    // If 'param_name' was given, it defines, whether the matrix is polar.
    // If no param name was given, we'll do as 'v2' has it.
    //
    bool polar = param_is_2d ? param_is_polar : v2->isPolar();

    NA_Param::Unit b_unit(polar ? NA_Param::UNIT_DEG : a_unit);

    if (dont_init) {
#ifdef NDEBUG
      new (L) MemMatrix(gs, a_unit, proj); // not initialized (the script will)
      new (L) MemMatrix(gs, b_unit, proj);
#else
      new (L) MemMatrix(gs, UNINITIALIZED_VALUE, a_unit, proj);
      new (L) MemMatrix(gs, UNINITIALIZED_VALUE, b_unit, proj);
#endif
    } else if (polar) {
      new (L) MemMatrix(gs, v2 ? v2->getAbs() : NAN, a_unit, proj);
      new (L) MemMatrix(gs, v2 ? v2->getDeg() : NAN, b_unit, proj);
    } else {
      new (L) MemMatrix(gs, v2 ? v2->getX() : NAN, a_unit, proj);
      new (L) MemMatrix(gs, v2 ? v2->getY() : NAN, b_unit, proj);
    }

    new (L) VectorMatrix(
        L, polar); // eat components and take 'param' and 'proj' from them
    return 1;

  } else {
    // Push matrix of scalars
    //
    if (dont_init) {
#ifdef NDEBUG
      new (L) MemMatrix(gs, a_unit, proj);
#else
      new (L) MemMatrix(gs, UNINITIALIZED_VALUE, a_unit, proj);
#endif
    } else {
      float v = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : NAN;
      new (L) MemMatrix(gs, v, a_unit, proj);
    }
    return 1;
  }
}

/*
 * matrix_ud = new_ScalarMatrix( matrix_ud )
 * matrix_ud = new_ScalarMatrix( matrixpos_ud, [Matrix|num|nan],
 * [projection_str], [setgrid] )
 *
 * Note: If 'num' is missing, the matrix is really NOT NEEDED TO BE INITIALIZED.
 *       The script (not the application script) will fill in so initializing to
 * NAN would be a waste.
 */
static int new_ScalarMatrix(lua_State *L) {
  Matrix *m = Matrix::instance(L, 1);
  if (m) {
    proto(L, "Matrix"); // make sure no other params
  } else {
    proto(L, "MatrixPos,[Matrix|number],[string],[bool]");
  }

  if (m) {
    new (L) MemMatrix(*m); // copies values, param and projection
    return 1;

  } else {
    const MatrixPos &gs = *MatrixPos::instance(L, 1);
    const char *proj = lua_tostring(L, 3);

    Matrix *m = Matrix::instance(L, 2);
    if (m) {
      // Take level, parameter and unit from given matrix
      new (L) MemMatrix(gs, m->getLevel(), m->getParam(), m->getUnit(),
                        proj); // uninitialized

      if (lua_isboolean(L, 4) && lua_toboolean(L, 4)) {
        // Take grid from given matrix. This is to enable "chaining" of
        // PEEKXY's. Should there be any problems, disable this !
        //
        Matrix *m2 = Matrix::instance(L, -1);

        if (m2) {
          lua_pushstring(L, "grid");
          lua_gettable(L, 2);
          unsigned key = LuaNew_base::keep_alive(L, -1, 1);
          m2->setGridKey(key);
          m2->setGrid(m->getGrid());
          lua_pop(L, 1);
        }
      }
    } else {
      if (!lua_isnumber(L, 2)) {
        new (L) MemMatrix(gs, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE,
                          proj); // uninitialized
      } else {
        double v = lua_tonumber(L, 2);
        new (L) MemMatrix(gs, v, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, proj);
      }
    }
    return 1;
  }
}

/*
 * vectormatrix_ud = new_VectorMatrix( m_ud, m_ud )
 * vectormatrix_ud = new_VectorMatrix( size_pos_ud )
 *
 * Creates a 2D matrix based on the given components, or matrix size.
 *
 * The first upvalue decides if the matrix is made as cartesian ('false', used
 * as 'new_VectorMatrix_xy') or polar ('true', used as
 * 'new_VectorMatrix_polar').
 *
 * If submatrices are provided, their values are copied, so the returned matrix
 * will be independent of the params given.
 */
static int new_VectorMatrix(lua_State *L) {
  bool polar = lua_toboolean(L, lua_upvalueindex(1));

  Matrix *mx = Matrix::instance(L, 1);
  if (mx) {
    proto(L, "Matrix,Matrix");

    Matrix *my = Matrix::instance(L, 2);

    L_GROW(2);
    new (L) MemMatrix(*mx); // make a copy
    new (L) MemMatrix(*my);
  } else {
    proto(L, "MatrixPos");
    MatrixPos &gs = *MatrixPos::instance(L, 1);

#ifdef NDEBUG
    new (L) MemMatrix(gs, NA_Param::UNIT_UNKNOWN_,
                      nullptr); // not initialized (the script will)
    new (L) MemMatrix(gs, NA_Param::UNIT_UNKNOWN_, nullptr);
#else
    new (L)
        MemMatrix(gs, UNINITIALIZED_VALUE, NA_Param::UNIT_UNKNOWN_, nullptr);
    new (L)
        MemMatrix(gs, UNINITIALIZED_VALUE, NA_Param::UNIT_UNKNOWN_, nullptr);
#endif
  }

  new (L) VectorMatrix(L, polar); // eat components
  return 1;
}

/*
 * matrix_ud|vectormatrix_ud = nan_matrix( size_pos_ud [, number|vector] )
 *
 * Creates a NAN-initialized matrix for storing values like the second
 * parameter.
 *
 * Note: The sample value is NOT set anywhere in the matrix. It is just used to
 * define, whether the returned matrix is storing scalars, cartesian (xy)
 * vectors or polar vectors.
 */
static int nan_matrix(lua_State *L) {
  proto(L, "MatrixPos,[number|Vector]");

  const MatrixPos &size = *MatrixPos::instance(L, 1);

  if (lua_type(L, 2) == LUA_TNIL || lua_type(L, 2) == LUA_TNUMBER) {
    new (L) MemMatrix(size, NAN, NA_Param::UNIT_UNKNOWN_, nullptr);
    return 1;

  } else {
    const Vector &v = *Vector::instance(L, 2);

    new (L) MemMatrix(size, NAN, NA_Param::UNIT_UNKNOWN_, nullptr);
    new (L) MemMatrix(size, NAN, NA_Param::UNIT_UNKNOWN_, nullptr);
    new (L) VectorMatrix(L, v.isPolar()); // eat components
    return 1;
  }
}

/*
 * sub_matrix_ud = new_SubMatrix( matrix_ud, window_size_ud )
 *
 * Creates a "peekhole" matrix to see part of 'm'.
 */
static int new_SubMatrix(lua_State *L) {
  proto(L, "Matrix,MatrixPos");

  Matrix &m = *Matrix::instance(L, 1);
  MatrixPos &a = *MatrixPos::instance(L, 2);

  new (L) SubMatrix(m, a);
  LuaNew_base::keep_alive(L, -1, 1); // keep 'm' alive throughout our lifespan

  return 1;
}

/*
 * vector_ud = new_Vector_xy( x_num, y_num )
 *
 * Creates a new two-value vector (for placing in a matrix).
 */
static int new_Vector_xy(lua_State *L) {

  double x = lua_tonumber(L, 1);
  double y = lua_tonumber(L, 2);

  new (L) Vector(x, y, false); // cartesian
  return 1;
}

/*
 * vector_ud = new_Vector_polar( abs_num, deg_num )
 *
 * Creates a new two-value vector (for placing in a matrix).
 *
 * Note: Values are adjusted to [0,inf) and [0,360) range, by 'Vector'
 * constructor.
 */
static int new_Vector_polar(lua_State *L) {

  double abs = lua_tonumber(L, 1);
  double deg = lua_tonumber(L, 2);

  new (L) Vector(abs, deg, true);
  return 1;
}

/*
 * m2= LONLAT( projection_str, gridsize_pos )
 */
static int LONLAT(lua_State *L) {
  proto(L, "string,MatrixPos");

  const char *proj = lua_tostring(L, 1);
  const MatrixPos &gs = *MatrixPos::instance(L, 2);

  L_GROW(3);

  MemMatrix &m_lat = *new (L) MemMatrix(gs, NA_Param::UNIT_LAT, proj);
  MemMatrix &m_lon = *new (L) MemMatrix(gs, NA_Param::UNIT_LON, proj);

  Projection pr(proj);

  double x_top = gs.getX() - 1;
  double y_top = gs.getY() - 1;

  for (MatrixIter mi(gs); mi.within(); ++mi) {
    LatLon ll = pr.latlon(mi.getX() / x_top, mi.getY() / y_top);
    m_lat[mi] = ll.getLat();
    m_lon[mi] = ll.getLon();
  }

  // Eat up the two components to build a 'VectorMatrix'
  //
  new (L) VectorMatrix(L, false /*not polar*/);
  return 1;
}

/*
 * latlon_ud= latlon( projection_str, gs_pos_ud, pos_ud )
 * latlon_ud= latlon( lat_num, lon_num )
 * latlon_ud= latlon( str )          -- i.e. "60.2N 50.7E"
 * latlon_ud= latlon( latlon_ud )    -- just make a copy
 *
 * Note: 'require "fminames"' extends 'latlon' to take in city and geographic
 * names (i.e. 'latlon("Helsinki")').
 */
static int latlon(lua_State *L) {

  unsigned n = lua_gettop(L);
  if (n == 3) {
    proto(L, "string,MatrixPos,MatrixPos");

    const char *proj_s = lua_tostring(L, 1);
    const MatrixPos &gs = *MatrixPos::instance(L, 2);
    const MatrixPos &pos = *MatrixPos::instance(L, 3);

    double dx = ((double)pos.getX()) / (gs.getX() - 1);
    double dy = ((double)pos.getY()) / (gs.getY() - 1);

    Projection proj(proj_s);

    new (L) LatLon(proj.latlon(dx, dy));
    return 1;

  } else if (n == 2) {
    proto(L, "number,number");
    double lat = lua_tonumber(L, 1);
    double lon = lua_tonumber(L, 2);

    try {
      new (L) LatLon(lat, lon);
    } catch (const E_USAGE &e) {
      LuaNew_base::nuke(L,
                        -1); // removes the link from Lua GC to C++ destructor
      luaL_error(L, "%s", e.what());
    }
    return 1;

  } else {
    LatLon *ll = new (L) LatLon();
    if (!ll->init_from_ud(L, 1)) {
      ll->parse(L, 1, true /*errors if bad string*/);
    }
    return 1;
  }
}

/*
 * km_num= distance_km( latlon_ud, latlon_ud )
 *
 * Return the shortest distance over the Earth geoid between two locations.
 */
static int distance_km(lua_State *L) {
  proto(L, "latlon,latlon");

  // Note: Use 'init_from' instead of 'LatLon::instance()' to allow conversion
  // from
  //       strings automatically to 'LatLon' (also 'proto.latlon' uses that).
  //
  LatLon a, b;
  bool ok_a = a.init_from_ud(L, 1);
  bool ok_b = b.init_from_ud(L, 2);

  assert(ok_a && ok_b); // must be, otherwise 'proto' would have kicked in
  (void)ok_a;
  (void)ok_b;

  double dist = a.spherical_great_circle_distance_km(b);
  lua_pushnumber(L, dist);
  return 1;
}

/*
 * id = getparamid( paramname )
 *
 * Return parameter id
 */
static int getparamid(lua_State *L) {
  proto(L, "string");

  NFmiEnumConverter converter;
  const char *paramname = lua_tostring(L, 1);
  int id = paramname ? converter.ToEnum(paramname) : kFmiBadParameter;

  if (id == kFmiBadParameter)
    lua_pushnil(L);
  else
    lua_pushnumber(L, id);

  return 1;
}

/*
 * setdecimals( decimals )
 *
 * Set data output precision
 */
static int setdecimals(lua_State *L) {
  proto(L, "number");
  int decimals = lua_tonumber(L, 1);
  RegTools::set_Decimals(L, decimals);

  return 0;
}

/*
 * peekxy( Matrix m, Matrix m2, MatrixPos gp, MatrixPos offset )
 * peekxy( Matrix m, Matrix m2, MatrixPos gp, xdistance, ydistance )
 *
 * Offseting for Q2 smarttools PEEKXY/PEEKXY3.
 * Return location (latlon_ud) relative to given matrix/gridpoint gp of m2
 *
 * Offset given as number of matrix/grid points or as distance in km
 * along latitude/longitude lines is applied to m (finer/native matrix/grid)
 */
static int peekxy(lua_State *L) {
  unsigned n = lua_gettop(L);
  if (n == 4)
    proto(L, "Matrix,Matrix,MatrixPos,MatrixPos");
  else
    proto(L, "Matrix,Matrix,MatrixPos,number,number");

  const Matrix &m = *Matrix::instance(L, 1);
  const Matrix &m2 = *Matrix::instance(L, 2);
  const MatrixPos &pos = *MatrixPos::instance(L, 3);

  if (n == 4) {
    const MatrixPos &offset = *MatrixPos::instance(L, 4);
    return m2.offsetPosition(L, m, pos, offset);
  }

  double xoffsetkm = lua_tonumber(L, 4);
  double yoffsetkm = lua_tonumber(L, 5);

  return m2.offsetPosition(L, m, pos, xoffsetkm, yoffsetkm);
}

/*
 * Push a table to 'L' with bindings to Q3 C++ side functions.
 */
void q3_bind(lua_State *L) {
  L_GROW(3);

  lua_newtable(L);

  // Set up classes
  //
  Raw::create_mt(L);
  Grid::create_mt(L);
  Matrix::create_mt(L);
  VectorMatrix::create_mt(L);
  Vector::create_mt(L);
  MatrixSize::create_mt(L);
#ifdef METQU
  TrackedDataSet::create_mt(L);
#endif
  JDay::create_mt(L);
  LatLon::create_mt(L);

  // Derivation ambiguities leads us using such syntax (this is still
  // essentially the Same Thing as above):
  //
  LuaNew<MatrixPosBind>::create_mt(L);
  LuaNew<MatrixIterBind>::create_mt(L);
  LuaNew<SubMatrixBind>::create_mt(L);

  // 05-Jun-2012 PKi: For using tron hints
  LuaNew<TronHintsBind>::create_mt(L);

  // matrix:
  //
  lua_pushcfunction(L, new_ApiMatrix);
  lua_setfield(L, -2, "new_ApiMatrix");

  lua_pushcfunction(L, new_ScalarMatrix);
  lua_setfield(L, -2, "new_ScalarMatrix");

  lua_pushcfunction(L, MatrixPos::new_MatrixPos);
  lua_setfield(L, -2, "new_MatrixPos");

  lua_pushcfunction(L, MatrixIter::q3_points_iterator);
  lua_setfield(L, -2, "_points_iterator");

  lua_pushcfunction(L, LatLonList::_areamask);
  lua_setfield(L, -2, "_areamask");

  lua_pushcfunction(L, SubMatrix::set_pos);
  lua_setfield(L, -2, "_subm_set_pos");

  lua_pushcfunction(L, SubMatrix::set_radius);
  lua_setfield(L, -2, "_subm_set_radius");

  lua_pushcfunction(L, new_SubMatrix);
  lua_setfield(L, -2, "new_SubMatrix");

  lua_pushcfunction(L, new_Vector_xy);
  lua_setfield(L, -2, "new_Vector_xy");

  lua_pushcfunction(L, new_Vector_polar);
  lua_setfield(L, -2, "new_Vector_polar");

  lua_pushcfunction(L, new_VectorMatrix); // no upvalue = false
  lua_setfield(L, -2, "new_VectorMatrix_xy");

  lua_pushboolean(L, true);
  lua_pushcclosure(L, new_VectorMatrix, 1 /*upvalues*/);
  lua_setfield(L, -2, "new_VectorMatrix_polar");

  lua_pushcfunction(L, nan_matrix);
  lua_setfield(L, -2, "_nan_matrix");

  // Mathematical formulas
  //
  lua_pushcfunction(L, Matrix::q3_abs);
  lua_setfield(L, -2, "abs");

  lua_pushcfunction(L, Matrix::q3_ceil);
  lua_setfield(L, -2, "ceil");

  lua_pushcfunction(L, Matrix::q3_cos);
  lua_setfield(L, -2, "cos");

  lua_pushcfunction(L, Matrix::q3_floor);
  lua_setfield(L, -2, "floor");

  lua_pushcfunction(L, Matrix::q3_fmod);
  lua_setfield(L, -2, "fmod");

  lua_pushcfunction(L, Matrix::q3_log);
  lua_setfield(L, -2, "log");

  lua_pushcfunction(L, Matrix::q3_log10);
  lua_setfield(L, -2, "log10");

  lua_pushcfunction(L, Matrix::q3_max);
  lua_setfield(L, -2, "_max");

  lua_pushcfunction(L, Matrix::q3_min);
  lua_setfield(L, -2, "_min");

  lua_pushcfunction(L, Matrix::q3_sum_or_avg);
  lua_setfield(L, -2, "_sum");

  // Note: If we define 'avg' as 'return sum(...)/count(...)' in 'prepare.lua'
  //      we don't need this C++ side support for it (but it would be slower,
  //      causing two rounds through the data).
  //
  lua_pushboolean(L, 1); // true: avg_mode
  lua_pushcclosure(L, Matrix::q3_sum_or_avg, 1 /*upvalues*/);
  lua_setfield(L, -2, "_avg");

  lua_pushcfunction(L, Matrix::q3_count);
  lua_setfield(L, -2, "_count");

  lua_pushcfunction(L, Matrix::q3_set);
  lua_setfield(L, -2, "_set");

  lua_pushcfunction(L, Matrix::q3_modf);
  lua_setfield(L, -2, "modf");

  lua_pushcfunction(L, Matrix::q3_sin);
  lua_setfield(L, -2, "sin");

  lua_pushcfunction(L, Matrix::q3_tan);
  lua_setfield(L, -2, "tan");

  // Raw:
  //
#ifdef METQU
  lua_pushcfunction(L, RawBind::new_Raw_ro);
  lua_setfield(L, -2, "new_Raw_ro");

  lua_pushcfunction(L, RawBind::new_Raw_rw);
  lua_setfield(L, -2, "new_Raw_rw");
#endif

  // Contouring support:
  //
#ifdef USE_TRON
  Contour::create_mt(L);
  EdgePoint::create_mt(L);

  proto_init(L)
      .set(Contour::name(), Contour::is)
      .set(EdgePoint::name(), EdgePoint::is);

  lua_pushcfunction(L, Contour::contour);
  lua_setglobal(L, "contour");

  lua_pushcfunction(L, Contour::contour_smoothen_one);
  lua_setfield(L, -2, "contour_smoothen_one");

  lua_pushcfunction(L, Contour::calc_slants);
  lua_setfield(L, -2, "calc_slants");

  // 21-Nov-2011 PKi: Draw, fill and labelize contours
  lua_pushcfunction(L, Labelizer::pushLabelizerCfg);
  lua_setglobal(L, "LabelizerConfig");
  lua_pushcfunction(L, Contour::drawcontours);
  lua_setglobal(L, "drawcontours");
#endif

  // misc:
  //
  lua_pushcfunction(L, q3_parse_jday);
  lua_setfield(L, -2, "_parse_jday");

  lua_pushcfunction(L, LONLAT);
  lua_setfield(L, -2, "LONLAT");

  lua_pushcfunction(L, latlon);
  lua_setfield(L, -2, "latlon");

  lua_pushcfunction(L, distance_km);
  lua_setfield(L, -2, "distance_km");

  lua_pushcfunction(L, getparamid);
  lua_setglobal(L, "getparamid");

  lua_pushcfunction(L, setdecimals);
  lua_setglobal(L, "setdecimals");

  lua_pushcfunction(L, peekxy);
  lua_setglobal(L, "peekxy");

  lua_pushcfunction(L, Q3Engine::getAddonConfigSetting);
  lua_setglobal(L, "getaddonsetting");

  // Test pattern generator:
  //
#ifdef USE_TESTRAW
  new (L) TestRaw();
  lua_setfield(L, -2, "testraw");
#endif

  // Variant of LOG that reports one level upper than usual (for DUMP)
  //
  lua_pushboolean(L, true);
  lua_pushcclosure(L, Logger::LOG_, 1 /*upvalues*/);
  lua_setfield(L, -2, "LOG_ONE_UP");
}

/*---=== Session ===---
 */

/*
 * Help debug initialization errors when running on Panik. These SHOULD be
 * out of memory issues only (which should not really happen).
 */
static void INIT_ERROR(lua_State *L, int st, const char *file, unsigned line) {

  LOG_FATAL("Lua error %d in %s:%d: %s", st, file, line, lua_tostring(L, -1));
  //
  // 3: syntax error
  // 4: out of memory

  lua_pushnil(
      L); // trying to use as little memory as we can (in case of out-of-memory)
  lua_error(L); // never returns
}

/*
 * void= init( [package_path_str], [package_cpath_str] )
 *
 * Initialize a Lua state, with proper sandboxing and bindings for Q3.
 *
 * NOTE: This function must be called via 'lua_[p]call'; it may give errors
 *       from the scripts (i.e. if global s.a. 'validtime' has wrong kind
 *       of syntax).
 */
int Session::init(lua_State *L) {
  // Note: 'proto' not initialized, yet. Don't use.
  int st;
  assert(L);

#ifndef METQU
  const char *package_path = lua_tostring(L, 1);
  const char *package_cpath = lua_tostring(L, 2);
#endif

  const int old_tos = lua_gettop(L);

  // Declare 'LOG()' as a global function (for debugging); _before_ 'proto'.
  //
  lua_pushcfunction(L, Logger::LOG_);
  lua_setglobal(L, "LOG");

  // Pre-register baked-in C module openers in 'package.preload'.
  //
  // Without this, a script's `require "newcairo"` finds nothing in
  // 'package.loaded' (the state is rebuilt per request) and falls through to
  // Lua's C loader, which resolves the module with dlsym(). dlsym() takes
  // glibc's recursive dynamic-loader lock (_dl_load_lock) on every request. If
  // a Lua error is thrown and unwinds out through that dlsym() frame (e.g. the
  // kill-time hook, or any script/data error), glibc's unlock never runs and
  // the recursive loader lock is abandoned owner-held-but-idle, deadlocking
  // every other thread that touches the dynamic linker.
  //
  // Registering the opener in 'package.preload' makes `require` resolve it via
  // the in-process preload loader (no dlsym, no loader lock), while staying
  // lazy: the opener still runs only when the module is actually required.
  //
  // Done before prepare.lua replaces the global 'package' with a read-only
  // proxy; 'require' uses the real package table regardless of that proxy.
  {
    lua_getglobal(L, "package"); // [package]
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "preload"); // [package][preload]
      if (lua_istable(L, -1)) {
        lua_pushcfunction(L, luaopen_newcairo);
        lua_setfield(L, -2, "newcairo"); // package.preload.newcairo = opener
      }
      lua_pop(L, 1); // preload
    }
    lua_pop(L, 1); // package
  }

  // Must also stamp this state with a unique id (keeps the various log messages
  // together in distributed logs)
  //
  {
#ifndef METQU
    Logger::unique_t id = Logger::gen_unique(); // 64-bit value

    lua_pushlightuserdata(L, (void *)Logger::gen_unique);
    lua_pushnumber(L, (double)id); // 'double' has 56-bit accuracy (Lua integers
                                   // normally 32, even in 64-bit systems)
    lua_settable(L, LUA_REGISTRYINDEX);
#endif
  }

  // Initiate the 'proto' system and add our specific classes to it.
  //
  proto_init(L)
      .set(Raw::name(), Raw::is)
      .set(Grid::name(), Grid::is)
      .set(Matrix::name(), Matrix::is)
      .set(VectorMatrix::name(), VectorMatrix::is)
      .set(Vector::name(), Vector::is)
      .set(MatrixSize::name(), MatrixSize::is)
#ifdef METQU
      .set(TrackedDataSet::name(), TrackedDataSet::is)
#endif
      .set(LuaNew<MatrixPosBind>::name(), LuaNew<MatrixPosBind>::is)
      .set(LuaNew<MatrixIterBind>::name(), LuaNew<MatrixIterBind>::is)
      .set(LuaNew<SubMatrixBind>::name(), LuaNew<SubMatrixBind>::is)
      .set(
          "jday",
          LuaNew<JDayBind>::is) // making the constraint lower case (not "JDay")
      .set("latlon", LatLon::is)
      // 05-Jun-2012 PKi: For using tron hints
      .set(LuaNew<TronHintsBind>::name(), LuaNew<TronHintsBind>::is);

  //---
  // Run 'type.lua' (baked-in)
  //
  {
    st = luaL_loadbuffer(L, (char *)type_chunk, sizeof(type_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      // Can only be LUA_ERRMEM (the script is precompiled so no syntax errors)
      //
      INIT_ERROR(L, st, __FILE__, __LINE__); // does not return
    }

    lua_call(L, 0 /*args*/, 0 /*results*/); // errors will be thrown up
  }

  //---
  // Run 'assert.lua' (baked-in)
  //
  {
    st = luaL_loadbuffer(L, (char *)assert_chunk, sizeof(assert_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    lua_call(L, 0 /*args*/, 0 /*results*/);
  }

  //---
  // Run 'json.lua' (baked-in) and keep a reference to the returned function
  //
  {
    st = luaL_loadbuffer(L, (char *)json_chunk, sizeof(json_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    lua_call(L, 0 /*args*/, 1 /*results*/);

    // [-1]: function returned by 'json.lua', to be used for filtering query
    // return values We'll store it in 'reg[REG_JSON_FUNC]', which is invisible
    // for the Lua side.
    //
    L_ASSERT(lua_isfunction(L, -1));

    // Also place the function in 'json' global, to be used by 'metqu' launching
    // script.
    //
#ifdef METQU
    lua_pushliteral(L, "json");
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_GLOBALSINDEX);
#endif

    lua_pushlightuserdata(L, (void *)REG_JSON_FUNC);
    lua_insert(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }

  //---
  // Run 'latlon.lua' (baked-in) and keep a reference to the returned function.
  //
  {
    st = luaL_loadbuffer(L, (char *)latlon_chunk, sizeof(latlon_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    lua_call(L, 0 /*args*/, 1 /*results*/);

    // [-1]: function returned by 'latlon.lua', to be used for parsin
    //       complex latlon index strings (could also be done with C++ regexps,
    //       if there were any decent ones).
    //
    L_ASSERT(lua_isfunction(L, -1));

    lua_pushlightuserdata(L, (void *)REG_LATLON_FUNC);
    lua_insert(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }

  L_ASSERT(lua_gettop(L) == old_tos);

  //---
  // Push a table with C side bindings
  //
  q3_bind(L);

  // [-1]: binding table

  L_ASSERT(lua_istable(L, -1));

  const int bind_index = lua_gettop(L);

  //---
  // Run 'prepare.lua' (baked-in)
  //
  {
    st = luaL_loadbuffer(L, (char *)prepare_chunk, sizeof(prepare_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    lua_pushvalue(L, bind_index);

#ifdef METQU
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    unsigned args = 2;

#ifndef METQU
    lua_pushstring(L, package_path);
    lua_pushstring(L, package_cpath);
    args += 2;
#endif

    lua_call(L, args, 0 /*results*/);
  }

  //---
  // Run 'q3.lua' (baked-in)
  //
  {
    st = luaL_loadbuffer(L, (char *)q3_chunk, sizeof(q3_chunk),
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    // Call 'q3.lua' with bindings table, current UTC time, METQU and
    // SMARTTOOL_NAMES as parameters
    //
    lua_pushvalue(L, bind_index);

    // UTC date as JDay
    //
    struct timeval tv;
    // {
    //   time_t       tv_sec;   /* seconds since Jan. 1, 1970 */
    //   suseconds_t  tv_usec;  /* and microseconds */
    // };

    int rc =
        gettimeofday(&tv, nullptr /*time zone not used any more (in Linux)*/);
    assert(rc == 0);
    (void)rc;

    struct tm tmp;
    gmtime_r(&tv.tv_sec, &tmp); // Split it out into year, month etc.
                                //
                                // tm_year  /*0..*/ +1900
                                // tm_mon   /*0..11*/ +1
                                // tm_mday  /*0..31*/
                                // tm_hour  /*0..23*/
                                // tm_min   /*0..59*/
                                // tm_sec   /*0..61*/

    new (L) JDay(tmp.tm_year + 1900, tmp.tm_mon + 1, tmp.tm_mday, tmp.tm_hour,
                 tmp.tm_min, tmp.tm_sec);

#ifdef METQU
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif

#ifdef SMARTTOOL_NAMES
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif

    lua_call(L, 4 /*args*/, 0 /*results*/);
  }

  //---=== Run baked-in utility chunks ===---

  struct {
    const unsigned char *chunk;
    size_t bytes;
  } chunks[] = {
      {util_chunk, sizeof(util_chunk)},
      {gauss_chunk, sizeof(gauss_chunk)},
      {cross_chunk, sizeof(cross_chunk)},
#ifdef USE_TRON
      {contour_chunk, sizeof(contour_chunk)},
#endif
      // ... add more chunks here

      {nullptr, 0} // end mark
  };

  for (unsigned i = 0; chunks[i].chunk; i++) {
    st = luaL_loadbuffer(L, (char *)chunks[i].chunk, chunks[i].bytes,
                         nullptr /*from precompiled*/);
    if (st) {
      INIT_ERROR(L, st, __FILE__, __LINE__);
    }

    lua_pushvalue(L, bind_index);
    lua_call(L, 1 /*args*/, 0 /*results*/);
  }

  lua_settop(L, old_tos); // clear the stack

  return 0;
}

/*
 * Output script return value N (if any).
 *
 * Returns: MIME type of the entry (nullptr for just text)
 *
 * Note: The caller is supposed to have 'lua_pcall()' somewhere above us, to
 * catch possible errors from user-provided metamethods (or built-in
 * 'json.lua').
 */
string_or_null Session::result_(lua_State *L, ostream &os, unsigned i) {
  assert((i >= 1) && (i <= (unsigned)lua_gettop(L)));

  bool jsonp_mode = RegTools::get_JSONP(L);

  L_GROW(2);

  // Check for types we know how to stream
  //
  if (lua_getmetatable(L, i)) {

    // Is it a Cairo surface (PNG/SVG/PDF)?
    //
    lua_getfield(L, -1, METAFIELD_TYPE);
    const char *s = lua_tostring(L, -1);

    if (s && (strcmp(s, "Cairo surface") == 0)) {
      lua_pop(L, 2);

      if (jsonp_mode) {
        luaL_error(L, "Trying to output an image using JSONP. No can do.");
      }

      // [i]: cairo surface
      //
      // data_str, mime_type_str= cs.contents()

      lua_pushliteral(L, "contents");
      lua_gettable(L, i);
      if (lua_type(L, -1) != LUA_TFUNCTION) {
        const char *msg = "Problem with Cairo binding: '<cairo "
                          "surface>.contents' is not a function";
        LOG_BUG0(msg);
        luaL_error(L, msg);
      }
      lua_call(L, 0 /*args*/, 2 /*retvals*/);

      // [-1]: mime type (string); i.e. "image/png"
      // [-2]: content (as binary string)

      string_or_null mime = lua_tostring(L, -1);

      size_t len;
      const char *data = lua_tolstring(L, -2, &len);
      os.write(data, len);

      lua_pop(L, 2);
      return mime;
    }
    lua_pop(L, 1);

    // [-1]: metatable
    // ...
    // [i]: the object to output

    /*
     * If it's a matrix and binary mode is enabled, do that.
     */
#if (!defined METQU) && (defined CONFIG_BINARY_OUTPUT_ENABLED)
    if (RegTools::get_Binary(L)) {
      Matrix *m = Matrix::instance(L, i); // nullptr if not a (scalar) matrix
      if (m) {
        int decs = RegTools::get_Decimals(L);

        m->asBinary_q2_(os, decs);
        return MIME_BINARY_MATRIX;
      }
    }
#endif

    // [-1]: metatable

    // Does it have metatable '__tostring()' function?
    //
    lua_getfield(L, -1, "__tostring");
    //
    // [-1]: nil or function
    // [-2]: metatable

    if (lua_isfunction(L, -1)) {
      lua_pushvalue(L, i); // 2nd ref of returned object

      lua_call(L, 1 /*args*/, 1 /*results*/);
      //
      // [-1]: string

      const char *tmp = lua_tostring(L, -1);
      if (tmp) {
        os << tmp;
      }
      lua_pop(L, 2);
      return nullptr; // just text
    }
    lua_pop(L, 2); // remove nil and metatable
  }

  size_t len;
  const char *s;

  // Most return values are serialized as JSON (json.lua for tables/strings,
  // native rendering for scalars), so report 'application/json'. The special
  // cases below (Lua-mode 'nil', unsupported types) override this to text.
  string_or_null result_mime = MIME_JSON;

  switch (lua_type(L, i)) {
  case LUA_TNUMBER:
    // Converts [i] into a string in-place (we're fine with that)
    //
    s = lua_tolstring(L, i, &len); // JSON and Lua
    break;

  case LUA_TNIL:
    s = jsonp_mode ? "null" /*JSON*/ : "nil" /*Lua*/;
    len = strlen(s);
    if (!jsonp_mode) {
      result_mime = nullptr; // 'nil' is not valid JSON; treat as plain text
    }
    break;

  case LUA_TBOOLEAN:
    s = lua_toboolean(L, i) ? "true" : "false"; // JSON (and Lua)
    len = strlen(s);
    break;

  case LUA_TSTRING:
  case LUA_TUSERDATA:
  case LUA_TTABLE: {
    lua_pushlightuserdata(L, REG_JSON_FUNC); // key
    lua_gettable(L, LUA_REGISTRYINDEX);
    //
    // [-1]: function (from 'json.lua')

    L_ASSERT(lua_isfunction(L, -1));
    lua_pushvalue(L, i);

    // There should not be errors in the JSON conversion. If there are, upper
    // levels will get them (we use 'lua_call()' for performance here, too).
    //
    lua_call(L, 1 /*params*/, 1 /*retvals*/);
    //
    // [-1]: JSON format string

    s = lua_tolstring(L, -1, &len);
    L_ASSERT(s);
  } break;

  default:             // function etc.
    s = L_typename(i); // give something (calling 'tostring()' would give more)
    len = strlen(s);
    result_mime = nullptr; // bare type name, not JSON

    if (jsonp_mode) {
      luaL_error(L, "Trying to output '%s' using JSONP. Cannot do that.", s);
    }
    break;
  }

  os.write(s, len);

  return result_mime;
}
