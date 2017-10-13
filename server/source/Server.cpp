/*
* SERVER.CPP                      Copyright (c) 2008-2010, Ilmatieteen laitos
*
* General server interface to Q3 (abstract base for certain implementations)
*/
#include "Server.hpp"
#include "Q3Engine.h"

#include "Track.h"
#include "Versions.h"

#include "LogTools.h"
#include "Session.h"

#include "SyslogLogger.h"
#include "ZmqLogger.h"

#include "Wrap.hpp"

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

#include <assert.h>
#include <math.h>
#include <stdio.h>

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

static unsigned killtime_secs = 10;

static string_or_null package_path;
static string_or_null package_cpath;

#ifdef ONLINE_TESTBED
static string_or_null testbed;  // at "/q3"
#endif

/*---=== Helper routines ===---
*/

/*
* Seems C++ strings don't have tolower/toupper methods (even for ASCII).
* Solution from: <http://stackoverflow.com/questions/11491/string-to-lower-upper-in-c>
*/
static string string_tolower(const string &s)
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
static Logger *logger_parse(const char *conf)
{
  const unsigned BUFSIZE = 999;

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

  return NULL;  // unknown log string
}

/*---=== Configuration file ===---
*/

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

  char *line = 0;  // allocated and released by 'getline()'
  size_t len = 0;

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
    // testbed = filename
    // log = console|scribe|log4cpp|... [...params...]
    // package_path = str
    // package_cpath = str
    //
    char buf[200];
    strncpy(buf, line, sizeof(buf));  // truncates if 'line' is longer
    buf[sizeof(buf) - 1] = '\0';      // just in case (some implementations may not do it?)

    const char *key = 0;
    const char *val = 0;

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
        int i;
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
    // 'key': non-NULL if "key =" existed
    // "val": non-NULL if non-whitespace string followed

    bool eat_line = (key != NULL);
    if (key)
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
          // [Xh][YYmin][ZZsec] - same syntax as for Q3 engine time values
          //
          const char *pp = val;
          while (p)
          {
            char *end;
            int v = strtol(pp, &end, 10);

            if (begins_with(end, "h"))
            {
              killtime_secs += 60 * 60 * v;
              pp = end + 1;
            }
            else if (begins_with(end, "min"))
            {
              killtime_secs += 60 * v;
              pp = end + 3;
            }
            else if (begins_with(end, "sec"))
            {
              killtime_secs += v;
              pp = end + 3;
            }
            else
            {
              LOG_USAGE("Bad configuration: '%s'", line);
              killtime_secs = 0;
              break;
            }
          }
        }
      }
      else if (strcmp(key, "testbed") == 0)
      {
        testbed = val;
      }
      else
      {
        eat_line = false;
      }
    }
    s += eat_line ? "\n" : line;
  }
  if (line)
    free(line);  // like in "man getline" sample

  return s;  // all file contents (without comments and used entries)
}

/*
* Move a whole file to 'resp'
*/
#ifdef ONLINE_TESTBED
static bool file_to(RequestResponse &rr, const char *fn)
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
#endif

/*---=== Server object ===---
*/

/*---=== Query handling ===---*/

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
  LuaWrapper L(false);

  // Set globals. This must be done before 'q3.lua' which modifies some values
  // (validtime, origintime, gridsize).
  //
  if (key_val_as_globals)
  {
    for (map<string, string>::const_iterator it = key_val.begin(); it != key_val.end(); ++it)
    {
      lua_pushstring(L, it->first.c_str());
      lua_pushstring(L, it->second.c_str());
      lua_rawset(L, LUA_GLOBALSINDEX);
    }
  }

  lua_pushcfunction(L, Session::init);
  //
  lua_pushstring(L, package_path.c_str());
  lua_pushstring(L, package_cpath.c_str());

  int st;
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

    for (vector<string>::const_iterator it = names.begin(); it != names.end(); ++it)
    {
      const char *cstr = it->c_str();
      const Q3Engine::Track *track = itsEngine->getTrack(cstr);
      if (!track)
      {
        LOG_BUG("No track for %s", cstr);
      }

      lua_pushstring(L, cstr);
      new (L) TrackProxy(track);
      lua_settable(L, LUA_GLOBALSINDEX);
    }

    lua_newtable(L);
    int i = 0;

    for (vector<string>::const_iterator it = names.begin(); it != names.end(); ++it, i++)
    {
      lua_pushinteger(L, i + 1);
      lua_pushstring(L, it->c_str());
      lua_settable(L, -3);
    }

    lua_setglobal(L, "mt_tracknames");

// Make sure none of the preparations accidentially set '_' global
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
      unsigned args = 0;

      // Pass 'key_val' as param table (not globals)
      //
      if (!key_val_as_globals)
      {
        L_GROW(3);
        lua_newtable(L);
        for (map<string, string>::const_iterator it = key_val.begin(); it != key_val.end(); ++it)
        {
          lua_pushstring(L, it->first.c_str());
          lua_pushstring(L, it->second.c_str());
          lua_rawset(L, -3);
        }
        args = 1;
      }

      lua_pushstring(L, "");
      lua_setglobal(L, "first_origintime");

      st = L.run(args,
                 rr,
                 killtime_secs,
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

    lua_getglobal(L, "first_origintime");
    string_or_null fstot = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (fstot.c_str() && (fstot != ""))
      rr.set_first_origintime(fstot.c_str());
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

/*
*/
static void content_error(RequestResponse &rr, const string &s)
{
  rr.set_output(CODE_ERROR, "text/html", s.c_str());
}

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
  int decimals = -1;
  string err;
  int err_code = -1;

  for (map<string, string>::const_iterator it = rr.begin(); it != rr.end(); ++it)
  {
    string key = it->first;
    string val = it->second;
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
      decimals = atoi(val_c);  // 0..n
    }
    else if (key == "output")
    {
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
      if (val == "bin")
      {
        binary_q2 = true;
        continue;
      }
      else if (val == "text")
      {
        // nothing; default
        continue;
      }
      err = string_fmt("Unknown param: output=%s (expected 'bin')", val_c);
      goto ERROR;
#else
      err = "Binary output disabled.";
      goto ERROR;
#endif
    }
    else if (val == "")
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
ERROR:
  if (err_code)
  {
    const char *title;
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

  for (map<string, string>::const_iterator it = rr.begin(); it != rr.end(); ++it)
  {
    const char *key = it->first.c_str();
    const char *val = it->second.c_str();

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
    const char *title;
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
