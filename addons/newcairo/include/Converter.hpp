/*
* CONVERTER.HPP                   Copyright (c) 2010, Ilmatieteen laitos
*
* Converter between Cairo enums and Lua strings (and back).
*/
#ifndef CONVERTER_HPP
#define CONVERTER_HPP

#include <sstream>
#include <stdexcept>
#include <map>

#include <string.h>

#include <iostream>

extern "C" {
# include <lua.h>
}

/*---=== Converter ===---
*
* Convert between Cairo enum T and string constants (actually a two-way map).
*
* Note: We're trying to keep this as narrow memory-wise as possible; using
*       just one map and string literals in it (not C++ strings, which would
*       need to be copied; not two maps).
*
* Note: Comparisons are case insensitive and underlines are ignored.
*/
template<typename T>
class Converter {
  protected:
    Converter(const char *name_) : m(), name(name_) {}

    void map(T e, const char *s) {
        m[(int)e]= s;    // Note: original string must be a literal (not copied)
    }

  public:
    const char * operator() (T e) const {
        std::map<int,const char *>::const_iterator it= m.find(e);
        if (it != m.end()) {
            return it->second;  // the string
        }
        std::ostringstream os;
        os << "Internal error: unexpected enum " << (int)e;
        throw std::runtime_error( os.str().c_str() );
    }

    /*
    * string -> enum (just try (never error)
    */
    bool just_try( lua_State *L, int index, T &to ) const {
        const char *s= lua_tostring(L,index);
        if (s) {
            for( std::map<int,const char *>::const_iterator it= m.begin();
                it != m.end();
                ++it ) {
//std::cerr << s << it->second << std::endl;
                if (strcasecmp(s,it->second)==0) {
                    to= (T) it->first;
                    return true;
                }
            }
        }
        return false;
    }

    /*
    * string -> enum (with default value)
    */
    T operator() (lua_State *L, int index, const T &def, bool must_have=false) const {
        T ret;
        if (just_try(L,index,ret)) {
            return ret;
        }
        // Catch bad values ('nil' returns default)
        //
        if (must_have || (!(lua_isnil(L,index) || lua_isnone(L,index)))) {
            luaL_error( L, "Not %s: %s", name.c_str(), L_string_or_typename(index) );
        }
        return def;
    }

    /*
    * string -> enum (must have value)
    */
    T operator() (lua_State *L, int index) const {
        return operator() (L, index, (T)(-1), true);
    }

  private:
    std::map<int,const char *> m;     // T -> const char *
    const std::string name;           // i.e. "antialias" (for error messages)
};


/*---=== MethodNames ===---
*
* Mapping method names to 'lua_CFunction' function pointers (one way).
*/
class MethodNames {
  protected:
    MethodNames() : m() {}
    
    /*
    * 'grant' has bits set for subtypes _allowed_ to use the function
    */
    void map( const char *s, lua_CFunction f, unsigned grant= (unsigned)(-1) ) {
        m[s]= f;
        bans[f]= ~grant;
    }

  public:    
    /*
    * 'mask' has one bit set, describing the particular type (or 0 for include all)
    */
    lua_CFunction operator() (const char *s, unsigned mask= 0 ) const {
        std::map<std::string,lua_CFunction>::const_iterator it= m.find(s);
        if (it != m.end()) {
            lua_CFunction f= it->second;
            if (mask) {
                std::map<lua_CFunction, unsigned>::const_iterator it2= bans.find(f);
                if (it2 != bans.end()) {
                    unsigned ban_mask= it2->second;

                    // Is this function banned?
                    //
                    if ((mask & ban_mask)!=0) {
                        return nullptr;    // hidden for us
                    }
                }
            }
            return f;
        }
        return nullptr;    // no function by that name
    }

  private:
    std::map<std::string, lua_CFunction> m;
    std::map<lua_CFunction, unsigned> bans;
};


#endif
    // CONVERTER_HPP
