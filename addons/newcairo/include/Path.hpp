/*
* PATH.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*/
#ifndef PATH_HPP
#define PATH_HPP

#include "LuaNew.h"
#include "Invariant.h"

#include "Common.h"

#include <cairo.h>


/*---=== Path ===---
*/
class Path;

struct Path_bind {
  public:
    static LuaNew_ID ID;     // the unique key
    static void setup( lua_State *L );
    static const char *name() { return "Cairo path"; }
    static const char *env_mode() { return NULL; }
    static const LuaNew_ID & id() { return ID; }
    typedef Path CAST_T;

  private:
    static int index( lua_State *L );
};

class Path : public LuaNew<Path_bind> {
  public:
    operator cairo_path_t *() { return cp; }
    cairo_status_t status() const { return cp->status; }

    Path( cairo_path_t *cp_ );
	~Path();

    //static int set_matrix( lua_State *L );

  private:
    // data members
    //
    cairo_path_t *cp;

#ifndef NDEBUG
    void _INVARIANT( const char *, unsigned ) const {
        assert_invariant( cp != 0 );
    }
#endif 
};

#endif
    // PATH_HPP
    
