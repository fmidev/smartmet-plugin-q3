/*
 * NA_PARAM.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Revised:  2-Dec-10 AKa
 */
#ifndef NA_PARAM_H
#define NA_PARAM_H

#include "Tools.h"

#ifdef USE_NEWBASE
#include "newbase/NFmiParameterName.h"
#endif

#include <string>
#include <vector>

// 31-Oct-2011 PKi: Parameter mapping for virtual parameters
//
typedef struct virtualParameterName {
  virtualParameterName(FmiParameterName p, FmiParameterName s) {
    primaryId = p;
    secondaryId = s;
  };

  FmiParameterName primaryId;   // primary parameter's native id in SQD file
  FmiParameterName secondaryId; // secondary parameter's native id in SQD file
} virtualParameterName;

/*
 * Differentiates parameter name and id, plus handles parameter unit (and
 * through it, interpolation mode).
 *
 * "Native name" means the data about a parameter as presented in the data
 * source (i.e. "Lämpötila:4" for temperature in SQD files)
 *
 * "Standard name" means the standard name for a parameter. Excludes the id
 * number
 *       (":nnn") and for well-known parameters is a standard string (i.e. "T").
 *
 *   Native                          Standard
 *   ------                          --------
 *   "Lämpötila:4"                   "T"
 *   "Paine:1"                       "P"
 *   ...
 *   ""                              "T"         parameter created by the script
 * (not stored)
 *
 * Note: The native id's are SPECIFIC TO EACH DATA ADAPTER. Some adapters may
 * use them, some not. For SQD files, i.e. 4 means temperature but for some
 * other data storage it may mean something else. This is why we need standard
 * names in the first place.
 *       --AKa 2-Dec-2010
 */
class NA_Param {
public:
  enum e_Datatype {
    DATATYPE_FLOAT,    // single precision range
    DATATYPE_UINT16,   // range 0..65534 (65535 is missing value)
    DATATYPE_BYTE,     // range 0..254 (255 is missing value)
    DATATYPE_HALFBYTE, // range 0..14 (15 is missing value)
    DATATYPE_BOOL      // range 0..1 (>= 2 is missing value)
  };

  enum e_Interpolation {
    INTERPOLATE_UNKNOWN = 0, // interpolation method unknown
    INTERPOLATE_NEAREST, // take nearest value (i.e. for discrete enum values,
                         // 'N','FOG', ...)
    INTERPOLATE_LINEAR,  // interpolate continuously between the values
    INTERPOLATE_LINEAR_DEG, // interpolate within 0..359.999..0 continuum
    INTERPOLATE_LINEAR_LON, // interpolate within -179.99..180.0 continuum
  };

  class Unit {
  public:
    Unit(enum e_Datatype dt_, enum e_Interpolation method_,
         const char *unit_ = nullptr)
        : dt(dt_), method(method_), unit() {
      LOG_DEBUG("Initializing with method %d", (int)method_);
      if (!unit_) {
        *unit = '\0';
      } else {
        strncpy(unit, unit_, sizeof(unit) - 1);
        unit[sizeof(unit) - 1] = '\0';
      }
    }

    Unit() : dt(DATATYPE_FLOAT), method(INTERPOLATE_UNKNOWN), unit() {
      unit[0] = '\0';
    }

    string_or_null getUnitName() const { return *unit ? unit : nullptr; }
    enum e_Interpolation getMethod() const { return method; }
    enum e_Datatype getDatatype() const { return dt; }

    bool operator==(const Unit &o) const {
      return (dt == o.dt) && (method == o.method) &&
             (strcmp(unit, o.unit) == 0);
    }
    bool operator!=(const Unit &o) const { return !(*this == o); }

  private:
    // 'NA_Param' assignment operator means we cannot have these 'const'
    //
    /*const*/ enum e_Datatype dt;
    /*const*/ enum e_Interpolation method;

    // Note: had segfaults with 'string_or_null' here (when initializing
    // pre-main static
    //       variables).
    //
    /*const*/ char unit[16];
  };

  // Units with 'nearest point' method (no interpolation):
  //
  static const Unit UNIT_FLOAT_NEAREST; // unknown or no unit
                                        // no interpolation (value from nearest
                                        // point) data type: float

  static const Unit
      UNIT_UINT16_NEAREST; // same but range 0..65534 (65535 = missing value)

  static const Unit UNIT_BOOL; // 0|1 (>= 2 is missing value)
                               // no interpolation (value from nearest point)
                               // data type: byte (or two bits)

  static const Unit
      UNIT_MAX254_ENUM; // anything enumerated (with max. range 0..254; 255 =
                        // missing value) no interpolation (value from nearest
                        // point) data type: byte

  static const Unit
      UNIT_MAX14_ENUM; // same but range 0..14 (15 = missing value)

  // Units with rotary interpolation:
  //
  static const Unit UNIT_DEG; // degrees (0..359.999....; for 'WD' 0..360.0)
                              // rotary linear interpolation
                              // data type: float

  static const Unit UNIT_LON; // -179.999 .. 180.000

  // Units with normal (linear) interpolation:
  //
  // Note: There's multiple other "normal" units in this category as well (s.a.
  // "m", "km/h")
  //      as defined by 'SQD_Tools.cpp' (for SQD data) or MQD data files.
  //
  // interpolation:   linear
  // data type:       float
  //
  static const Unit UNIT_PRC;    // 1% (0..100)
  static const Unit UNIT_10PRC;  // 10% (0..10); not necessarily integer
  static const Unit UNIT_100PRC; // 100% (0..1.0)
  static const Unit UNIT_1;      // known to be "just numbers" (no unit)

  static const Unit
      UNIT_LAT; // -90.0 .. 90.0 (units in degrees but no wrap-around)

  static const Unit UNIT_UNKNOWN_; // no unit, no interpolation
  static const Unit
      UNIT_UNKNOWN_INTERPOLATABLE; // result of mathematical operation, unit
                                   // unknown (linear interpolation)

  NA_Param() : std_name(""), native_name(""), unit(UNIT_UNKNOWN_) {
    INVARIANT();
  }

  // 25-Oct-2011 PKi: Now needed by server too
  //#ifdef METQU
  NA_Param(const char *);
  //#endif

#ifdef USE_NEWBASE
  NA_Param(FmiParameterName e, const std::string &interpolation_name_ = "",
           const std::string &precision_ = "");
  NA_Param(FmiParameterName e, const std::string &native_name_,
           const Unit &unit_, const std::string &interpolation_name_ = "",
           const std::string &precision_ = "");

  static FmiParameterName standard_param_native_id(const char *s);

  // 31-Oct-2011 PKi: Parameter mapping for virtual parameters
  //
  static virtualParameterName virtual_param_native_id(const char *s);
  static std::string native_id_virtual_param(FmiParameterName e);
#endif

  // 20-Oct-2011 PKi: Now needed by server too (used to create virtual
  // parameters)
  //#ifdef USE_TESTRAW
  NA_Param(const char *std_name, const Unit &unit_, bool nonStd = false);
  //#endif

  std::string toString(bool prefer_standard_names) const;

  const std::string &getStandardName() const { return std_name; }

  const std::string &getNativeName_() const { return native_name; }

  const Unit &getUnit() const { return unit; }
  bool hasUnit() const { return unit.getUnitName().c_str() != nullptr; }

  e_Interpolation getMethod() const { return unit.getMethod(); }
  const std::string &getInterpolationName() const { return interpolation_name; }
  const std::string &getPrecision() const { return precision; }

  operator bool() const { return !((std_name == "") && (native_name == "")); }

private:
  // data members
  //
  /*const*/ std::string std_name;    // i.e. "T" (for 'xxx:4' in SQD)
  /*const*/ std::string native_name; // native name (i.e. "Lämpötila:4")

  /*const*/ Unit unit;
  /*const*/ std::string interpolation_name;
  /*const*/ std::string precision;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {

    // 'std_name' and 'native_name' relations:
    //
    // Neither:             empty
    // Both:                i.e. "T" with "Lämpötila:4" as native (SQD) name
    // Just standard name:  i.e. "WS" or "T" when not coming from SQD native
    //                          (Note: With 'USE_NEWBASE' defined, all params
    //                          must have native name, at least ':NNN')
    // Just native name:    i.e. "MyCustom:12345" as a custom parameter (note:
    // only SQD requires the id trailing)

#ifdef USE_NEWBASE
#if 0
        if (native_name != "") {
            LOG_DEBUG( "Invariant: %s", native_name.c_str() );
        }
#endif

    if (std_name != "") {
      assert_invariant(native_name != "");
    }
    if (native_name != "") {
      const char *colon = strchr(native_name.c_str(), ':');
      assert_invariant(colon && (atoi(colon + 1) >
                                 0)); // always has an id part with integer tail
    }
#else
    (void)file;
    (void)line;
#endif
  }
#endif
};

#endif
// NA_PARAM_H
