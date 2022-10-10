/*
 * PROTO.H                       Copyright (c) 2010, Ilmatieteen laitos
 *
 * Checking Lua function call parameters.
 */
#ifndef PROTO_H
#define PROTO_H

#include <lua.hpp>

/*
 * ProtoProxy
 *
 * Invisible class useful for getting:
 *
 *   proto_init(L).set( str, func ).set( ... )
 */
class ProtoProxy {
public:
  ProtoProxy(lua_State *L_) : L(L_) {}
  ProtoProxy &set(const char *id, lua_CFunction func);

private:
  lua_State *L;
};

ProtoProxy proto_init(lua_State *L);

void proto(lua_State *L, const char *s);

#endif
// PROTO_H
