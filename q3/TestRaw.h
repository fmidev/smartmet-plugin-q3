/*
 * TESTRAW.H                         Copyright (c) 2010, Ilmatieteen laitos
 */
#include "Config.h"

#if (!defined(TESTRAW_H)) && defined(USE_TESTRAW)
#define TESTRAW_H

#include "Raw.h"
#include "Test_Data.h"

/*
 * Object that behaves similar to 'Raw', providing static test patterns.
 */
class TestRaw : public Raw_interface {
public:
  TestRaw() = default;
  /*virtual*/ ~TestRaw() override {}

  // Get data pointer; valid as long as this object is.
  //
  /*virtual*/ const NA_Data *getData() const override { return &test; }

  /*virtual*/ string_or_null getSource() const override { return nullptr; }

  /*virtual*/ const NA_Level &getDefaultLevel() const override {
    return def_lev;
  }
  /*virtual*/ const MatrixPos &getDefaultGridsize() const override {
    return def_gs;
  }

  // data members
private:
  Test_Data test;
  static const NA_Level def_lev;
  static const MatrixPos def_gs;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {}
#endif
};

#endif
// TESTRAW_H (and that it's enabled in Config.h)
