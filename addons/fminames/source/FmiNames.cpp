/*
* LUA51-FMINAMES.CPP                       Copyright 2010, Ilmatieteen laitos
*
* Binding to 'FmiNames' C++ library for accessing FMI MySQL name database.
*
* TBD: Maybe we should use the 'geofind' command directly, and forget about Fminames.
*      Is there an added benefit from it?   --AKa 25-Feb-10
*/

// Locus
#include <Query.h>
#include <QueryOptions.h>
#include <SimpleLocation.h>

#include "Proto.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

using namespace std;

/*
* Lua chunk precompiled into us
*/
static unsigned char fminames_chunk[] =
#include "fminames.lch"

    /*---=== Helpers ===---
    */

    /*
    */
    static string string_fmt(const char *fmt, ...)
{
  char buf[4000];

  va_list vl;
  va_start(vl, fmt);
  {
    vsnprintf(buf, sizeof(buf), fmt, vl);
  }
  va_end(vl);

  // Some sources say 'snprintf()' would not zero terminate strings in
  // case of overflow, but this is also said to be "platform specific".
  // <http://stackoverflow.com/questions/1270387/are-snprintf-and-friends-safe-to-use>
  //
  buf[sizeof(buf) - 1] = '\0';  // make sure it's always terminated

  return string(buf);
}

/*
* [lon_num, lat_num]= conv( str )
*
* References:
*   <http://wiki.weatherproof.fi/index.php?title=Most_common_Fminames_search_examples>
*   <http://wiki.weatherproof.fi/index.php?title=Fminames_API>
*
* TBD: We could also allow use of "geoid" numbers for locating places.
*      (Mikko Rauhala knows more details, or see 'FmiNames::FetchById()'
*/
static int conv(lua_State *L)
{
  proto_init(L);

  proto(L, "string,string,string,string,string,string");

  const char *dbhost = lua_tostring(L, 1);
  const char *dbuser = lua_tostring(L, 2);
  const char *dbpass = lua_tostring(L, 3);
  const char *dbname = lua_tostring(L, 4);
  const char *dbport = lua_tostring(L, 5);
  const char *s = lua_tostring(L, 6);

  Locus::Query lq(dbhost, dbuser, dbpass, dbname, dbport);
  Locus::QueryOptions opt;
  //                                                                      defaults
  // .SetCountries(const std::string & theCountries);                     // "fi"
  // .SetCountries(const std::list<std::string> & theCountries);
  // .SetExcludedCountries(const std::string & theCountries);
  // .SetExcludedCountries(const std::list<std::string> & theCountries);  // (none)
  // .SetResultLimit(int theLimit);                                       // 100
  // .SetFeatures(const std::string & theFeatures);
  // .SetFeatures(const std::list<std::string> & theFeatures);
  // default:
  // "PPLC" (pääkaupungit)
  // "ADMD" (kunnat)
  // "PPLA"
  // "PPLG"
  // "PPL" (taajamat; "populated place")
  // "ISL"
  // "PPLX"
  // "POST" (Finnish postal number area; potentially LARGE)
  // "SKI"
  //
  // others:
  // "AIRP" (airports)
  // ...

  // .SetFullCountrySearch(bool theFlag);                                 // false (DEPRECATED!)
  // .SetSearchVariants(bool theFlag);                                    // true
  // .SetLanguage(const std::string & theLanguage);                       // "fi"
  // .SetCharset(const std::string & theCharset);                         // "utf8"
  // .SetCollation(const std::string & theCollation);                     // "utf8_general_ci"
  // .SetAutoCollation(bool theValue);                                    // false
  // .SetAutocompleteMode(bool theValue);                                 // false
  // .SetPopulationMin(int theValue);                                     // 0
  // .SetPopulationMax(int theValue);                                     // 0

  //---
  // Settings must work on all of these:
  //      "Helsinki"
  //      "Turku"
  //      "Inari"                 (gives error with two Inaris, needing more precise string)
  //      "Inari,Inari"           (the more precise string)
  //      "Taalintehdas"          (gives just one entry, not separate PPL and POST)
  //      "Lintula,Jalasjärvi"    (extended UTF-8 chars ok)
  //      "Rome,Italy"
  //      ...
  //      "Villmanstrand"         (Swedish name for 'Lappeenranta')   DOES NOT CURRENTLY WORK!
  //      "Dahlsbruk"             (Swedish name for 'Taalintehdas')   DOES NOT CURRENTLY WORK!
  //---
  opt.SetCountries("all");
  // opt.SetResultLimit(10);

  // We cannot 'turn off' default features; must redeclare a subset instead.
  //
  opt.SetFeatures(
      "PPLC,"
      "ADMD,"
      "PPLA,"
      "PPLG,"
      "PPL,"
      "ISL,"
      "PPLX,"
      // "POST,"  // turned off, to get only one "Taalintehdas" (PPL)
      "SKI,"
      // "AIRP"   // turned off, to get only one "Turku" (PPLA)
      );

  opt.SetSearchVariants(false);
  //
  // With this enabled (and "all" countries), we'd get "Turco, Bolivia" for "Turku"

  opt.SetLanguage("fi,sv,en");
  //
  // Tried "en,fi,sv" to get 'Villmanstrand' and 'Dahlsbruk' to work. Nope.... TBD!

  opt.SetCharset("utf8");

  vector<Locus::SimpleLocation> v = lq.FetchByName(opt, s);
  //
  // FmiSimpleLocation:               i.e.
  //   string name;                   // "Inari"
  //   float lat;
  //   float lon;
  //   string country;                // "Suomi"
  //   string feature;                // "PPL" <- what is this?
  //   string description;            // "Taajama"
  //   string admin;                  // "Inari" | "Lieksa"
  //   string timezone;               // "Europe/Helsinki"
  //   unsigned int population;
  //   string iso2;                   // "FI"
  //   unsigned int id;
  //   unsigned int elevation;

  // i.e. "Taalintehdas" has two entries, differing with:
  //      feature         "PPL" | "POST"
  //      description     "Taajama" | "post office"

  unsigned n = v.size();

  /*
  * If there are multiple matches, give an error providing the available alternatives.
  */
  if (n > 1)
  {
    // 'lua_pushfstring()' does not support width modifiers so we do this using C++ strings
    // (and not 'lua_concat()').
    //
    string tmp = string_fmt("Location %s has %d matches (be more specific):", s, n);
    for (unsigned i = 0; i < n; i++)
    {
      // Give the name that the scripter can use, to get actual results.
      //
      string precise_name = v[i].name + ", " + (v[i].admin != "" ? v[i].admin : v[i].country);

      tmp += string_fmt("\n\t\"%s\" (%.4f, %.4f)", precise_name.c_str(), v[i].lat, v[i].lon);
      tmp += string_fmt(" admin: %s", v[i].admin.c_str());
      tmp += string_fmt(" country: %s", v[i].country.c_str());
      // tmp += string_fmt( " description: %s", v[i].description.c_str() );
      tmp += string_fmt(" feature: %s", v[i].feature.c_str());

      // '.population' is 0 at least for New York. Maybe there's some info for some city.
      //
      tmp += string_fmt(" population: %d", v[i].population);
    }
    luaL_error(L, tmp.c_str());
  }
  else if (n == 1)
  {
    lua_pushnumber(L, v[0].lon);
    lua_pushnumber(L, v[0].lat);
    return 2;
  }

  return 0;  // no matches
}

/*---=== Initialization ===---*/

/*
* Lua addon module entry point
*/
extern "C" int /*WIN32_DLLEXPORT*/ luaopen_fminames(lua_State *L)
{
  int st;

  //---
  // Push the precompiled Lua level chunk
  //
  st =
      luaL_loadbuffer(L, (char *)fminames_chunk, sizeof(fminames_chunk), NULL /*from precompiled*/);
  if (st)
  {
    // Can only be LUA_ERRMEM (the script is precompiled so no syntax errors)
    //
    return 0;  // don't return anything
  }

  lua_pushcfunction(L, conv);
  lua_call(L, 1 /*args*/, 0 /*results*/);

  return 0;  // nothing
}
