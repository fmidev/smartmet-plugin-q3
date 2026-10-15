/*
 * SYSLOGLOGGER.H                       Copyright (c) 2010, Ilmatieteen laitos
 *
 * Logging to syslog backbone ('man 3 syslog').
 */
#ifndef SYSLOG_LOGGER_H
#define SYSLOG_LOGGER_H

#include "LogTools.h"

/*
 */
class SyslogLogger : public Logger {
public:
  SyslogLogger(const char *facility);
  /*virtual*/ void log(category cat, const char *file, unsigned line,
                       Logger::unique_t /*id_*/, const char *msg,
                       size_t msg_len) override;

private:
};

#endif
// SYSLOG_LOGGER_H
