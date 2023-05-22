/*
 * RAW.H                         Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * The full data of a certain Query Data file.
 *
 * Revised:  15-Oct-2010
 */
#ifndef RAW_H
#define RAW_H

#include "LuaNew.h"
#include "MatrixPos.h"
#include "NA_Data.h"
#include "TrackedData.h"

#include "JDay.h"

#ifdef METQU
#include "ApiParam.h"
#endif

class MemMatrix;
class TrackedDataSet;
class TrackedData;

class Raw_interface;

struct RawBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "Raw"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef Raw_interface CAST_T;

#ifdef METQU
  static int new_Raw_ro(lua_State *L);
  static int new_Raw_rw(lua_State *L);
  static int new_Raw(lua_State *L, const TrackedDataSet &files, const JDay &ot,
                     const std::vector<JDay> &required_times,
                     const std::vector<NA_Level> &required_levels,
                     const std::vector<ApiParam> &required_params,
                     bool height_true,
                     const MatrixPos &default_gs);
#endif

private:
  static int __index(lua_State *L);
  static int __call(lua_State *L);

  static int has_param(lua_State *L);
#ifdef METQU
  static int write(lua_State *L);
#endif
};

/*
 * 'Raw_interface' required if a 'TestRaw' pattern generator class is used.
 * Otherwise 'Raw' would be the only derivative from us.
 */
class Raw_interface : public LuaNew<RawBind> {
public:
  virtual ~Raw_interface(){};

#ifdef METQU
  virtual NA_Data *getData_rw() = 0;
#endif
  virtual const NA_Data *getData() const = 0;

  virtual string_or_null getSource() const = 0;

  virtual const NA_Level &getDefaultLevel() const = 0;
  virtual const MatrixPos &getDefaultGridsize() const = 0;

private:
  // nothing (abstract base class)
};

/*
 * Covers the data of a whole data source (SQD/MQD file).
 */
class Raw : public Raw_interface {
public:
  Raw(TrackedData *td_, const NA_Level &def_level_,
      const MatrixPos &def_gridsize_ = MatrixPos::ZERO,
      bool metaQuery = false) throw(); // from disk (read-only; server)

#ifdef METQU
  Raw(const NA_Info &info) throw(); // from scratch (read-write)
  bool /*not cancelled*/ fill_from(lua_State *L, const Raw_interface &r_from,
                                   NA_Data::ProgressCallback *cb);
#endif

  /*virtual*/ ~Raw();

  // Get data pointer; valid as long as this object is.
  //
#ifdef METQU
  /*virtual*/ NA_Data *getData_rw() { return qd; }
#endif
  /*virtual*/ const NA_Data *getData() const { return qd; }

  /*virtual*/ string_or_null getSource() const {
    return td ? td->getSource() : 0;
  }

  /*virtual*/ const NA_Level &getDefaultLevel() const { return def_level; }
  /*virtual*/ const MatrixPos &getDefaultGridsize() const {
    return def_gridsize;
  }

  // data members
private:
  TrackedData *const td;
  CONST_IF_SERVER NA_Data *const qd; // received from 'td->Acquire()' or
                                     // explicitly given (and to be deleted)
  const NA_Level
      def_level; // level requirement used in opening the raw (default for
                 // grids) NA_Level::NO_LEVEL if no level requirement used

  const MatrixPos def_gridsize; // Default gridsize for grids made out of this
                                // Raw object 'MatrixPos::ZERO' for no default
                                // 'MatrixPos::DX' for use native gridsize as
                                // default (ignore 'gridsize' global)

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    // 'td' is 0 for temporarily crafted (not tracked) QueryData
    assert_invariant(qd);
  }
#endif
};

#endif
// RAW_H
