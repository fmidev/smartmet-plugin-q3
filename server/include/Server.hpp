/*
* SERVER.HPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*
* Common server features
*/
#ifndef Q3_SERVER_HPP
#define Q3_SERVER_HPP

#include "Tools.h"
#include "RequestResponse.hpp"

extern "C" {
# include "lua.h"
# include "lauxlib.h"
}

#include <string>
#include <map>
#include <utility>

class Q3Engine;

/*
* Give OGC WMS services at '/q3_wms?...'
*/
#define ENABLE_OGC_WMS

/*
* Abstract base class for any server implementation (Brainstorm, Mongoose [removed], ...)
*/
class Q3Server {
  public:
	Q3Server( const char *conf );
	~Q3Server();

    void native_handler( RequestResponse & );
#ifdef ENABLE_OGC_WMS
    void wms_handler( RequestResponse & );
#endif

  private:
    int query( const std::map<std::string,std::string> &key_val,
               bool key_val_as_globals,
               RequestResponse &rr, 
               std::string &err,
               int decimals /*= -1*/
#ifdef CONFIG_BINARY_OUTPUT_ENABLED
               , bool binary_q2 /*= false*/
#endif
             );

    // Disallow copying
    //
    Q3Server( const Q3Server & );
    Q3Server& operator=( const Q3Server & );

    Q3Engine *itsEngine;
};

#endif
    // Q3_SERVER_HPP
    
