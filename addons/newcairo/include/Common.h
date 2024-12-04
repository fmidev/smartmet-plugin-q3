/*
* COMMON.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*
* Definitions common to all subparts of 'NewCairo' binding.
*/
#ifndef NEWCAIRO_COMMON_HPP
#define NEWCAIRO_COMMON_HPP

#include <stdexcept>

#include <cairo.h>

extern "C" {
# include <luajit-2.1/lua.h>
}

#ifndef NDEBUG
# define assert_invariant assert
#endif

class out_of_memory : public std::exception {
    /*virtual*/ const char *what() const noexcept { return "out of memory"; }
};

inline void status_check( lua_State *L, cairo_status_t st ) {
    if (st) { 
        luaL_error( L, "%s", cairo_status_to_string(st) );
    }
}

/*
* This function is assert-like, called when the status is EXPECTED to be zero.
*/
inline void status_check_ok( lua_State *L, cairo_status_t st ) {
    if (st) {
        luaL_error( L, "%s", cairo_status_to_string(st) );
    }
}

#endif
    // NEWCAIRO_COMMON_HPP
