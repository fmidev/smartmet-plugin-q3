/*
* PLUGIN.CPP                      Copyright (c) 2008-2010, Ilmatieteen laitos
*
* SmartMet Server plugin interface to Q3 server
*
* 10-Jan-2014 PKi: SmartMet Server API now uses HTTP classes
*/
#include "Server.hpp"

/*
* For 'SP_HttpRequest' and 'SP_HttpResponse' documentation, see:
*
*  <http://code.google.com/p/spserver/>
*  <http://spserver.googlecode.com/svn/trunk/spserver/sphttpmsg.hpp>
*
* As of now, SPServer has no actual documentation. --AKa 28-Nov-2007
*/
#include <smartmet/spine/SmartMetPlugin.h>
#include <smartmet/spine/Reactor.h>
#include <smartmet/spine/HTTP.h>
#include <smartmet/spine/SmartMet.h>

/*
..8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...
    SP_HttpRequest : SP_HttpMessage

    SP_HttpMessage::void setVersion( const char * version );
    SP_HttpMessage::const char * getVersion() const;
    SP_HttpMessage::void appendContent( const void * content, int length = 0, int maxLength = 0 );
    SP_HttpMessage::void setContent( const void * content, int length = 0 );
    SP_HttpMessage::const void * getContent() const;
    SP_HttpMessage::int getContentLength() const;
    SP_HttpMessage::void addHeader( const char * name, const char * value );
    SP_HttpMessage::int removeHeader( const char * name );
    SP_HttpMessage::int getHeaderCount() const;
    SP_HttpMessage::const char * getHeaderName( int index ) const;
    SP_HttpMessage::const char * getHeaderValue( int index ) const;
    SP_HttpMessage::const char * getHeaderValue( const char * name ) const;
    SP_HttpMessage::int isKeepAlive() const;

    SP_HttpRequest::void setMethod( const char * method );
    SP_HttpRequest::const char * getMethod() const;
    SP_HttpRequest::void setURI( const char * uri );
    SP_HttpRequest::const char * getURI() const;
    SP_HttpRequest::void setClientIP( const char * clientIP );
    SP_HttpRequest::const char * getClientIP() const;
    SP_HttpRequest::void addParam( const char * name, const char * value );
    SP_HttpRequest::int removeParam( const char * name );
    SP_HttpRequest::int getParamCount() const;
    SP_HttpRequest::const char * getParamName( int index ) const;
    SP_HttpRequest::const char * getParamValue( int index ) const;
    SP_HttpRequest::const char * getParamValue( const char * name ) const;

..8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...
    resp.setStatusCode(200);
    resp.setReasonPhrase("OK");
    //resp.addHeader("Content-type","text/plain");
    resp.setContent( str.c_str(), str.size() );
..8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...8<...
*/

#include <iostream>
// cerr

using namespace std;

/*---=== Smartmet Server ===---
*
* Smartmet Server interfacing is done by a 'SmartMetPlugin'-derived
* object.
*/
class Plugin : public Q3Server, public SmartMetPlugin
{
 public:
  Plugin(SmartMet::Spine::Reactor *, const char *);
  void init();
  void shutdown();
  ~Plugin();

  const string &getPluginName() const
  {
    static const string name("Q3");
    return name;
  }
  int getRequiredAPIVersion() const { return SMARTMET_API_VERSION; }
  void runScheduledTasks() { LOG_DEBUG0("runScheduledTasks() ignored"); }
  void runDataUpdateTasks() { LOG_DEBUG0("runDataUpdateTasks() ignored"); }
  // 21-Dec-2012 PKi: Dummy request handler (using the old one)
  //
  void requestHandler(SmartMet::Spine::Reactor &theReactor,
                      const SmartMet::Spine::HTTP::Request &theRequest,
                      SmartMet::Spine::HTTP::Response &theResponse)
  {
    LOG_DEBUG0("requestHandler() ignored");
  }

 private:
};

/*
* Object to tie SmartMet Server request/response
*/
class BS_RequestResponse : public RequestResponse
{
 public:
  BS_RequestResponse(const SmartMet::Spine::HTTP::Request &req_,
                     SmartMet::Spine::HTTP::Response &resp_);

  /*virtual*/ void set_mime(const char *mime) { resp.setHeader("Content-type", mime); }
  /*virtual*/ void set_header(const char *header, const char *value)
  {
    resp.setHeader(header, value);
  }

  /*virtual*/ void set_code(unsigned rc) { resp.setStatus(rc); }
  /*virtual*/ void append(const char *s, int bytes) { resp.appendContent(string(s, bytes)); }
  /*virtual*/ string get_client_ip() const { return req.getClientIP(); }
  /*virtual*/ string get_query_string() const { return req.getQueryString(); }
 private:
  const SmartMet::Spine::HTTP::Request &req;  // valid throughout the lifespan
  SmartMet::Spine::HTTP::Response &resp;      // -''-
};

BS_RequestResponse::BS_RequestResponse(const SmartMet::Spine::HTTP::Request &req_,
                                       SmartMet::Spine::HTTP::Response &resp_)
    : req(req_), resp(resp_)
{
  SmartMet::Spine::HTTP::ParamMap pmap = req.getParameterMap();

  for (auto it_p = pmap.begin(); it_p != pmap.end(); it_p++)
    set_key_val(it_p->first, it_p->second);
}

/*
* We need a process-specific pointer because the handlers are not given
* the plugin pointer otherwise.
*/
static Plugin *me;  // = 0;

static void native_handler(SmartMet::Spine::Reactor &theReactor,
                           const SmartMet::Spine::HTTP::Request &req,
                           SmartMet::Spine::HTTP::Response &resp)
{
  BS_RequestResponse proxy(req, resp);
  me->native_handler(proxy);
}

#ifdef ENABLE_OGC_WMS
static void wms_handler(SmartMet::Spine::Reactor &theReactor,
                        const SmartMet::Spine::HTTP::Request &req,
                        SmartMet::Spine::HTTP::Response &resp)
{
  BS_RequestResponse proxy(req, resp);
  me->wms_handler(proxy);
}
#endif

/*
* Plugin constructor
*
* This is called once at the startup of the SmartMet Server.
*/
Plugin::Plugin(SmartMet::Spine::Reactor *server, const char *cfg_file)
    : Q3Server(cfg_file), SmartMetPlugin()
{
  namespace p = boost::placeholders;

  assert(server);
  if (server->getRequiredAPIVersion() != SMARTMET_API_VERSION)
  {
    // Better to use 'stderr' for this than the logging system. Should not occur.
    //
    cerr << "*** SmartMet Server API version mismatch ***" << endl;
    return;
  }

  // Queries will start coming in after this.
  //
  server->addContentHandler(this, "/q3", boost::bind(&::native_handler, p::_1, p::_2, p::_3));

  // Note: In order to serve also the 'http://.../q3/' URL (without any params)
  //      we need this.
  //
  server->addContentHandler(this, "/q3/", boost::bind(&::native_handler, p::_1, p::_2, p::_3));

// Do we provide an OGC compatibility front-end?
//
#ifdef ENABLE_OGC_WMS
  server->addContentHandler(this, "/q3_wms", boost::bind(&::wms_handler, p::_1, p::_2, p::_3));
#endif

  // This DOES go to q3 log. Keep it around to see when restarts have happened.
  //
  LOG_MAINTENANCE0("initialization done");
}

void Plugin::init()
{
}

void Plugin::shutdown()
{
}

/*
*/
Plugin::~Plugin()
{
  LOG_DEBUG0("Plugin destructor");  // never gets here (see 'destroy()')
}

// ======================================================================

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 *
 * NOTE: We need to set 'me' to point to the created plugin, since
 *       'addContentHandler()' does not provide that info to handlers.
 */
extern "C" SmartMetPlugin *create(SmartMet::Spine::Reactor *them, const char *config)
{
  // This goes to 'stderr' (we haven't redirected logging, yet)
  //
  LOG_MAINTENANCE0("Starting up.");

  assert(me == 0);
  return me = new Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin *us)
{
  // Seems we don't ever get here (at least the way 'systemctl stop smartmetd' kills us)
  //
  LOG_MAINTENANCE0("Going down.");

  assert(me == us);

  // This will call '~Plugin()' since the destructor is virtual
  delete us;
  me = 0;
}
