/*
 * LUAWRAP.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
 */
#ifndef LUAWRAP_H
#define LUAWRAP_H

#include "LuaNew.h"
#include "Tools.h"
// assert_invariant

// A wrapper for initializing a Lua state, and cleaning it up if the C++ code
// throws an exception or program code does a return.
//
struct LuaWrap {
public:
  LuaWrap(const luaL_Reg *libs, const char *chunk, size_t chunk_size);
  ~LuaWrap();
  operator lua_State *() { return L; }

private:
  lua_State *L;

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
    assert_invariant(L);
  }
#endif
};

#endif
// LUAWRAP_H
