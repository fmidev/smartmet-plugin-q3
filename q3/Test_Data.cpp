/*
 * Test_Data.CPP                          Copyright (c) 2010, Ilmatieteen laitos
 */
#include "Config.h"

#ifdef USE_TESTRAW
#include "Test_Data.h"

#include "MemMatrix.h"
#include "SQD_Projection.h"

using namespace std;

namespace
{
const NA_Info *my_info = nullptr;

/*
 * Init 'my_info'
 */
struct JustOnce_Test_Data { // note: must have unique name (otherwise
                                   // runtime problems, linker mixes the two
                                   // structs)
  JustOnce_Test_Data() {

    vector<JDay> times; // TODAY+0 .. TODAY+24
    string nativeLevelType;
    vector<NA_Level> levels; // ground, height 0..inf
    vector<NA_Param> params;

    time_t tt = 0;
    JDay jd(time(&tt));
    jd.set(jd.year(), jd.month(), jd.day(), 0, 0, 0);

    for (unsigned i = 0; i <= 24; i += 3) {
      times.push_back(jd.add_hours(i));
    }

    levels.emplace_back(NA_Level::GROUND_LEVEL);

    params.emplace_back("SW", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE);
    params.emplace_back("SE", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE);
    params.emplace_back("S", NA_Param::UNIT_UNKNOWN_INTERPOLATABLE);

    // Note: Cannot use 'Projection::NONE' here. Order of initialization of C++
    // globals is not defined; 'Projection::NONE'
    //       constructor may not have been run before us.
    //
    Projection pr_empty; // empty projection

    my_info =
        new NA_Info(0, 0, jd, times, nativeLevelType, levels, params, pr_empty);
  }
} just_for_init;
}  // namespace

/*
 */
Test_Data::Test_Data()
    : NA_Data(*my_info) {

  INVARIANT();
}

using t_param_func = bool (*)(double, double);

namespace
{
/*
 */
bool f_SW(double dx, double dy) {
  return (dx + dy < 1.0); // lower left corner
}

/*
 */
bool f_SE(double dx, double dy) {
  return ((1.0 - dx) + dy < 1.0); // lower right corner
}

/*
 */
bool f_S(double dx, double dy) {
  (void)dx;

  return (dy < 0.5); // lower half
}

/*
 */
bool f_FULL(double dx, double dy) {
  (void)dx;
  (void)dy;
  return true;
}

/*
 */
bool f_NONE(double dx, double dy) {
  (void)dx;
  (void)dy;
  return false;
}

/*
 */
bool f_BALL(double dx, double dy) {
  return pow(dx - 0.5, 2) + pow(dy - 0.5, 2) < pow(0.5, 2);
}
}  // namespace

/*
 */
const Matrix *Test_Data::push_TestMatrix(lua_State *L, const JDay & /*vt*/,
                                         const NA_Level &lev, const NA_Param &p,
                                         const MatrixPos &gs) const noexcept {
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

  static const std::pair<const char *, t_param_func> patterns[] = {
      {"SW", f_SW},     {"SE", f_SE},     {"S", f_S},
      {"FULL", f_FULL}, {"NONE", f_NONE}, {"BALL", f_BALL}};

  t_param_func param_func = nullptr;
  for (const auto &pattern : patterns) {
    if (ps == pattern.first) {
      param_func = pattern.second;
      break;
    }
  }

  if (!param_func) {
    return nullptr;
  }

  // TBD: Käytä aikaa ('vt') testimatriisien kasvattamiseen reunoilta (TODAY+0)
  // toiseen reunaan (TODAY+24)

  auto *m = new (L)
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
