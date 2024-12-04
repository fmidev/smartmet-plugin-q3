/*
 * SESSION.H                   Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Revised: 11-Nov-2010
 */
#ifndef SESSION_H
#define SESSION_H

#include <map>
#include <vector>
#include <string>
#include <ostream>

#include "Tools.h"  // string_or_null

extern "C" {
    #include "luajit-2.1/lua.h"
}

class Session {
  public:    
    static int init( lua_State *L );
    static string_or_null result_( lua_State *L, std::ostream &os, unsigned i );
};

#endif
    // SESSION_H
