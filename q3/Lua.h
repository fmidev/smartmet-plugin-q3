/*
 * LUA.H
 *
 * Include this instead of <lua.hpp>.
 *
 * The Lua error functions never return (they unwind to the enclosing
 * lua_pcall), but the LuaJIT headers do not say so. Without the attribute
 * the compiler and static analyzers assume execution continues after
 * "if (!p) luaL_error(...);" and report null dereferences that cannot happen.
 */
#pragma once

#include <lua.hpp>

// The redeclarations only add the attribute.
// NOLINTBEGIN(readability-redundant-declaration)
extern "C" {
LUA_API int(lua_error)(lua_State *L) __attribute__((noreturn));
LUALIB_API int(luaL_error)(lua_State *L, const char *fmt, ...) __attribute__((noreturn));
LUALIB_API int(luaL_argerror)(lua_State *L, int numarg, const char *extramsg)
    __attribute__((noreturn));
LUALIB_API int(luaL_typerror)(lua_State *L, int narg, const char *tname)
    __attribute__((noreturn));
}
// NOLINTEND(readability-redundant-declaration)
