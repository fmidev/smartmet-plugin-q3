/*
 * JDAY.CPP                          Copyright (c) 2010, Ilmatieteen laitos
 *
 * Storage of 'YYYYMMDDHHMMSS' UTC times, allowing easy input/output and
 * calculations.
 *
 * Ref: http://en.wikipedia.org/wiki/Julian_day
 *      http://www.xmission.com/~tknarr/code/Date.html
 *
 * Revised:  25-Oct-2010
 */
#include "JDay.h"
#include "Proto.h"

#include "Tools.h"

#include <cassert>

using namespace std;

LuaNew_ID JDayBind::ID;

JDay::jd_T JDay::INVALID_VALUE = LLONG_MAX;

/*---=== Helpers ===---*/

/*---=== JDayBind ===---*/

/*
 * [bool|int]= __index( jday_ud, key_any )
 */
int JDayBind::index(lua_State *L) {
  const JDay &my = *JDay::instance(L, 1);

  if (!my) {
    return 0; // undefined, return nothing
  }

  const char *s = lua_tostring(L, 2); // key
  if (s) {
    // The fields are compatible with Lua 'os.date()' and 'os.time()' so an
    // object of this kind should be usable with them.
    //
    if (strcmp(s, "year") == 0) {
      lua_pushinteger(L, my.year()); // i.e. 2010
      return 1;
    }
    if (strcmp(s, "month") == 0) {
      lua_pushinteger(L, my.month()); // 1..12
      return 1;
    }
    if (strcmp(s, "day") == 0) {
      lua_pushinteger(L, my.day()); // 1..31
      return 1;
    }
    if (strcmp(s, "hour") == 0) {
      lua_pushinteger(L, my.hour()); // 0..23
      return 1;
    }
    if (strcmp(s, "min") == 0) {
      lua_pushinteger(L, my.min()); // 0..59
      return 1;
    }
    if (strcmp(s, "sec") == 0) {
      lua_pushinteger(L, my.sec()); // 0..60 (never jump seconds)
      return 1;
    }
    if (strcmp(s, "wday") == 0) {
      lua_pushinteger(L, my.wday()); // 1 (Sunday) .. 7 (Saturday)
      return 1;
    }
    if (strcmp(s, "yday") == 0) {
      lua_pushinteger(L, my.yday()); // day of the year (1..366)
      return 1;
    }
    if (strcmp(s, "isdst") == 0) {
      lua_pushboolean(L,
                      false); // always UTC (for compatibility with 'os.date()')
      return 1;
    }

    // Additional fields (not 'os.date()' specific)
    //
    if (strcmp(s, "jday") == 0) {
      lua_pushnumber(L, my.jd_noon()); // traditional JDAY (based on noon UTC)
      return 1;
    }

    // Provide Unix epoch seconds i.e. for use with 'os.date()' formatting
    //
    if (strcmp(s, "epoch") == 0) {
      unsigned year = my.year();
      if (year < 1970) {
        luaL_error(L, "Cannot provide epoch values for < 1970");
      }

      struct tm tmp;
      //
      memset(&tmp, 0, sizeof(tmp));
      tmp.tm_year = my.year() - 1900;
      tmp.tm_mon = my.month() - 1;
      tmp.tm_mday = my.day();
      tmp.tm_hour = my.hour();
      tmp.tm_min = my.min();
      tmp.tm_sec = my.sec();

      time_t t = timegm(&tmp);

      // Note: 'lua_Integer' would not have enough integer resolution in 32-bit
      // mode.
      //       'lua_Number' (double) always has.
      //
      lua_pushnumber(L, (double)t);
      return 1;
    }
  }

  luaL_error(L, "Bad index (for JDay): %s", L_string_or_typename(2));
  return 0; // never
}

/*
 * void= __newindex( jday_ud, key_any, val_any )
 *
 * Setting individual minutes, hours etc.
 *
 * This is (at least) used for setting seconds and hours to 0 for NOW and TODAY.
 * Users may find it useful as a generic mechanism as well.
 */
int JDayBind::newindex(lua_State *L) {
  proto(L, "*,string,uint");

  JDay &my = *JDay::instance(L, 1);

  const char *s = lua_tostring(L, 2); // key
  assert(s);

  if (!my) {
    luaL_error(L, "Trying to set '%s' of an undefined JDay object", s);
  }

  unsigned v = lua_tointeger(L, 3);

  // The fields are compatible with Lua 'os.date()' and 'os.time()' (and of
  // course our own '::index()').
  //
  if (strcmp(s, "year") == 0) {
    my.set(v, my.month(), my.day(), my.hour(), my.min(), my.sec());
  } else if (strcmp(s, "month") == 0) {
    my.set(my.year(), v, my.day(), my.hour(), my.min(), my.sec());
  } else if (strcmp(s, "day") == 0) {
    my.set(my.year(), my.month(), v, my.hour(), my.min(), my.sec());
  } else if (strcmp(s, "hour") == 0) {
    my.set(my.year(), my.month(), my.day(), v, my.min(), my.sec());
  } else if (strcmp(s, "min") == 0) {
    my.set(my.year(), my.month(), my.day(), my.hour(), v, my.sec());
  } else if (strcmp(s, "sec") == 0) {
    my.set(my.year(), my.month(), my.day(), my.hour(), my.min(), v);
  } else {
    luaL_error(L, "Bad index (for JDay): %s", lua_tostring(L, 2));
  }

  return 0; // nothing to return
}

/*
 * jday_ud= __add( jday, hour_num )
 *        = __add( any, jday )       <-- can get in with this (give an error)
 *
 * Note: We expect JDay to be first param, but also (any,JDay) would lead here.
 *       Be careful.
 *
 * Note: Selection of hour as the unit of addition is intentional, and fixed.
 *       It's mainly there to have 'NOW[+-]nn' and 'TODAY[+-]nn' work in hours.
 */
int JDayBind::add(lua_State *L) {
  const JDay *a = JDay::instance(L, 1);

  if (a) {
    if (lua_isnumber(L, 2)) {
      new (L) JDay(a->add_hours(lua_tonumber(L, 2)));
      return 1;
    }
  }

  luaL_error(L, "Cannot add %s and %s", L_typename(1), L_typename(2));
  return 0; // never
}

/*
 * jday_ud= __sub( jday, hour_num )
 * hour_num= __sub( jday, jday )
 *       = __sub( any, jday )    <-- can get in with this (give an error)
 *
 * Note: Either (or both) of the two parameters is a 'JDay'
 *
 * Note: See '::add()' for discussion on why the integer unit is hours.
 */
int JDayBind::sub(lua_State *L) {
  const JDay *a = JDay::instance(L, 1);
  const JDay *b = JDay::instance(L, 2);

  if (a) {
    if (b) {
      lua_pushnumber(L, (a->jd_plus_half - b->jd_plus_half) / 3600.0);
      return 1;

    } else if (lua_isnumber(L, 2)) {
      new (L) JDay(a->sub_hours(lua_tointeger(L, 2)));
      return 1;
    }
  }

  luaL_error(L, "Cannot subtract %s and %s", L_typename(1), L_typename(2));
  return 0; // never
}

/*
 * bool= __eq( jday, jday )
 *
 * Note: Lua metamethod handling guarantees the types are both 'jday'.
 */
int JDayBind::eq(lua_State *L) {
  const JDay &a = *JDay::instance(L, 1);
  const JDay &b = *JDay::instance(L, 2);

  lua_pushboolean(L, a && b && (a == b));
  return 1;
}

/*
 * bool= __lt( jday, jday )
 *
 * Note: Lua metamethod handling guarantees the types are both 'jday'.
 */
int JDayBind::lt(lua_State *L) {
  const JDay &a = *JDay::instance(L, 1);
  const JDay &b = *JDay::instance(L, 2);

  lua_pushboolean(L, a && b && (a < b));
  return 1;
}

/*
 * [str]= __tostring( jday )
 */
int JDayBind::tostring(lua_State *L) {
  const JDay &a = *JDay::instance(L, 1);

  if (a) {
    lua_pushstring(L, a.toString().c_str());
    return 1;
  } else {
    return 0; // undefined time
  }
}

/*
 * Set up a metatable.
 */
void JDayBind::setup(lua_State *L) {

  assert(lua_istable(L, -1));

  // Metamethods
  //
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushliteral(L, "__add");
  lua_pushcfunction(L, add);
  lua_settable(L, -3);

  lua_pushliteral(L, "__sub");
  lua_pushcfunction(L, sub);
  lua_settable(L, -3);

  lua_pushliteral(L, "__eq");
  lua_pushcfunction(L, eq);
  lua_settable(L, -3);

  lua_pushliteral(L, "__lt");
  lua_pushcfunction(L, lt);
  lua_settable(L, -3);

  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, tostring);
  lua_settable(L, -3);
}

/*---=== JDay ===---*/

/*
 */
JDay::jd_T JDay::calc(time_t ticks) {
  struct tm tmp;
  gmtime_r(&ticks, &tmp);

  return calc(1900 + tmp.tm_year, 1 + tmp.tm_mon, tmp.tm_mday, tmp.tm_hour,
              tmp.tm_min, tmp.tm_sec);
}

/*
 * Construction from "YYYYMMDD[HH[MM[SS]]]"
 *
 * Returns INVALID if the syntax was not expected (test with 'operator bool()').
 */
JDay::jd_T JDay::fromstring(const char *s) {
  assert(s);

  int yyyy, mm, dd, hh = 0, min = 0, sec = 0, dummy;

  // One extra field catches numeric strings that are too long
  //
  int rc = sscanf(s, "%4d%2d%2d%2d%2d%2d%d", &yyyy, &mm, &dd, &hh, &min, &sec,
                  &dummy);
  //
  // ...
  // 5: YYYYMMDDHHMM
  // 6: YYYYMMDDHHMMSS
  // 7: extra characters

  if ((rc < 3) || (rc > 6)) {
    return INVALID_VALUE; // bad syntax
  }

  return calc(yyyy, mm, dd, hh, min, sec);
}

/*
 * Construct from an entry in Lua stack.
 *
 *  "<YYYYMMDD[HH[MM[SS]]]>"    given time, in UTC (string)
 *  YYYYMMDD[HH[MM[SS]]]        given time, in UTC (number)
 *  JDay                        userdata entry (make copy)
 *
 * Note: This function is being called for both 'validtime' and 'origintime'
 *      once per a query, to transform their initial values to 'JDay' for ease
 * of calculations. However, this is being _also_ called for any subsequent read
 *      of validtime/origintime parameters. This is so that changes to the
 * values can be made using the same format as in URL.
 *
 * Creates a non-valid JDay (test with 'operator bool()') if the entry is not
 * appropriate.
 */
JDay::JDay(lua_State *L, int index) : jd_plus_half(INVALID_VALUE) {
  const JDay *a = JDay::instance(L, index);
  if (a) {
    *this = *a;
    return;
  }

  // YYYYMMDD[HH[MM[SS]]] string or number (gets converted to string in-place)
  //
  const char *s = lua_tostring(L, index);
  if (s) {
    jd_plus_half = fromstring(s); // INVALID_VALUE if bad syntax
  }
}

/*
 * Note: It is essential the parameters are 'int' (not 'unsigned').
 */
unsigned JDay::calc_jdn(int yyyy, int mm, int dd) {

  // Julian Day number (integer) based on noon (calculation relies on integer
  // division)
  //
  return (1461 * (yyyy + 4800 + (mm - 14) / 12)) / 4 +
         (367 * (mm - 2 - 12 * ((mm - 14) / 12))) / 12 -
         (3 * ((yyyy + 4900 + (mm - 14) / 12) / 100)) / 4 + dd - 32075;
}

unsigned JDay::year() const {
  unsigned x;
  gregorian(&x, nullptr, nullptr);
  return x;
}

unsigned JDay::month() const {
  unsigned x;
  gregorian(nullptr, &x, nullptr);
  return x;
}

unsigned JDay::day() const {
  unsigned x;
  gregorian(nullptr, nullptr, &x);
  return x;
}

unsigned JDay::hour() const { return (jd_plus_half / 3600) % 24; }

unsigned JDay::min() const { return (jd_plus_half / 60) % 60; }

unsigned JDay::sec() const { return jd_plus_half % 60; }

unsigned JDay::wday() const {
  // JDN mod 7: Mon (0) ... Sun (6)
  //
  // But JDN's start at noon, so Monday before noon is still JD Sunday. :)
  //
  // To give our kind of weekdays, we simply DO NOT do the subtraction of half
  // (43200).
  //
  return (((jd_plus_half / 86400) + 1) % 7) +
         1; // 1..7 (Sun..Sat); akin to Lua 'os.date()' :)
}

unsigned JDay::yday() const {

  unsigned yyyy;
  gregorian(&yyyy, nullptr, nullptr);

  // Calculate day of year by reducing Jan 1st
  //
  // Note: Likewise in 'wday()' we DO NOT subtract the hour/minute/sec
  // correction; this
  //      compensates for our days starting at midnight and JD's at noon.
  //
  return (jd_plus_half / 86400) - calc_jdn(yyyy, 1, 1) +
         1; // 1..366 (365 on regular years)
}

/*
 * Note: This only gets years >= AD 1 (but could be changed/tested for earlier
 * years, if needed).
 *
 * Months are given as 1..12 (Jan..Dec).
 * Days are given as 1..28|29|30|31.
 */
void JDay::gregorian(unsigned *year_ptr, unsigned *month_ptr,
                     unsigned *day_ptr) const {
  int z = (int)(jd_plus_half / 86400);
  int A;
  if (z < 2299161) {
    A = z;
  } else {
    int alpha = (int)((z - 1867216.25) / 36524.25);
    A = z + 1 + alpha - (alpha / 4); // integer division
  }

  int B = A + 1524;
  int C = (int)((B - 122.1) / 365.25);
  int D = (int)(365.25 * C);
  int E = (int)((B - D) / 30.6001);

  if (day_ptr) {
    *day_ptr = B - D - (int)floor(30.6001 * E);
  }

  if (month_ptr || year_ptr) {
    unsigned month_ = (E < 14) ? E - 1 : E - 13;
    if (month_ptr)
      *month_ptr = month_;
    if (year_ptr)
      *year_ptr = (month_ > 2) ? C - 4716 : C - 4715;
  }
}

/*
 */
string JDay::toString() const {
  unsigned yyyy, mm, dd;
  gregorian(&yyyy, &mm, &dd);

  char buf[20]; // enough for 'YYYYMMDDHHMMSS'
  sprintf(buf, "%04d%02d%02d%02d%02d%02d", yyyy, mm, dd, hour(), min(), sec());
  return string(buf);
}

/*
 * Is the time covered by a collection of times?
 *
 * Note: 'vec' is expected to be in rising order.
 *
 * If covered (returns 'true'), 'exact' is set to tell whether the time was
 * precisely there, or interpolated.
 */
bool JDay::covered_by(const vector<JDay> &times, bool &exact) const {

  if (!*this)
    return false; // time is unknown = not covered

  if ((*this < times.front()) || (*this > times.back())) {
    return false;

  } else {
    int n = find_index<JDay>(times, *this);
    exact = (n >= 0);
    return true;
  }
}

/*
 * Selftest for exercising the conversions.
 */
#ifndef NDEBUG
void JDay::selftest() {

  //---
  JDay a(2010, 3, 29, 11, 22, 33);
  string as = a.toString();

  // cerr << "!!" << a.wday() << " " << a.yday() << " " <<
  // string_fmt("%.6f",a._getJD_debug()) << endl;
  assert(as == "20100329112233");
  assert(fabs(a.jd_noon() - 2455284.973993) < 1e-6);
  assert(a.year() == 2010);
  assert(a.month() == 3);
  assert(a.day() == 29);
  assert(a.wday() == 2);  // Monday
  assert(a.yday() == 88); // day of year

  //---
  JDay aa(2010, 3, 29, 20, 44, 55);
  string aas = aa.toString();

  // cerr << "!!" << aa.wday() << " " << aa.yday() << " " <<
  // string_fmt("%.6f",aa._getJD_debug()) << endl;
  assert(aas == "20100329204455");
  assert(a.year() == 2010);
  assert(a.month() == 3);
  assert(a.day() == 29);
  assert(a.wday() == 2);  // Monday
  assert(a.yday() == 88); // day of year
}
#endif
