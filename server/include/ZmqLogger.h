/*
* ZMQLOGGER.H                       Copyright (c) 2010, Ilmatieteen laitos
*
* Logging via ZeroMQ to remote log server.
*/
#ifndef ZMQ_LOGGER_H
#define ZMQ_LOGGER_H

#include "LogTools.h"

/*
*/
class ZmqLogger : public Logger
{
 public:
  ZmqLogger(const char *addr);
  /*virtual*/ void log(category cat,
                       const char *file,
                       unsigned line,
                       const Logger::unique_t,
                       const char *msg,
                       size_t msg_len);

 private:
  // Implementation hidden to allow plugins to include us, and to find in runtime
  // whether ZMQ feature was enabled or not (we'll throw an exception if not)
  //
  struct impl;
  impl *opaque;
  // zmq::context_t ctx;
  // zmq::socket_t socket;
};

#endif
// ZMQ_LOGGER_H
