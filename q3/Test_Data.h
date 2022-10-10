/*
 * TEST_DATA.H                            Copyright (c) 2010, Ilmatieteen laitos
 */
#ifndef TEST_DATA_H
#define TEST_DATA_H

#include "Config.h"

#ifndef USE_TESTRAW
#error "This file shouldn't be included."
#endif
#ifdef METQU
#error "Not for writeable data matrices."
#endif

#include "MatrixPos.h"
#include "NA_Data.h"

/*
 * Provides static test data
 */
class Test_Data : public NA_Data {
public:
  Test_Data() throw(E_BAD_FILE);
  /*virtual*/ ~Test_Data() {}

  /*virtual*/ const Matrix *
  push_NativeMatrix(lua_State *L, const JDay &vt, const NA_Level &lev,
                    const NA_Param &p
                    //
                    // 22-Aug-2011 PKi: For fetching data using newbase
                    // interpolation, projection and scaling
                    //
                    ,
                    const Projection *target_proj_orig = nullptr,
                    const MatrixPos *target_gs_orig = nullptr,
                    bool *target_ready = nullptr,
                    const DataIdList *dataIds = nullptr) const throw() {
    return push_TestMatrix(L, vt, lev, p, MatrixPos(50, 50));
  }

  /*virtual*/ const Matrix *
  push_Matrix(lua_State *L, const JDay &vt, const NA_Level &lev,
              const NA_Param &p, const Projection &proj_,
              const MatrixPos &target_gs,
              const DataIdList *dataIds = nullptr) const throw() {
    (void)proj_; // not used
    return push_TestMatrix(L, vt, lev, p, target_gs);
  }

  /*virtual*/ bool providesPressureLevelsFromHybrid() const { return false; }

private:
  const Matrix *push_TestMatrix(lua_State *L, const JDay &vt,
                                const NA_Level &lev, const NA_Param &p,
                                const MatrixPos &gs) const throw();

  // data fields:

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {}
#endif
};

#endif
// TEST_DATA_H
