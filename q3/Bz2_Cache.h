/*
 * BZ2_CACHE.H                 Copyright (c) 2008-2009, Ilmatieteen laitos
 *
 * Last revised:
 *       10-Dec-2009 AKa
 */
#ifndef BZ2_CACHE_H
#define BZ2_CACHE_H

#include "JDay.h"
#include "Tools.h"

#include <map>
#include <string>

/*
 * Class for making str-to-str cache persistent (used for BZ2 origintime
 * caching). Each process has just one cache, shared by all threads.
 */
class Bz2_Cache {
public:
  Bz2_Cache(const char *fn);
  ~Bz2_Cache();

  JDay get(const std::string
               &key); // no 'const' since can cause on-demand initialization
  void set(const std::string &key, JDay ot);
  void flush() const;

  bool hasCacheFile() const { return f != 0; }

private:
  friend class Bz2_Tracker;

  // functions
  void write_LOCKED(const std::string &key, JDay ot);

  // data
  Mutex mapping_m; // also protects writes to 'f'
  //
  std::map<std::string, JDay> mapping;
  FILE *f; // 0: memory cache only, >0: persistent

  // No copying or assigning
  //
  Bz2_Cache(const Bz2_Cache &);
  Bz2_Cache &operator=(const Bz2_Cache &);
};

#endif
