/*
* SERVER.CPP                      Copyright (c) 2008-2010, Ilmatieteen laitos
*
* General server interface to Q3 (abstract base for certain implementations)
*/
#include "Server.hpp"
#include <macgyver/StringConversion.h>
#include "Q3Engine.h"

#include "Track.h"
#include "Versions.h"

#include "LogTools.h"
#include "Session.h"

#include "SyslogLogger.h"
#include "ZmqLogger.h"

#include "Wrap.hpp"
#include "Limits.h"

/*
* Enabling this will make "validTime=..." work as "validtime=..."
*/
#define Q2_URL_ALIASES

/*
* Enable '/q3' to give an interactive testbed.
*/
#define ONLINE_TESTBED

#include <iostream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>

#ifdef ONLINE_TESTBED
#include <fstream>
#endif

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cmath>

#include <pthread.h>

using namespace std;

/*
* Ref: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
*/
enum
{
  // 2xx: "request was successfully received, understood, and accepted"
  //
  CODE_OK = 200,

  // 5xx: "server is aware that it has erred or is incapable of performing the request"
  // CODE_ERROR_INTERNAL=500,
  // CODE_ERROR_NOT_IMPLEMENTED=501,
  //
  CODE_ERROR = 500
};

namespace
{
unsigned killtime_secs = 10;

// Memory budget of a single Lua session in megabytes (0 for no limit)
size_t memory_limit_mb = 1024;

string_or_null package_path;
string_or_null package_cpath;
}  // namespace

#ifdef ONLINE_TESTBED
namespace
{
string_or_null testbed;  // at "/q3"
}  // namespace
#endif

/*---=== Helper routines ===---
*/

namespace
{
/*
* Seems C++ strings don't have tolower/toupper methods (even for ASCII).
* Solution from: <http://stackoverflow.com/questions/11491/string-to-lower-upper-in-c>
*/
string string_tolower(const string &s)
{
  string ret = s;
  std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
  return ret;
}

/*
* Parse a log config entry
*
* Stderr:   "stderr"
* Syslog:   "syslog facility"
*               facility: "daemon"|"user"|"local0".."local7"
* ZeroMQ:   "zmq x.x.x.x:port"
* ...
*/
Logger *logger_parse(const char *conf)
{
  const unsigned BUFSIZE = 999;

  if (!conf)
    throw runtime_error("Missing value for the 'log' setting");

  cerr << string_fmt("Logging to: %s\n", conf);

  if (strlen(conf) >= BUFSIZE)
  {
    throw runtime_error(string_fmt("Entry too long: %s", conf));
  }

  if (strcmp(conf, "stderr") == 0)
  {
    return new StderrLogger();
  }

  // Syslog (always available in Linux)
  {
    char facility[BUFSIZE];
    int rc = sscanf(conf, "syslog %s", facility);
    if (rc == 1)
    {
      return new SyslogLogger(facility);
    }
  }

  {
    char addr[BUFSIZE];  // "x.x.x.x:port"
    int rc = sscanf(conf, "zmq %s", addr);
    if (rc == 1)
    {
      return new ZmqLogger(addr);
    }
  }

  return nullptr;  // unknown log string
}
}  // namespace

/*---=== Configuration file ===---
*/

namespace
{
/*
* Split a configuration line in 'buf' (modified in place) to 'key' and 'val'.
*
* 'key': non-nullptr if "key =" existed
* "val": non-nullptr if non-whitespace string followed
*/
void split_config_line(char *buf, const char *&key, const char *&val)
{
  for (char *pp = buf; *pp; pp++)
  {
    if (!key)
    {  // looking for '='
      if (isspace(*pp))
      {
        *pp = '\0';  // cut at first white space (don't expect spaces within key names)
      }
      else if (*pp == '=')
      {
        *pp = '\0';  // in case there wasn't white space
        key = buf;
      }
    }
    else if (isspace(*pp))
    {
      // skip white space after the '=' (if any)
    }
    else
    {
      // Cut white space trail off 'val' (there's at least a newline)
      //
      int i = 0;
      for (i = strlen(pp) - 1; i >= 0; i--)
      {
        if (!isspace(pp[i]))
          break;
      }
      // 'i' is the index of the last non-space char or -1
      //
      pp[i + 1] = '\0';

      val = pp;
      break;  // done!
    }
  }
}

/*
* Parse a 'killtime' value [Xh][YYmin][ZZsec] - same syntax as for Q3 engine
* time values. A plain "0" disables the limit.
*
* Note: 'val' has already been stripped of a tail comment and trailing white space.
*
* Throws on a bad value instead of silently disabling the limit.
*/
unsigned parse_killtime(const char *val, const char *line)
{
  if (strcmp(val, "0") == 0)
    return 0;  // no limit

  unsigned long secs = 0;
  const char *pp = val;
  while (*pp)
  {
    if (isspace(*pp))
    {
      pp++;
      continue;
    }

    char *end = nullptr;
    errno = 0;
    long v = strtol(pp, &end, 10);

    if (end == pp || errno != 0 || v < 0)
      throw runtime_error(string_fmt("Bad killtime entry: %s", line));

    if (begins_with(end, "h"))
    {
      secs += 60UL * 60UL * v;
      pp = end + 1;
    }
    else if (begins_with(end, "min"))
    {
      secs += 60UL * v;
      pp = end + 3;
    }
    else if (begins_with(end, "sec"))
    {
      secs += v;
      pp = end + 3;
    }
    else
    {
      throw runtime_error(string_fmt("Bad killtime entry: %s", line));
    }

    if (secs > 7UL * 24 * 60 * 60)
      throw runtime_error(string_fmt("Too large killtime entry: %s", line));
  }

  return static_cast<unsigned>(secs);
}

/*
* Parse a non-negative integer configuration value (0 for no limit)
*/
size_t parse_limit(const char *val, const char *line)
{
  char *end = nullptr;
  errno = 0;
  long v = strtol(val, &end, 10);
  if (end == val || *end != '\0' || errno != 0 || v < 0)
    throw runtime_error(string_fmt("Bad configuration entry: %s", line));
  return static_cast<size_t>(v);
}

/*
* Set the global of a configuration entry. Returns false for keys not used
* here.
*/
bool apply_config_entry(const char *key, const char *val, const char *line)
{
  if (strcmp(key, "log") == 0)
  {
    Logger *lgr = logger_parse(val);
    if (!lgr)
    {
      throw runtime_error(string_fmt("Bad log entry: %s", line));
    }
    Logger::init(lgr);  // remains valid throughout process lifespan (not deleted)
  }
  else if (strcmp(key, "package_path") == 0)
  {
    package_path = val;
  }
  else if (strcmp(key, "package_cpath") == 0)
  {
    package_cpath = val;
  }
  else if (strcmp(key, "killtime") == 0)
  {
    killtime_secs = 0;  // no limit
    if (val)
    {
      killtime_secs = parse_killtime(val, line);
    }
  }
  else if (strcmp(key, "memory_limit_mb") == 0)
  {
    memory_limit_mb = 0;  // no limit
    if (val)
    {
      memory_limit_mb = parse_limit(val, line);
    }
  }
  else if (strcmp(key, "max_image_pixels") == 0)
  {
    Limits::max_image_pixels = 0;  // no limit
    if (val)
    {
      Limits::max_image_pixels = static_cast<int64_t>(parse_limit(val, line));
    }
  }
  else if (strcmp(key, "max_grid_cells") == 0)
  {
    Limits::max_grid_cells = 0;  // no limit
    if (val)
    {
      Limits::max_grid_cells = static_cast<int64_t>(parse_limit(val, line));
    }
  }
  else if (strcmp(key, "testbed") == 0)
  {
    testbed = val;
  }
  else
  {
    return false;
  }
  return true;
}
}  // namespace

/*
* Read certain entries from the configuration, and set them to globals.
*
* Returns: The whole configuration file (with used entries replaced by empty lines)
*/
string read_config(const char *fn)
{
  string s;

  FILE *f = fopen(fn, "r");
  if (!f)
  {
    // Not logging, yet
    throw runtime_error(string_fmt("Cannot read: %s", fn));
  }

  char *line = nullptr; // allocated and released by 'getline()'
  size_t len = 0;

  // Release the file and the getline() buffer also if a config entry throws
  struct Cleanup
  {
    FILE *file;
    char *&buffer;
    Cleanup(FILE *theFile, char *&theBuffer) : file(theFile), buffer(theBuffer) {}
    Cleanup(const Cleanup &) = delete;
    Cleanup &operator=(const Cleanup &) = delete;
    ~Cleanup()
    {
      free(buffer);  // like in "man getline" sample
      (void)fclose(file);
    }
  } cleanup{f, line};

  while (getline(&line, &len, f) != -1)
  {
    // "Buffer is null-terminated and includes the newline character, if one was found"
    //
    char *p = strchr(line, '#');
    if (p)
      strcpy(p, "\n");  // remove tail comments

    // Find if the line begins with any key we use:
    //
    // killtime = [Xh][YYmin][ZZsec]
    // memory_limit_mb = megabytes (0 for no limit)
    // max_image_pixels = pixels (0 for no limit)
    // max_grid_cells = cells (0 for no limit)
    // testbed = filename
    // log = console|scribe|log4cpp|... [...params...]
    // package_path = str
    // package_cpath = str
    //
    char buf[200];
    strncpy(buf, line, sizeof(buf));  // truncates if 'line' is longer
    buf[sizeof(buf) - 1] = '\0';      // just in case (some implementations may not do it?)

    const char *key = nullptr;
    const char *val = nullptr;

    split_config_line(buf, key, val);

    // 'key': non-nullptr if "key =" existed
    // "val": non-nullptr if non-whitespace string followed

    bool eat_line = (key != nullptr) && apply_config_entry(key, val, line);
    s += eat_line ? "\n" : line;
  }
  return s;  // all file contents (without comments and used entries)
}

/*
* Move a whole file to 'resp'
*/
#ifdef ONLINE_TESTBED
namespace
{
bool file_to(RequestResponse &rr, const char *fn)
{
  ifstream fs(fn, ios::in | ios::binary | ios::ate);  // 'ate' positions at end of file

  if (fs.is_open())
  {
    size_t sz = fs.tellg();
    char *block = new char[sz];
    fs.seekg(0, ios::beg);
    fs.read(block, sz);  // all at once
    fs.close();

    rr.set_output(CODE_OK, "text/html", block, sz);
    delete[] block;
    return true;
  }
  LOG_WARNING("Providing %s FAILED", fn);
  return false;
}
}  // namespace
#endif

/*---=== Server object ===---
*/

/*---=== Query handling ===---*/

namespace
{
/*
* Set 'key_val' as globals
*/
void set_globals(lua_State *L, const map<string, string> &key_val)
{
  for (const auto &it : key_val) {
    lua_pushstring(L, it.first.c_str());
    lua_pushstring(L, it.second.c_str());
    lua_rawset(L, LUA_GLOBALSINDEX);
  }
}

/*
* Set producer proxies ('HIR' etc.)
*/
void set_track_proxies(lua_State *L, const Q3Engine &engine, const vector<string> &names)
{
  for (const auto &name : names) {
    const char *cstr = name.c_str();
    const Q3Engine::Track *track = engine.getTrack(cstr);
    if (!track)
    {
      LOG_BUG("No track for %s", cstr);
      continue;
    }

    lua_pushstring(L, cstr);
    new (L) TrackProxy(track, (cstr != track->getName() ? cstr : ""));
    lua_settable(L, LUA_GLOBALSINDEX);
  }
}

/*
* Set the names of the tracks (other than aliases) as 'mt_tracknames' global
*/
void set_track_names(lua_State *L, const Q3Engine &engine, const vector<string> &names)
{
  lua_newtable(L);
  int i = 0;

  for (const auto &name : names) {
    const char *cstr = name.c_str();
    const Q3Engine::Track *track = engine.getTrack(cstr, false);
    if (!track)
    {
      LOG_BUG("No track for %s", cstr);
      continue;
    }

    if (!(track->getAlias().empty()))
      continue;

    lua_pushinteger(L, i + 1);
    lua_pushstring(L, name.c_str());
    lua_settable(L, -3);

    i++;
  }

  lua_setglobal(L, "mt_tracknames");
}

/*
* Run the code chunk from user (function at [1])
*/
int run_code(LuaWrapper &L,
             const map<string, string> &key_val,
             bool key_val_as_globals,
             RequestResponse &rr,
             int decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
             ,
             bool binary_q2
#endif
             )
{
  unsigned args = 0;

  // Pass 'key_val' as param table (not globals)
  //
  if (!key_val_as_globals)
  {
    L_GROW(3);
    lua_newtable(L);
    for (const auto &it : key_val) {
      lua_pushstring(L, it.first.c_str());
      lua_pushstring(L, it.second.c_str());
      lua_rawset(L, -3);
    }
    args = 1;
  }

  lua_pushstring(L, "");
  lua_setglobal(L, "first_origintime");

  return L.run(args,
               rr,
               killtime_secs,
               decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
               ,
               binary_q2
#endif
               );
}

/*
* Set the origintime of the first used querydata (stored as a lua global) to
* response header
*/
void set_first_origintime(lua_State *L, RequestResponse &rr)
{
  lua_getglobal(L, "first_origintime");
  string_or_null fstot = lua_tostring(L, -1);
  lua_pop(L, 1);

  if (fstot.c_str() && (!fstot.empty()))
    rr.set_first_origintime(fstot.c_str());
}
}  // namespace

/*
 * Data responses (to the caller) are to be provided over the 'rr' class
 * whereas errors (Lua 'error' calls, or syntax errors) are over 'error'
 * string.
 *
 * Returns: 0 for okay, LUA_ERRRUN/LUA_ERRMEM/... for not
*/
int Q3Server::query(const map<string, string> &key_val,
                    bool key_val_as_globals,
                    RequestResponse &rr,
                    string &err,
                    int decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                    ,
                    bool binary_q2
#endif
                    )
{
  err = "";

  // Prepare a Lua session, with sandboxing and Q3 specific bindings
  //
  // Note: Preparation takes 1..2 ms on crash.fmi.fi (64-bit Linux). Not worth making
  //      cached states that would be instantly available.    --AKa 10-Sep-2009
  //
  LuaWrapper L(memory_limit_mb * 1024 * 1024);

  // Set globals. This must be done before 'q3.lua' which modifies some values
  // (validtime, origintime, gridsize).
  //
  if (key_val_as_globals)
  {
    set_globals(L, key_val);
  }

  lua_pushcfunction(L, Session::init);
  //
  lua_pushstring(L, package_path.c_str());
  lua_pushstring(L, package_cpath.c_str());

  int st = 0;
  try
  {
    st = lua_pcall(L, 2 /*args*/, 0 /*retvals*/, 0 /*no errfunc*/);
  }
  catch (const E_OUT_OF_MEMORY &)
  {
    // Out of memory loading one of the precompiled chunks (should be very rare!).
    //
    err = "Out of memory";
    return LUA_ERRMEM;
  }

  if (st)
  {
    // Most often, this is because of bad parameters (s.a. 'validtime' not in right syntax).
    // Can also be a bug in the initialization scripts.
    //
    LOG_USAGE("Failed to initialize: %s", lua_tostring(L, -1));
  }
  else
  {
// Provide the RPM package version (if built for RPM)
//
#ifdef RPM_PACKAGE
    lua_pushstring(L, RPM_PACKAGE);
    lua_setglobal(L, "RPM_PACKAGE");
#endif
#ifdef RPM_VERSION
    lua_pushstring(L, RPM_VERSION);
    lua_setglobal(L, "RPM_VERSION");
#endif

    // Plugin specific addons
    //
    TrackProxy::create_mt(L);

    lua_getglobal(L, "proto");
    {
      L_ASSERT(lua_istable(L, -1));

      lua_pushstring(L, TrackProxy::name());
      lua_pushcfunction(L, TrackProxy::is);
      lua_settable(L, -3);
    }
    lua_pop(L, 1);

    // Set producer proxies ('HIR' etc.)
    //
    vector<string> names = itsEngine->getNames();

    set_track_proxies(L, *itsEngine, names);
    set_track_names(L, *itsEngine, names);

// Make sure none of the preparations accidentally set '_' global
// (this is just to keep clean).
//
#if 1
    lua_pushliteral(L, "_");
    lua_rawget(L, LUA_GLOBALSINDEX);
    L_ASSERT(lua_isnil(L, -1));
    lua_pop(L, 1);
#endif

    // Use a custom reader for transforming URL escapes
    //
    // Note: Chunk name is used in error messages. Prepending it with "="
    //      makes it behave like if filename:
    //
    //      "=xxx"  ->  xxx:<line number>: <error description>
    //      "xxx"   ->  [string "xxx"]:<line number>: <error description>
    //
    st = rr.compile_code(L, "=code");
    //
    // 0: success (chunk on stack)
    // LUA_ERRSYNTAX: syntax error (msg on stack)
    // LUA_ERRMEM: out of memory

    if (st == 0)
    {
      // [1]: code chunk from user (function)
      //
      st = run_code(L,
                    key_val,
                    key_val_as_globals,
                    rr,
                    decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                    ,
                    binary_q2
#endif
                    );
    }
  }

  if (st != 0)
  {
    err = lua_tostring(L, -1);
  }
  else
  {
    // 16-Apr-2015 PKi: If any querydata was used, first querydata's origintime was stored as a lua
    // global; set it to response header

    set_first_origintime(L, rr);
  }

// Note: If we ever cache the Lua states, also the cleanup should be done
//      by another thread (after this point, we don't need the Lua state
//      any more).
//
//      Measured cleanup times are 0..1 ms (crash.fmi.fi, 64-bit Linux).
//
//      If we do the cleanup here (implicitly, by the 'L' destructor), we're
//      delaying the moment the HTTP response actually gets sent.
//          --AKa 10-Sep-2009
//
#if 0
    uint64_t t0= now_ms();
    LW.destroy();
    LOG_DEBUG( "Destruction of Lua state took: %d ms", (int)(now_ms() - t0) );
#endif

  return st;
}

namespace
{
/*
*/
void content_error(RequestResponse &rr, const string &s)
{
  rr.set_output(CODE_ERROR, "text/html", s.c_str());
}

/*
* Move params from 'rr' to a standard mapping. Returns false with 'err' set
* for bad params.
*/
bool read_request_params(RequestResponse &rr,
                         map<string, string> &key_val,
                         int &decimals,
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                         bool &binary_q2,
#endif
                         string &err)
{
  for (const auto &it : rr) {
    string key = it.first;
    string val = it.second;
    // const char *key_c= key.c_str();
    const char *val_c = val.c_str();

    // Note: SmartMet Server or Shttpserver seems to be erroneously giving us
    //      "_"=<big number> i.e. if there is "code=return%20_" in the params.
    //      Eliminate this ('_' is normally used in Lua as a local 'ignore' drain,
    //      and its global should not carry a value).   --AKa 1-Feb-2010
    //
    if (key == "_")
    {
      continue;  // ignore
    }

    if ((key == "code") || (key == "callback") /*JSONP*/)
    {
      continue;  // already used
    }

    if (key == "decimals" ||
        key == "maxdecimals"  // AKa 2-Feb-2010: should really decide on the proper names?
#ifdef Q2_URL_ALIASES
        ||
        key == "maxDecimals"
#endif
        )
    {
      decimals = Fmi::stoi(val_c);  // 0..n
    }
    else if (key == "output")
    {
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
      if (val == "bin")
      {
        binary_q2 = true;
        continue;
      }
      if (val == "text")
      {
        // nothing; default
        continue;
      }
      err = string_fmt("Unknown param: output=%s (expected 'bin')", val_c);
      return false;
#else
      err = "Binary output disabled.";
      return false;
#endif
    }
    else if (val.empty())
    {
// "..&key=&.."; no value for the key is skipped. We don't set the value to an empty
//      string. This is mostly for making life easy for test suite Makefile.

#ifdef Q2_URL_ALIASES
    }
    else if ((key == "validTime") || (key == "gridSize") || (key == "originTime"))
    {
      key_val[string_tolower(key)] = val;
#endif
    }
    else
    {
      // Q3 itself uses 'validtime', 'projection', 'gridsize' and 'origintime'.
      // Other globals can be used for giving parameters to the script.
      //
      key_val[key] = val;
    }
  }
  return true;
}

/*
* Output the error of a query
*/
void output_query_error(RequestResponse &rr, int err_code, const string &err)
{
  const char *title = nullptr;
  switch (err_code)
  {
    case LUA_ERRSYNTAX:
      title = "Syntax error";
      break;
    case LUA_ERRRUN:
      title = "Runtime error";
      break;
    case LUA_ERRMEM:
      title = "Out of memory";
      break;
    default:
      title = "Unknown error";
      break;  // i.e. no 'code' field
  }

  // Let the client format the message, always.
  // Convention now is we start with 'xxx error[: ...]' (without hyphens) which means an error.
  // Any valid returned string will be in JSON format (with hyphens)
  //  --AKa 11-Dec-2009
  //
  const char *err_c = err.c_str();
  string out = (!err_c) ? string(title) : string_fmt("%s: %s", title, err_c);
  LOG_DEBUG("Giving out error: %s", out.c_str());

  rr.set_output(CODE_ERROR, "text/plain", out.c_str());
}
}  // namespace

/*
*/
void Q3Server::native_handler(RequestResponse &rr)
{
  if (!rr.has_code())
  {
#ifdef ONLINE_TESTBED
    const char *fn = testbed.c_str();
    if (fn)
    {
      /*
      * Note: CodeMirror and jQuery libraries used by the file need to be separately provided
      *       (preferably within the FMI network).
      */
      if (!file_to(rr, fn))
      {
        LOG_ERROR("Could not read: %s", fn);

        // Not configured or unable to open the file
        //
        content_error(rr, string_fmt("Testbed file not found: %s.", fn));
      }
      return;
    }
#endif
    content_error(rr, "No 'code=' field");
    return;
  }

  LOG_OK("Code: %s", rr.get_code());

  /*
  * Move params from 'rr' to a standard mapping
  *
  * Some keys have special meaning; others will be passed on as globals
  * to the Lua state.
  */
  map<string, string> key_val;
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  bool binary_q2 = false;
#endif
  int decimals = 0;
  string err;
  int err_code = -1;

  if (!read_request_params(rr,
                           key_val,
                           decimals,
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                           binary_q2,
#endif
                           err))
  {
    output_query_error(rr, err_code, err);
    return;
  }

  err_code = query(key_val,
                   true /*to globals*/,
                   rr,
                   err,
                   decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                   ,
                   binary_q2
#endif
                   );

// Note: We could use different status codes (5xx) for the syntax, runtime and memory errors
//
  if (err_code)
  {
    output_query_error(rr, err_code, err);
  }
}

/*
* WMS context handler
*/
#ifdef ENABLE_OGC_WMS
void Q3Server::wms_handler(RequestResponse &rr)
{
#if 1
  LOG_OK("WMS query from %s: %s",
         rr.get_client_ip().c_str(),  // x.x.x.x
         rr.get_query_string().c_str());
#endif

  /*
  * Sample WMS query:
  *
  * <http://...?
  *   map=/var/www/html/mapserver/mapfiles/radar_finland_1km_5min_b.map
  *   SERVICE=WMS
  *   VERSION=1.1.1           -- (optional)
  *   REQUEST=GetMap          -- WMS request kind: "GetCapabilities", "GetMap" or "GetFeatureInfo"
  * (optional)
  *   SRS=epsg:2393           -- projection
  *   BBOX=3016850,6445445,3783538,7680553
  *   format=image/png        -- requested MIME type (optional)
  *   styles=default
  *   WIDTH=760
  *   HEIGHT=1226
  *   layers=tutka
  *   EXCEPTIONS=INIMAGE      -- format for error messages
  *   time=200908110630
  *   UPDATESEQUENCE=xxx      -- (optional); "integer or timestamp string or any other string"
  * >
  *   -->
  * require "wms"
  * return wms.query{ kkk=xxx, ... }
  *
  * Note: We switch keys to lower case; WMS requires case ignorance for keys but not for values.
  */
  map<string, string> key_val;

  for (const auto &it : rr) {
    const char *key = it.first.c_str();
    const char *val = it.second.c_str();

    // Store the keys in lower case (WMS standard requires the keys to be case independent)
    //
    key_val[string_tolower(key)] = val;
  }

  rr.set_script("require \"wms\"; return wms.query(...)");

  string err;
  int err_code = query(key_val,
                       false /*keyval as call params to 'code'*/,
                       rr,
                       err,
                       -1
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                       ,
                       false /*text output*/
#endif
                       );

  if (err_code)
  {
    const char *title = nullptr;
    switch (err_code)
    {
      case LUA_ERRSYNTAX:
        title = "Syntax error";
        break;
      case LUA_ERRRUN:
        title = "Runtime error";
        break;
      case LUA_ERRMEM:
        title = "Out of memory";
        break;
      default:
        title = "Unknown error";
        break;  // i.e. no 'code' field
    }

    const char *err_c = err.c_str();
    string out = (!err_c) ? string(title) : string_fmt("%s: %s", title, err_c);
    LOG_DEBUG("Giving out error: %s", out.c_str());

    rr.set_output(CODE_ERROR, "text/plain", out.c_str());
  }
}
#endif
// ENABLE_OGC_WMS

Q3Server::Q3Server(const char *cfg)
{
  // Read in the configuration, and take over logging
  //
  string conf = read_config(cfg);

  // Note: Naming of 'Q3Engine' is a relic from time it used to be a SmartMet Server engine.
  //      It no longer is, this code is compiler right-in to us, and the 'Q3Server' and
  //      'Q3Engine' classes could be merged.     --AKa 30-Jun-10
  //
  itsEngine = new Q3Engine(conf.c_str());
}

Q3Server::~Q3Server()
{
  // Actually never gets here (at least with SmartMet Server plugin)

  delete itsEngine;
}
