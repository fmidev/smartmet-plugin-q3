/*
 * WRAP.CPP                          Copyright (c) 2010, Ilmatieteen laitos
 *
 * Lua sandboxing
 */
#include "Wrap.hpp"
#include "Tools.h"

#include "LuaNew.h"
// L_ASSERT etc.

#include "Q3Engine.h"
#include "RegTools.h"

#include <map>
#include <sstream>
#include <string>

using namespace std;

static const luaL_Reg stdlibs[] = {
    {"", luaopen_base},                 // 'print' etc. and 'coroutine.*'
    {LUA_LOADLIBNAME, luaopen_package}, // 'require' and 'package.*'
    {LUA_TABLIBNAME, luaopen_table},    // 'table.*'
    {LUA_OSLIBNAME, luaopen_os},        // 'os.*'
    {LUA_STRLIBNAME, luaopen_string},   // 'string.*'
    {LUA_MATHLIBNAME, luaopen_math},    // 'math.*'
    {LUA_DBLIBNAME, luaopen_debug}, // 'debug' (only 'debug.getinfo' exposed)
    {nullptr, nullptr}};

static /*const*/ void *REGISTRY_KEY_HOOK =
    (void *)LuaWrapper::my_hook; // unique key

/*---=== LuaWrapper ===---
 */
LuaWrapper::LuaWrapper(bool ignore) : L(luaL_newstate()) {
  (void)ignore;
  if (!L)
    throw runtime_error("out of memory");

  // Open selected standard libraries
  //
  for (const luaL_Reg *lib = stdlibs; lib->func; ++lib) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }

  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");

  lua_pushcclosure(L, my_traceback, 1 /*upvalues*/);
  lua_insert(L, -2);
  //
  // [1]: 'my_traceback' function (REMAINS HERE THROUGHOUT LIFESPAN)
  // [2]: 'debug' table

  L_ASSERT(lua_gettop(L) == 2);

// Hide rest of 'debug.*' except for 'debug.getinfo' (used by some extension
// libraries and generally harmless)
//
#if 1
  // Create a new 'debug' table with only one entry
  //
  lua_newtable(L);
  lua_getfield(L, -2, "getinfo");
  //
  // [-1]: 'debug.getinfo'
  // [-2]: new empty table
  // [-3]: 'debug' table

  lua_setfield(L, -2, "getinfo");
  //
  // [-1]: { debug.getinfo }
  // [-2]: 'debug' table (not needed any more)

  lua_setglobal(L, "debug");
  lua_remove(L, -1);
#endif

  L_ASSERT(lua_gettop(L) == 1); // only 'my_traceback' is there

  INVARIANT();
}

/*
 */
void LuaWrapper::destroy() {
  if (L) {
    lua_close(L);
    L = 0;
  }
}

/*
 * Lua hook, called every N instructions (see 'lua_sethook()').
 *
 * Debug hooks cannot have upvalues of their own (they're stored in Lua as
 * C pointers) so we need to pass the end time via registry:
 *
 *   registry[my_hook] = ms_uint   End limit for allowed execution
 */
void LuaWrapper::my_hook(lua_State *L, lua_Debug *ar) {
  (void)ar; // debugging information not needed

  L_GROW(1);

  lua_pushlightuserdata(L, REGISTRY_KEY_HOOK);
  lua_gettable(L, LUA_REGISTRYINDEX);

  uint64_t run_until = (uint64_t)lua_tonumber(L, -1);
  assert(run_until > 0); // if no limit, the hook shouldn't be set up

  lua_pop(L, 1);

  if (now_ms() >= run_until) {
    luaL_error(L, "Script took too long to execute");
  }
}

/*
 * [str|any]= func( [str|any] )
 *
 * If parameter is string, append complete stack trace to it.
 * Based on code from Lua 5.1.4 'lua.c'.
 *
 * Upvalue 1:    'debug.traceback' function
 */
int LuaWrapper::my_traceback(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    return 1; // message not a string; keep it intact
  }

  L_GROW(3);

  lua_pushvalue(L, lua_upvalueindex(1));
  assert(lua_isfunction(L, -1));

  lua_pushvalue(L, 1); // duplicate of error message

  lua_pushinteger(L, 2); // depth; skip traceback and this function
  lua_call(L, 2 /*args*/,
           1 /*retval*/); // call upvalue (original 'debug.traceback')
  return 1;
}

/*
 * Run a query script, at 'L: [-1-args]'
 *
 * Returns 0 on success
 *       LUA_... error code + error message topmost on stack
 *       998 for trying to return multiple (binary) results (which cannot be
 * delivered by HTTP) 999 for internal bug situation
 */
int LuaWrapper::run(int args, RequestResponse &rr, double secs, int decimals
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
                    ,
                    bool binary_q2
#endif
) {
  // Set up killtime hook if required
  //
  if (secs > 0.0) {
    double run_until_ms = ((double)now_ms()) + secs * 1000;

    // 'my_hook' is a C function pointer so we cannot give upvalues to it;
    // use Lua registry to pass the end time.
    //
    lua_pushlightuserdata(L, REGISTRY_KEY_HOOK); // the key
    lua_pushnumber(L, run_until_ms);
    lua_settable(L, LUA_REGISTRYINDEX);

    // Only LUA_MASKCOUNT will guarantee the hook gets called even in
    // mid of an eternal loop s.a. 'while(true) do end'.
    //
    // The count parameter affects the reaction time; it does not really
    // matter which value is here (as long as its >100 or so).
    //
    // NOTE: This is enough for finding accidential eternal loops but
    //      NOT ENOUGH TO COUNTERFIGHT MALICOUS SCRIPTS intended to bring
    //      a server to its knees. Making a very elaborate 'string.gsub'
    //      call or similar will essentially take "forever" within a single
    //      Lua line. To counterattack such requires custom made protection
    //      to 'string.*' and 'table.*' (at least) features. Maybe Lua >5.1
    //      will provide more bullet proof sandboxing mechanisms for this.
    //      --AKa 27-Feb-2009
    //
    // NOTE: The script gets to run slightly longer than 'secs' since the check
    //      only happens every 'LUA_MASKCOUNT' commands. This should be okay.
    //
    lua_sethook(L, my_hook, LUA_MASKCOUNT, 500);
  }

  // Set output parameters
  //
  RegTools::set_Decimals(L, decimals);
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  RegTools::set_Binary(L, binary_q2);
#endif
  RegTools::set_JSONP(L, rr.get_jsonp_mode());

  // [1]: error function
  //  ...
  // [-1-args]: function to call (user provided code)
  // [...]: args

  L_GROW(2);
  lua_pushlightuserdata(L, (void *)&rr);
  lua_pushcclosure(L, run2_, 1 /*upvalues*/);

  lua_insert(L, -2 - args); // prior to the user code function

  // Catching C++ exceptions here is a safety precaution (should not happen)
  //
  // 'run2()' must be called with 'lua_pcall()' to catch any runtime errors
  // either in the user-provided code or in the conversion of return values.
  //
  try {
    int st = lua_pcall(L, args + 1, 0 /*retvals*/, 1 /*errfunc*/);
    return st;
  } catch (const E_ANY &e) {
    LOG_BUG("Uncaught exception: %s", e.what());
    L_GROW(1);
    lua_pushfstring(L, "Uncaught exception: %s", e.what());
    return 999;
  }
}

/*
 * 2nd stage of 'run', called via 'lua_pcall()' to collect all Lua errors with
 * just one 'lua_pcall()' (use of which is more costly than 'lua_call()' so
 * using just one is Good).
 *
 * void= run2( code_func [, ...] )
 *
 * Upvalues:
 *   1: 'rr' pointer
 */
int LuaWrapper::run2_(lua_State *L) {
  RequestResponse &rr =
      *((RequestResponse *)lua_touserdata(L, lua_upvalueindex(1)));

  lua_call(L, lua_gettop(L) - 1 /*args*/, LUA_MULTRET);

  // We can only provide one MIME type for the returned data. If all is text,
  // that's fine (and we can join multiple return values by newline). If there's
  // just one binary return value, that's also fine.
  //
  // For multiple (some binary) return values we could i.e. create a ZIP file on
  // the server side. Is this useful, or required? Could be used i.e. to craft
  // multiple pictures all at one round.
  //
  string mime = MIME_TEXT_UTF8;
  stringstream ss;

  unsigned retvals = lua_gettop(L);

#ifdef CONFIG_BINARY_OUTPUT_ENABLED
  if ((retvals > 1) && RegTools::get_Binary(L)) {
    luaL_error(L, "Cannot return multiple values as binary");
  }
#endif

  for (unsigned i = 1; i <= retvals; i++) {
    string_or_null mime2 = Q3Engine::result_(L, ss, i);

    // Text and JSON can be joined (by newline); binary (images, matrices)
    // cannot be combined with anything else.
    bool is_binary = mime2.c_str() && (mime2 != MIME_TEXT_UTF8) &&
                     (mime2 != MIME_JSON);

    if ((retvals > 1) && is_binary) {
      luaL_error(L, "Cannot return multiple values if one is non-text");
    }

    if (mime2.c_str()) {
      mime = mime2;
    }

    if (!is_binary) { // newline between multiple return values
      ss << "\n";
    }
  }

  rr.set_output(200, mime.c_str(), ss.str().c_str(), ss.str().size());

  return 0;
}
