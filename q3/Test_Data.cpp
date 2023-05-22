/*
 * Test_Data.CPP                          Copyright (c) 2010, Ilmatieteen laitos
 */
#include "Config.h"

#ifdef USE_TESTRAW
#include "Test_Data.h"

#include "MemMatrix.h"
#include "SQD_Projection.h"

using namespace std;

static const NA_Info *my_info = nullptr;

/*
 * Init 'my_info'
 */
static struct JustOnce_Test_Data { // note: must have unique name (otherwise
                                   // runtime problems, linker mixes the two
                                   // structs)
  JustOnce_Test_Data() {

    vector<JDay> times; // TODAY+0 .. TODAY+24
    string nativeLevelType;
    vector<NA_Level> levels; // ground, height 0..inf
    vector<NA_Param> params;

    time_t tt;
    JDay jd(time(&tt));
    jd.set(jd.year(), jd.month(), jd.day(), 0, 0, 0);

    for (unsigned i = 0; i <= 24; i += 3) {
      times.push_back(jd.add_hours(i));
    }

    levels.push_back(NA_Level(NA_Level::GROUND_LEVEL));

    params.push_back(NA_Param("SW", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE));
    params.push_back(NA_Param("SE", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE));
    params.push_back(NA_Param("S", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE));

    // Note: Cannot use 'Projection::NONE' here. Order of initialization of C++
    // globals is not defined; 'Projection::NONE'
    //       constructor may not have been run before us.
    //
    Projection pr_empty; // empty projection

    my_info =
        new NA_Info(0, 0, jd, times, nativeLevelType, levels, params, pr_empty);
  }
} just_for_init;

/*
 */
Test_Data::Test_Data()
    : NA_Data(*my_info) {

  INVARIANT();
}

typedef bool (*t_param_func)(double, double);

/*
 */
static bool f_SW(double dx, double dy) {
  return (dx + dy < 1.0); // lower left corner
}

/*
 */
static bool f_SE(double dx, double dy) {
  return ((1.0 - dx) + dy < 1.0); // lower right corner
}

/*
 */
static bool f_S(double dx, double dy) {
  (void)dx;

  return (dy < 0.5); // lower half
}

/*
 */
static bool f_FULL(double dx, double dy) {
  (void)dx;
  (void)dy;
  return true;
}

/*
 */
static bool f_NONE(double dx, double dy) {
  (void)dx;
  (void)dy;
  return false;
}

/*
 */
static bool f_BALL(double dx, double dy) {
  return pow(dx - 0.5, 2) + pow(dy - 0.5, 2) < pow(0.5, 2);
}

/*
 */
const Matrix *Test_Data::push_TestMatrix(lua_State *L, const JDay &vt,
                                         const NA_Level &lev, const NA_Param &p,
                                         const MatrixPos &gs) const throw() {
  (void)lev; // not used

  // LOG_DEBUG( "Lev: %d %lf", (int)lev.getType(), lev.getValue() );

  // Make factors for multiplication (faster than division; like speed would
  // matter for test features, eh?)
  //
  double x_scale = 1.0 / (gs.getX() - 1);
  double y_scale = 1.0 / (gs.getY() - 1);

  float v = 1.0;

  // Param decides the pattern of test output.
  //
  string ps = p.toString(true /*prefer standard name (no ':nnn') */);

  t_param_func param_func = (ps == "SW")     ? f_SW
                            : (ps == "SE")   ? f_SE
                            : (ps == "S")    ? f_S
                            : (ps == "FULL") ? f_FULL
                            : (ps == "NONE") ? f_NONE
                            : (ps == "BALL") ? f_BALL
                                             : nullptr;

  if (!param_func) {
    return nullptr;
  }

  // TBD: Käytä aikaa ('vt') testimatriisien kasvattamiseen reunoilta (TODAY+0)
  // toiseen reunaan (TODAY+24)

  MemMatrix *m = new (L)
      MemMatrix(gs, NA_Param::UNIT_UNKNOWN_INTERPOLATABLE, "" /*projection*/);

  // How we fill the matrix depends on time, level and parameter.
  //
  for (MatrixIter it(gs); it.within(); ++it) {
    double dx = it.getX() * x_scale;
    double dy = it.getY() * y_scale;

    // (dx,dy) is (0,0)..(1,1)

    (*m)[it] = param_func(dx, dy) ? v : NAN;
  }

  return m;
}

#endif
// USE_TESTRAW
