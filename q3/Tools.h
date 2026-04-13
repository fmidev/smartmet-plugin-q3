/*
 * TOOLS.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Misc tools for string processing, Lua helpers etc.
 */
#ifndef TOOLS_H
#define TOOLS_H

// Make sure 'posix_memalign()' gets defined
//
#ifndef _GNU_SOURCE
#warning "_GNU_SOURCE not defined; 'posix_memalign()' may not be available"
#endif

#include "Invariant.h"
#include "LogTools.h"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
// abort(), posix_memalign()

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
// keep this for Ubuntu (Centos seems to be giving 'string.h' from some system
// headers)

#include <lua.hpp>

#ifdef METQU
#define CONST_IF_SERVER /*not const*/
#else
#define CONST_IF_SERVER const
#endif

// Compile-time configurations
//
#include "Config.h"

#include <float.h>
const float INF_F = FLT_MAX; // = FLT_MAX (not quite infinity, but close by)

const char *const MIME_TEXT_UTF8 = "text/html; charset=UTF-8";

/*---=== 'find' to C++ vector iterators ===---*/

/*
 * Returns the (first) index of the asked value in a vector, or -1 if not there.
 *
 * Usage:
 *   int n= find_index<JDay>( times, vt );
 */
template <typename T>
int find_index(const std::vector<T> &vec, const T &value) {
  int index = 0;
  for (typename std::vector<T>::const_iterator it = vec.begin();
       it != vec.end(); ++it) {
    if (*it == value)
      return index;
    else
      ++index;
  }
  return -1; // not found
}

/*
 * Returns the iterator of the (first) matching value, or 'vec.end()' if not
 * there.
 *
 * Usage:
 *   vector<JDay>::const_iterator it= find_it<JDay>( times, vt );
 */
template <typename T>
typename std::vector<T>::const_iterator find_it(const std::vector<T> &vec,
                                                const T &value) {
  typename std::vector<T>::const_iterator it;
  for (it = vec.begin(); it != vec.end(); ++it) {
    if (*it == value)
      break;
  }
  return it;
}

/*---=== Misc ===---*/

/*
 * NOTE: With 'assert_invariant()' and failing 'pthread' calls, we DON'T want to
 * continue execution because the conditions are so severe. On the other hand,
 * we don't want to throw exception on these cases, because it would burden the
 * C++ prototype declarations unnecessarily. --AKa 4-Sep-2009
 */
#ifndef NDEBUG
#define assert_invariant(cond)                                                 \
  if (!(cond)) {                                                               \
    Logger::FATAL(file, line, 0, string_fmt("INVARIANT '%s' failed", #cond));  \
    abort();                                                                   \
  }
#endif

typedef unsigned int uint_t;

const char *pthread_rc2str(int rc);

#define PTHREAD_CALL(func, ...)                                                \
  {                                                                            \
    int rc = func(__VA_ARGS__);                                                \
    if (rc) {                                                                  \
      Logger::FATAL(__FILE__, __LINE__, 0,                                     \
                    string_fmt("%s failed: %s", #func, pthread_rc2str(rc)));   \
      abort();                                                                 \
    }                                                                          \
  }

class Mutex {
public:
  Mutex() : mm(new pthread_mutex_t()) {
    PTHREAD_CALL(pthread_mutex_init, mm, nullptr);
  }

  ~Mutex() { Destroy(); }

  void Destroy() {
    if (mm) {
      PTHREAD_CALL(pthread_mutex_destroy, mm);
      delete mm;
      mm = 0;
    }
  }

private:
  void Lock() {
    assert(mm);
    PTHREAD_CALL(pthread_mutex_lock, mm);
  }
  void Unlock() {
    assert(mm);
    PTHREAD_CALL(pthread_mutex_unlock, mm);
  }

  friend class ClaimMutex;

  pthread_mutex_t *mm; // pointer so we can have an explicit 'Destroy()'
};

/*
 * By making PThread locks C++ objects we gain automatic release i.e. if an
 * exception is thrown within the lifespan of a lock.
 */
class ClaimMutex {
public:
  ClaimMutex(Mutex &m_) : m(m_) { m.Lock(); }
  ~ClaimMutex() { m.Unlock(); }

private:
  Mutex &m;
};

/*---=== ... ===---*/

void Sleep_ms(unsigned ms);

uint64_t now_ms();

bool begins_with(const char *a, const char *b);
bool ends_with(const char *a, const char *b);

#ifdef UNIX
#include <glob.h>
const char *glob_fn(const char *fn_mask, glob_t &gbuf, int &gbuf_i,
                    bool is_dir = false);
#else
#error "Not implemented for Win32"
#endif

void remove_file(const char *fn);

// OS X (10.5.6, gcc 4.0.1) may have problems with 'isnan[f]()'. It does not
// seem to get defined in C++ (C presumably works fine)
//
#ifdef __APPLE__
#define isnanf(x) ((x) != (x))
#endif

#define INFINITY_F std::numeric_limits<float>::infinity()

/*---=== string_or_null ===---*/
/*
 * Like 'const std::string' but allows nullptr be used in initialization (and
 * returned in 'c_str()'). This class basically carries a copy of a string when
 * we cannot trust it to remain available on the Lua stack.
 */
class string_or_null : public std::string {
public:
  string_or_null() : std::string(""), is_null(true) {}
  string_or_null(const char *s) : std::string(s ? s : ""), is_null(s == 0) {}
  string_or_null(const std::string &s) : std::string(s), is_null(false) {}
  string_or_null(const string_or_null &o) : std::string(o), is_null(o.is_null) {}

  const char *c_str() const { return is_null ? 0 : std::string::c_str(); }

  string_or_null &operator=(const string_or_null &o) {
    if (this != &o) {
      std::string::operator=(o);
      is_null = o.is_null;
    }
    return *this;
  }
  string_or_null &operator=(const std::string &o) {
    std::string::operator=(o);
    is_null = false;
    return *this;
  }
  string_or_null &operator=(const char *s) {
    std::string::operator=(s ? s : "");
    is_null = (s == 0);
    return *this;
  }

  /*
   * Like 'std::string::compare()'
   *
   * nullptr is ordered less than any string (and unequal to "").
   */
  int compare(const string_or_null &b) const {
    if (is_null) {
      return b.is_null ? 0 : -1; // equal or 'this' is smaller
    } else if (b.is_null) {
      return 1; // 'this' is bigger
    } else {
      return std::string::compare(b); // both are non-nullptr
    }
  }

  bool operator==(const string_or_null &b) const { return compare(b) == 0; }
  bool operator!=(const string_or_null &b) const { return compare(b) != 0; }
  bool operator<(const string_or_null &b) const { return compare(b) < 0; }
  bool operator<=(const string_or_null &b) const { return compare(b) <= 0; }
  bool operator>(const string_or_null &b) const { return compare(b) > 0; }
  bool operator>=(const string_or_null &b) const { return compare(b) >= 0; }

  bool operator==(const char *b) const { return compare(b) == 0; }
  bool operator!=(const char *b) const { return compare(b) != 0; }

  bool operator!() const { return is_null; }

  // Note: Implicit conversion operations are generally _bad_ in C++.
  //       To check for non-nullptr value, use 'if (x.c_str()) ...'
  //
  // operator bool() const { return !is_null; }

private:
  bool is_null;
};

string_or_null latin1_to_utf8(const char *);
#ifdef METQU
string_or_null utf8_to_latin1(const char *);
#endif

/*---=== Aligned memory allocation ===---
 *
 * We need 16 byte alignment for SSE.
 *
 * Ref:
 * <http://www.gnu.org/software/libc/manual/html_node/Aligned-Memory-Blocks.html#Aligned-Memory-Blocks>
 *      'man posix_memalign'
 */
#ifdef __SSE__
inline float *sse_alloc(size_t bytes) {
  void *p;
  int rc = posix_memalign(
      &p, 16, bytes); // 16 "must be power of two and multiple of sizeof(void*)"
  if (rc != 0) {
    assert(rc == ENOMEM);
    throw std::runtime_error("out of memory (posix_memalign)");
  }
  assert(p);
  return (float *)p;
}

inline void sse_free(void *block) { free(block); }
#else
#define sse_alloc(bytes) ((float *)malloc(bytes))
#define sse_free(p) free(p)
#endif

/*---=== Performance counting ===---*/

/*
 * Tools for measuring code speed.
 *
 * Notes:
 *   While the performance counters in modern x86 processors might feel useful
 *   (http://www.scl.ameslab.gov/Projects/Rabbit) they will be puzzled by
 *   context switches to other processes. At the moment, for the sake of
 *   plugin's performance testing, a "time of day" (wall clock) ms value is
 *   enough.
 *
 *   Also, the behaviour of performance counters on virtualized hardware (s.a.
 *   crash.fmi.fi) is unknown. Could even be, they are not there at all.
 *   -- AKa 19-Jan-2009
 */
class Profiler_ms {
private:
  typedef uint64_t ms_t;
  ms_t cumulative; // sum of profiled sections
  ms_t last_start; // time when last section started (0 if not running)

  static ms_t offset; // first start ever (to keep debugging values low)

public:
  const std::string name; // for debugging

  Profiler_ms(const char *name_ = 0)
      : cumulative(0), last_start(0), name(name_ ? name_ : "") {}

  void start();
  void stop();
  unsigned long ms() const;

private:
  static ms_t cpu_ms();
};

#endif
// TOOLS_H
