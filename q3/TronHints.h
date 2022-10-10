/*
 * TRONHINTS.H                            Copyright (c) 2012, Ilmatieteen laitos
 */
#ifndef TRONHINTS_H
#define TRONHINTS_H

#include "LuaNew.h"

class TronHints;

struct TronHintsBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "TronHints"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef TronHints CAST_T;

private:
};

#endif // TRONHINTS_H
