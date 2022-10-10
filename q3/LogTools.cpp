/*
 * LOGTOOLS.CPP                    Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Logging from Metqu/Q3, into multiple log backbones.
 */
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include <pthread.h>

#include "JDay.h"
#include "LogTools.h"
#include "Tools.h"

#include "LuaNew.h"
// L_GROW

#include "ApiParam.h"
#include "Matrix.h" // MatrixSize
#include "MatrixPos.h"
#include "NA_Level.h"

#ifdef USE_SYSLOG
#include "SyslogLogger.h"
#endif

#ifdef USE_ZMQ
#include "ZmqLogger.h"
#endif

using namespace std;

/*---=== Helpers ===---*/

/*
 */
string string_fmt(const char *fmt, ...) {
  char buf[4000];

  va_list vl;
  va_start(vl, fmt);
  { vsnprintf(buf, sizeof(buf), fmt, vl); }
  va_end(vl);

  // Some sources say 'snprintf()' would not zero terminate strings in
  // case of overflow, but this is also said to be "platform specific".
  // <http://stackoverflow.com/questions/1270387/are-snprintf-and-friends-safe-to-use>
  //
  buf[sizeof(buf) - 1] = '\0'; // make sure it's always terminated

  return string(buf);
}

/*---=== Misc ===---*/

/*
 * Conversion functions required because we cannot read many headers in
 * 'Q3Log.h' (it is one of the most elementary headers itself).
 */
string E_ANY::to_str(const MatrixSize &sz) { return sz.asString(); }

string E_ANY::to_str(const MatrixPos &pos) { return pos.asString(); }

string E_ANY::to_str(const JDay &vt) { return vt.toString(); }

string E_ANY::to_str(const NA_Level &lev) { return lev.toString(); }

string E_ANY::to_str(const NA_Param &p) {
  return p.toString(false /*prefer native names*/);
}

/* ANSI terminal codes for effects (of LOG)
 *
 * Ref: <http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html>
 */
#define _ANSI_BOLD(str) "\x1b[1m" str "\x1b[22m"
#define _ANSI_RED(str) "\x1b[31m" str "\x1b[39m"
#define _ANSI_GREEN(str) "\x1b[32m" str "\x1b[39m"
#define _ANSI_YELLOW(str) "\x1b[33m" str "\x1b[39m"

/*---=== Logger ===---*/

// Note: The 'logger' object is never destructed. If needed, we can make here
//      a little static struct with a destructor that will take care of that.

Logger *Logger::logger; // = 0

/*
 * Initialize a logging system
 */
void Logger::init(Logger *logger_) {

  // We don't allow changing a logger once it's set. If we do, we need locks in
  // use of logger (and that we don't want).
  //
  if (logger) {
    throw E_LOG_ERROR0("Logger can be set only once.");
  }

  logger = logger_;
}

/*
 * = LOG( ... )                  -- log the parameters
 *
 * = LOG{ timing=true|false }    -- start/stop ms time stamps on the logs
 *
 * Logging events from _built in_ Lua code (not code from the user) to console
 * or log file.
 *
 * Note: Currently this is allowed for client code as well, but could eventually
 *       be disabled.
 *
 * NOTE: Do NOT limit the length of the logged string in any way. DUMP() uses
 * this and can generate rather long string for debugging purposes. DO NOT pass
 *       anything through 'string_fmt' that has an arbitrary (truncating) buffer
 *       limit.
 *
 * Upvalues: #1 if 'true', reports one level higher than normal (used by
 * 'DUMP()')
 */
int Logger::LOG_(lua_State *L) {

  // If logging not active, don't waste time
  //
  if (!logger) {
    return 0;
  }

  bool one_up = lua_toboolean(L, lua_upvalueindex(1));

#ifdef METQU
  unique_t id = 0; // id's only used in server mode
#else
  L_GROW(1);

  // Get the id for this script
  //
  lua_pushlightuserdata(L, (void *)gen_unique);
  lua_gettable(L, LUA_REGISTRYINDEX);
  unique_t id = (unique_t)lua_tonumber(
      L,
      -1); // double has 56-bit accuracy (Lua integers are normally 32-bit only)

  if (!id) {
    // Do NOT bug about this. Can occur i.e. if using 'LOG' in 'proto.lua'
    // initialization.
    //
    // LOG_BUG0( "Lua state did not have a unique id" );
  }
  lua_pop(L, 1);
#endif

  //---
  static const void *REG_LOG_timings =
      (void *)Logger::LOG_; // Use the function pointer as register key

  // Check if it's a timing on/off call.
  //
  if (lua_istable(L, 1) && (lua_gettop(L) == 1)) {
    L_GROW(2);

    lua_pushliteral(L, "timing");
    lua_gettable(L, 1);

    if (!lua_isboolean(L, -1)) {
      luaL_error(L,
                 "Bad LOG call, expected LOG{ timing=true/false } or LOG(...)");
    }

    bool timings_on = lua_toboolean(L, -1);
    lua_pop(L, 1);

    // Set registry value of this Lua state to current ms value (if 'on') or to
    // nil (if 'off')
    //
    lua_pushlightuserdata(L, const_cast<void *>(REG_LOG_timings));
    if (timings_on) {
      lua_pushnumber(L, (double)now_ms());
    } else {
      lua_pushnil(L);
    }
    lua_settable(L, LUA_REGISTRYINDEX);

    return 0; // pushed nothing
  }

  // Read timing since 'LOG{ timing=true }' if that has been enabled.
  //
  long int timing_ms; // -1 if timings not enabled
  {
    L_GROW(1);

    lua_pushlightuserdata(L, const_cast<void *>(REG_LOG_timings));
    lua_gettable(L, LUA_REGISTRYINDEX);

    uint64_t t0 = (uint64_t)lua_tonumber(L, -1); // 0 if nil
    lua_pop(L, 1);

    timing_ms = t0 ? (long int)(now_ms() - t0) : -1;
  }

  L_GROW(
      1 +
      lua_gettop(
          L)); // we essentially double the entries, by adding '\t' in between

  // Change parameters to string form (we may modify the slots, they are ours)
  //
  for (unsigned i = 1; i <= (unsigned)lua_gettop(L); i += 2) {
    if (!lua_tostring(L, i)) {
      lua_pushstring(L, L_typename(i));
      lua_replace(L, i);
    }
    if (i < (unsigned)lua_gettop(L)) {
      lua_pushliteral(L, "\t");
      lua_insert(L, i + 1);
    }
  }

  // Prefix with timing info
  //
  if (timing_ms >= 0) {
    lua_pushfstring(L, "(%ld ms) ", timing_ms);
    lua_insert(L, 1);
  }

  lua_concat(L, lua_gettop(L)); // Concatenates all values in stack

  const char *name = "???";
  unsigned line = 0;

  // With Lua debug API we (should) get the caller's file & line
  //
  lua_Debug ar;
  int rc = lua_getstack(L, one_up ? 2 : 1 /*just above us*/, &ar);
  if (rc > 0) {
    rc = lua_getinfo(L, "lS" /*current line & source*/, &ar);
    if (rc > 0) {
      name = ar.short_src; // "printable version of source" for error messages
      line = ar.currentline;
    }
  }

  // Lua can provide us the string length without looking for the end char.
  //
  size_t msg_len = 0;
  const char *msg = lua_tolstring(L, 1, &msg_len);

  logger->log(_LUA, name, line, id, msg, msg_len);

  return 0; // pushes nothing
}

/*
 * Create unique id's for the loggers
 *
 * Note: These are unique PER SERVER INSTANCE only. Should be enough for us.
 */
#ifndef METQU
Logger::unique_t Logger::gen_unique() {
  static Mutex my_m;
  static uint64_t last = 0;

  // Reduce some offset from the times to make the values reasonably small.
  //
  // Note: Reading the time outside of the lock should minimize locked time.
  //
  uint64_t ms = now_ms() - 1263567375545;

  {
    ClaimMutex lock(my_m);
    if (last >= ms) {
      ms = last + 1; // Taking up such time (it may be slightly in the future,
                     // no problem)
    }
    last = ms;
  }

  return ms;
}
#endif

/*---=== Stderr logging ===---
 *
 * Note: calls to logging are _not_ multithreading protected; that is up to the
 *       callback to do (i.e. writing to 'stderr' is atomic in itself).
 */

void StderrLogger::log(enum category cat, const char *file, unsigned line,
                       const unique_t id_, const char *msg, size_t msg_len) {

  (void)id_; // useful in distributed logs (not shown here)

  const char *ansi = nullptr;
  switch (cat) {
  case _INFO:
    ansi = _ANSI_GREEN("OK");
    break;
  case _WARNING:
    ansi = _ANSI_YELLOW("WARNING");
    break;
  case _ERROR:
    ansi = _ANSI_RED("ERROR");
    break;
  case _FATAL:
    ansi = _ANSI_RED("FATAL");
    break;
  case _DEBUG:
    ansi = "DEBUG";
    break;
  case _LUA:
    ansi = _ANSI_YELLOW("LUA");
    break;
  case _BUG:
    ansi = _ANSI_RED("BUG");
    break;
  case _USAGE:
    ansi = _ANSI_YELLOW("USAGE");
    break;
  case _TIMING:
    ansi = _ANSI_YELLOW("TIMING");
    break;
  case _STAT:
    ansi = _ANSI_YELLOW("STAT");
    break;
  case _MAINTENANCE:
    ansi = _ANSI_YELLOW("MAINTENANCE");
    break;
  }

  // NOTE: Do NOT use 'string_fmt()' on 'msg' - it may be REALLY LONG (i.e. from
  // script 'DUMP()') and
  //      'string_fmt()' would truncate.
  //
  string head = string_fmt("%s [%s %d] ", ansi, file, line);
  size_t head_len = head.length();

  char buf[head_len + msg_len + 2]; // '\n' and terminating '\0'
  {
    strcpy(buf, head.c_str());
    strcpy(buf + head_len, msg); // potentially long (as said)
    strcpy(buf + head_len + msg_len, "\n");

    cerr << buf; // atomic, instant flush
  }
}
