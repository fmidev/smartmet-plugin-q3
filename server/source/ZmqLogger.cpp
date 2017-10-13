/*
* ZMQLOGGER.CPP                    Copyright (c) 2010, Ilmatieteen laitos
*
* We forward the logs via ZeroMQ to a log server.
*
* Ref: <http://www.zeromq.org/area:docs-v20>
*      <http://api.zeromq.org/zmq.7.html>
*/
#include "ZmqLogger.h"

#include <string>
#include <stdexcept>

using namespace std;

// USE_ZMQ is enabled in engine, to get the feature. Even if it's not, we'll
// give a sceleton to throw an error.
//
#ifdef USE_ZMQ
#include <zmq_mod.hpp>
#endif

struct ZmqLogger::impl
{
#ifdef USE_ZMQ
  zmq::context ctx;
  zmq::socket_pub socket;
  impl() : ctx(1, 1), socket(ctx) {}
#endif
};

/*
* 'addr':   "x.x.x.x:port"
*/
ZmqLogger::ZmqLogger(const char *addr) : opaque(new impl())
{
#ifndef USE_ZMQ
  throw E_LOG_FATAL0("Zmq logging not enabled");
#else
  // Prepend the "tcp://" protocol name
  //
  string s = string_fmt("tcp://%s", addr);

  opaque->socket.connect(s.c_str());
#endif
}

/*
*/
void ZmqLogger::log(enum category cat,
                    const char *file,
                    unsigned line,
                    const Logger::unique_t id,
                    const char *s,
                    size_t s_len)
{
#ifdef USE_ZMQ
  if (!msg)
    return;  // just in case

  const char *cs = NULL;  // if we get some unexpected category (should not)
  switch (cat)
  {
    case _FATAL:
      cs = "FATAL";
      break;
    case _ERROR:
      cs = "ERROR";
      break;  // "error conditions"
    case _WARNING:
      cs = "WARNING";
      break;
    case _INFO:
      cs = "INFO";
      break;
    case _DEBUG:
      cs = "DEBUG";
      break;

    // These are not part of the traditional categories (syslog)
    //
    case _BUG:
      cs = "BUG";
      break;
    case _USAGE:
      cs = "USAGE";
      break;
    case _TIMING:
      cs = "TIMING";
      break;
    case _LUA:
      cs = "LUA";
      break;
    case _STAT:
      cs = "STAT";
      break;
    case _MAINTENANCE:
      cs = "MAINTENANCE";
      break;
  }

  // Not passing the source location
  (void)file;
  (void)line;

  // NOTE: Once this code (the log format) has stabilized, we can gain some extra speed
  //      by writing directly to the ZMQ buffer.

  // NOTE: DO NOT use 'string_fmt' with 's' - the message may be long (i.e. if coming from a script
  // DUMP)
  //      and 'string_fmt()' might truncate it.

  // We know the size we're about to give - write directly to Zmq message.
  //
  string tmp = string_fmt(" [%lld]");

  size_t len = strlen(cs) + 1 + s_len + tmp.length();  // "<cs> <s> [<id>]"
  zmq::message msg(len + 1);                           // terminating zero

  char *p = msg.data();
  strcpy(p, cs);
  strcat(p, " ");
  strcat(p, s);
  strcat(p, tmp.c_str());

  opaque->socket.send(msg);
#endif
}
