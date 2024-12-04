/*
 * LOGTOOLS.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Logging from Metqu/Q3, into multiple log backbones.
 *
 * Note: Expects to be compiled in C99 mode, __VA_ARGS__ supported
 */
#ifndef LOGTOOLS_H
#define LOGTOOLS_H

#include <stdio.h>
#include <time.h>
#include <cassert>

#include <sstream>
#include <stdexcept>
#include <string>

class MatrixPos;
class MatrixSize;
class NA_Level;
class NA_Param;
class JDay;

extern "C"
{
#include "luajit-2.1/lua.h"
}

#include <stdint.h>
// 'uint64_t' (Ubuntu 9.10)

/*
 * 'sprintf' kind formatting, but creates a C++ string.
 */
std::string string_fmt(const char *fmt, ...);

/*
 * Note on logging levels:
 *
 * Standard logging systems s.a. 'log4j' and 'syslog' provide the following categories:
 *
 *   FATAL   Severe errors that cause premature termination. Expect these to be immediately visible
 * on a status console. ERROR   Other runtime errors or unexpected conditions. Expect these to be
 * immediately visible on a status console. WARN    Use of deprecated APIs, poor use of API,
 * 'almost' errors, other runtime situations that are undesirable or unexpected, but not necessarily
 * "wrong". Expect these to be immediately visible on a status console. INFO    Interesting runtime
 * events (startup/shutdown). Expect these to be immediately visible on a console, so be
 * conservative and keep to a minimum. DEBUG   Detailed information on the flow through the system.
 * Expect these to be written to logs only.
 *
 * We have some other categories (BUG, TIMING, USAGE, LUA, STAT) that can be mapped on the
 * 'standard' categories when needed.
 */

/*
 * Logger prototype that does NO logging.
 */
class Logger
{
  // Static methods
  //
 public:
  static void init(Logger *logger_);
  virtual ~Logger() {}

  // A unique id for groupping separate log messages (i.e. all that have to do with particular
  // script) together.
  //
  // This is unique within the one server (servers can be differentiated otherwise). If we need
  // global uniqueness, things like 'libuuid' can be used (giving 128-bit values).
  //
  // Current implementation simply gives the current time (in ms) and checks that the same time is
  // not given twice.
  //
  typedef uint64_t unique_t;  // defined but not used (always 0) on METQU
#ifndef METQU
  static unique_t gen_unique();
#endif

  enum category
  {
    _INFO = 1,  // was: _OK but syslog standard categories have the name INFO
    _WARNING,
    _ERROR,
    _FATAL,
    _DEBUG,
    //
    // Following categories are our own (not in syslog)
    _BUG,     // assert failed -like situation
    _USAGE,   // bad input
    _TIMING,  // performance reports
    _LUA,     // from Lua script 'LOG(...)'
    _STAT,    // server condition statistics
    //
    _MAINTENANCE  // notices to maintainers (i.e. system starting, system start finished);
                  // these are passed through ANOTHER conf entry than the other logs, allowing
                  // them i.e. to be delivered using email forwarding via syslog
  };
  static void LOG(enum category cat,
                  const char *file,
                  unsigned line,
                  const unique_t id_,
                  const std::string &msg)
  {
    // No filename causes no logging (used by 'E_NOLOG_xxx')
    //
    if (logger && file)
    {
      logger->log(cat, file, line, id_, msg.c_str(), msg.length());
    }
  };
  static int LOG_(lua_State *L);  // logging from Lua

  // Derived classes implement their actual logger
  //
 protected:
  virtual void log(enum category cat,
                   const char *file,
                   unsigned line,
                   const unique_t id_,
                   const char *msg,
                   size_t msg_len) = 0;

 public:
  static void DEBUG(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_DEBUG, fn, line, id, msg);
  }
  static void OK(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_INFO, fn, line, id, msg);
  }
  static void WARNING(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_WARNING, fn, line, id, msg);
  }
  static void ERROR(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_ERROR, fn, line, id, msg);
  }
  static void FATAL(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_FATAL, fn, line, id, msg);
  }
  static void BUG(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_BUG, fn, line, id, msg);
  }
  static void USAGE(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_USAGE, fn, line, id, msg);
  }
  static void TIMING(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_TIMING, fn, line, id, msg);
  }
  static void STAT(const char *fn, unsigned line, const unique_t id, const std::string &msg)
  {
    LOG(_STAT, fn, line, id, msg);
  }

  static void MAINTENANCE(const std::string &msg) { LOG(_MAINTENANCE, nullptr, 0, 0, msg); }

 private:
  static Logger *logger;  // current logger (nullptr: none; set only once)
};

#define LOG_DEBUG(fmt, ...)
#define LOG_OK(fmt, ...)
#define LOG_STAT(fmt, ...)
#define LOG_TIMING(fmt, ...)
#define LOG_WARNING(fmt, ...) Logger::WARNING(__FILE__, __LINE__, 0, string_fmt(fmt, __VA_ARGS__))
#define LOG_ERROR(fmt, ...) Logger::ERROR(__FILE__, __LINE__, 0, string_fmt(fmt, __VA_ARGS__))
#define LOG_FATAL(fmt, ...) Logger::FATAL(__FILE__, __LINE__, 0, string_fmt(fmt, __VA_ARGS__))
#define LOG_BUG(fmt, ...) Logger::BUG(__FILE__, __LINE__, 0, string_fmt(fmt, __VA_ARGS__))
#define LOG_USAGE(fmt, ...) Logger::USAGE(__FILE__, __LINE__, 0, string_fmt(fmt, __VA_ARGS__))

#define LOG_MAINTENANCE(fmt, ...) Logger::MAINTENANCE(string_fmt(fmt, __VA_ARGS__))

// Compiling has problem with empty __VA_ARGS__
//
#define LOG_DEBUG0(s) LOG_DEBUG("%s", s)
#define LOG_OK0(s) LOG_OK("%s", s)
#define LOG_WARNING0(s) LOG_WARNING("%s", s)
#define LOG_ERROR0(s) LOG_ERROR("%s", s)
#define LOG_FATAL0(s) LOG_FATAL("%s", s)
#define LOG_BUG0(s) LOG_BUG("%s", s)
#define LOG_USAGE0(s) LOG_USAGE("%s", s)
// # define LOG_TIMING0( s )  LOG_TIMING( "%s", s )
// # define LOG_STAT0( s )    LOG_STAT( "%s", s )
#define LOG_MAINTENANCE0(s) LOG_MAINTENANCE("%s", s)

/*---=== Stderr logging ===---
 */
class StderrLogger : public Logger
{
 public:
  StderrLogger() : Logger() {}
  /*virtual*/ void log(category cat,
                       const char *file,
                       unsigned line,
                       const unique_t id,
                       const char *msg,
                       size_t msg_len);

 private:
};

/*---=== General exceptions ===---
 *
 * These are tightly integrated to the logging system, so we get automatic logs i.e. of
 * thrown 'E_BUG' and alike.
 *
 * Note: 'E_ANY' (and derived) classes need to be copyable, for 'throw E_...' to work.
 *       Seems the throw _itself_ already makes a copy. There may be multiple such in
 *       the chain, so keep E_ANY contents narrow. The only way around this (imho) is
 *       to use 'throw new E_...' instead (which has other obvious downsides).
 *       --AKa 4-Sep-2009
 */
class E_ANY : public std::exception
{
 public:
  E_ANY(Logger::category cat,
        const char *f,
        unsigned l,
        const std::string &m,
        const Logger::unique_t id = 0)
      : std::exception(), file(f), line(l), msg(m), buf()
  {
    if (cat) Logger::LOG(cat, f, l, id, m);
  }

  E_ANY(const char *f, unsigned l, const std::string &m)
      : std::exception(), file(f), line(l), msg(m), buf()
  {
  }

  E_ANY(const std::string &m) : std::exception(), file(0), line(0), msg(m), buf() {}

  // general copy constructor is fine

  // Needed for 'std::exception' derivation (otherwise "looser throw specifier")
  //
  /*virtual*/ ~E_ANY() noexcept {}

  // Create the 'what()' message only if needed (= if the catcher asks for it)
  //
  // Note: We must keep it in a member, to provide lifespan.
  //
  /*virtual*/ const char *what() const noexcept
  {
    // Note: Using 'ostringstream' makes no limits to the length of the message.
    //       'string_fmt()' would have an internal limit (and truncate longer stuff).
    //
    if (!file)
    {
      return what_nosource();
    }
    else
    {
      // Prepare for multiple calls along the cascade (craft 'buf' only once)
      if (buf == "")
      {
        std::ostringstream o;
        o << file << ":" << line << ": " << msg;
        buf = o.str();
      }
      return buf.c_str();
    }
  }

  // Only the message (no file & line)
  //
  const char *what_nosource() const noexcept { return msg.c_str(); }

 protected:
  static std::string to_str(const MatrixSize &sz);
  static std::string to_str(const MatrixPos &pos);
  static std::string to_str(const NA_Level &lev);
  static std::string to_str(const JDay &time);
  static std::string to_str(const NA_Param &p);

 private:
  const char *file;  // string constant so we can keep the pointer
  unsigned line;

  const std::string msg;    // message without file and line
  mutable std::string buf;  // message with file and line (only if needed - see 'what())
};

class E_OUT_OF_MEMORY : public E_ANY
{
 public:
  E_OUT_OF_MEMORY(const char *f, unsigned l) : E_ANY(Logger::_FATAL, f, l, "Out of memory") {}
};
#define E_LOG_OUT_OF_MEMORY() E_OUT_OF_MEMORY(__FILE__, __LINE__)

class E_BUG : public E_ANY
{
 public:
  E_BUG(const char *f, unsigned l, const std::string &m) : E_ANY(Logger::_BUG, f, l, m) {}
};
#define E_LOG_BUG(fmt, ...) E_BUG(__FILE__, __LINE__, string_fmt(fmt, __VA_ARGS__))
#define E_LOG_BUG0(s) E_BUG(__FILE__, __LINE__, s)

class E_FATAL : public E_ANY
{
 public:
  E_FATAL(const char *f, unsigned l, const std::string &m) : E_ANY(Logger::_FATAL, f, l, m) {}
};
#define E_LOG_FATAL(fmt, ...) E_FATAL(__FILE__, __LINE__, string_fmt(fmt, __VA_ARGS__))
#define E_LOG_FATAL0(s) E_FATAL(__FILE__, __LINE__, s)

class E_ERROR : public E_ANY
{
 public:
  E_ERROR(const char *f, unsigned l, const std::string &m) : E_ANY(Logger::_ERROR, f, l, m) {}
};
#define E_LOG_ERROR(fmt, ...) E_ERROR(__FILE__, __LINE__, string_fmt(fmt, __VA_ARGS__))
#define E_LOG_ERROR0(s) E_ERROR(__FILE__, __LINE__, s)

class E_UNEXPECTED_DATA : public E_ANY
{
 public:
  E_UNEXPECTED_DATA(const char *f, unsigned l, const std::string &m)
      : E_ANY(Logger::_WARNING, f, l, std::string("Unexpected data: ") + m)
  {
  }
};
#define E_LOG_UNEXPECTED_DATA(fmt, ...) \
  E_UNEXPECTED_DATA(__FILE__, __LINE__, string_fmt(fmt, __VA_ARGS__))
#define E_LOG_UNEXPECTED_DATA0(s) E_UNEXPECTED_DATA(__FILE__, __LINE__, s)

class E_BAD_FILE : public E_ANY
{
 public:
  E_BAD_FILE(const char *f, unsigned l, const std::string &fn, const std::string &what_)
      : E_ANY(Logger::_WARNING,
              f,
              l,
              string_fmt("Unable to read (%s): %s", what_.c_str(), fn.c_str()))
  {
  }
  E_BAD_FILE(const std::string &fn, const std::string &what_)
      : E_ANY(string_fmt("Unable to read (%s): %s", what_.c_str(), fn.c_str()))
  {
  }
};
#define E_LOG_BAD_FILE(fn, what) E_BAD_FILE(__FILE__, __LINE__, fn, what)

class E_USAGE : public E_ANY
{
 public:
  E_USAGE(const char *f, unsigned l, const std::string &m) : E_ANY(Logger::_USAGE, f, l, m) {}
};
#define E_LOG_USAGE(fmt, ...) E_USAGE(__FILE__, __LINE__, string_fmt(fmt, __VA_ARGS__))
#define E_LOG_USAGE0(s) E_USAGE(__FILE__, __LINE__, s)

#define E_NOLOG_USAGE(fmt, ...) E_USAGE(nullptr, 0, string_fmt(fmt, __VA_ARGS__))

/** (not used)
// E_SECURITY is for cases where behaviour seems to be a hack attempt;
// these are like 'E_USAGE' but which should not happen by accident.
//
class E_SECURITY : public E_ANY {
  public:
    E_SECURITY( const char *f, unsigned l, const std::string& m )
        : E_ANY( Logger::_WARNING, f,l, "Security concern: "+m ) {}
};
#define E_LOG_SECURITY( fmt, ... ) E_SECURITY( __FILE__, __LINE__, string_fmt(fmt,__VA_ARGS__) )
#define E_LOG_SECURITY0( s )       E_SECURITY( __FILE__, __LINE__, s )
**/

// ... non-logging exceptions ...
//
class E_BAD_SIZE : public E_ANY
{
 public:
  E_BAD_SIZE(const MatrixPos &a, const MatrixPos &b, const char *describe = 0)
      : E_ANY(string_fmt("Differing grid sizes %s: %s != %s",
                         describe ? describe : "",
                         to_str(a).c_str(),
                         to_str(b).c_str()))
  {
  }
};

class E_READONLY : public E_ANY
{
 public:
  E_READONLY() : E_ANY("Trying to write to a read only object") {}
};

class E_NO_MATCH : public E_ANY
{
 public:
  E_NO_MATCH(const JDay &vt) : E_ANY(string_fmt("Time not in data: %s", to_str(vt).c_str())) {}
  E_NO_MATCH(const NA_Level &lev) : E_ANY(string_fmt("Level not in data: %s", to_str(lev).c_str()))
  {
  }
  E_NO_MATCH(const NA_Param &p) : E_ANY(string_fmt("Param not in data: %s", to_str(p).c_str())) {}
  E_NO_MATCH()
      : E_ANY("Param not in data") {} /* used by 'SQD_Matrix' - does not need param identifiers */
};

class E_OUTSIDE : public E_ANY
{
 public:
  E_OUTSIDE() : E_ANY("Outside of matrix") {}
  E_OUTSIDE(const MatrixSize &m_size, const MatrixPos &mi)
      : E_ANY(string_fmt("Outside of matrix: %s %s", to_str(mi).c_str(), to_str(m_size).c_str()))
  {
  }
};

#endif
// LOGTOOLS_H
