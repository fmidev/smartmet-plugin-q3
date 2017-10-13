/*
* REQUESTRESPONSE.HPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Make different server back-ends (Mongoose [removed 02/2017], SmartMet Server) seem uniform.
*/
#ifndef Q3_REQUESTRESPONSE_HPP
#define Q3_REQUESTRESPONSE_HPP

#include "Tools.h"

#include <map>

#define HTTPHdr_First_Origintime "XFMI-OriginTime"

/*
* Interface for providing HTTP params to 'Q3Server' without copying.
* Also takes care of JSONP padding part (if there is a 'callback=...' parameter).
*
* Also provides a Lua reader function for the 'code' parameter code block (taking
* care of %20 etc. conversion).
*/
class RequestResponse {
  private:
    std::map<std::string, std::string> kk;      // param/value mapping
    string_or_null jsonp_callback;
    string_or_null code;

  protected:
    void set_key_val( const std::string &key, const std::string &val );
    virtual void set_mime( const char *mime ) = 0;
    virtual void set_header( const char *header, const char *value ) = 0;
    virtual void set_code( unsigned code ) = 0;
    virtual void append( const char *s, int bytes ) = 0;

  public:
    virtual std::string get_client_ip() const = 0;
    virtual std::string get_query_string() const = 0;

    const char *get_code() const { return code.c_str(); }       // for debugging only
    bool get_jsonp_mode() const { return jsonp_callback != NULL; }

    void set_script( const char *s );

    void set_output( unsigned resp_code, const char *mime, const char *s, size_t bytes=(size_t)(-1) );
    void set_first_origintime( const char * fstot ) { set_header(HTTPHdr_First_Origintime,fstot); };

    std::map<std::string, std::string>::const_iterator begin() const { return kk.begin(); }
    std::map<std::string, std::string>::const_iterator end() const { return kk.end(); }
    std::map<std::string, std::string>::const_iterator find( const std::string &k ) const { return kk.find(k); }

    bool has_code() const { return code != NULL; }
    int compile_code( lua_State *L, const char *block_name );

    virtual ~RequestResponse() {};
};

#endif
    // Q3_REQUESTRESPONSE_HPP
    
