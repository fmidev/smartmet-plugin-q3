/*
* BZ2_CACHE.CPP               Copyright (c) 2008-2009, Ilmatieteen laitos
*
* File name -> origintime cache file.
*
* Last revised:
*       12-Mar-2009 AKa
*/
#include "Bz2_Cache.h"
#include "SQD_Tools.h"

using namespace std;

#include <newbase/NFmiMetTime.h>

JDay str2JDay(const char *fn, const char *ot)
{
  if (strspn(ot, "1234567890") != 14)
  {
    LOG_WARNING("Bad cache line (ignored): %s=%s", fn, ot);
    return 0;
  }

  char yyyy[5], MM[3], dd[3], hh[3], mm[3], ss[3];
  strncpy(yyyy, ot, 4);
  strncpy(MM, ot + 4, 2);
  strncpy(dd, ot + 6, 2);
  strncpy(hh, ot + 8, 2);
  strncpy(mm, ot + 10, 2);
  strncpy(ss, ot + 12, 2);

  NFmiMetTime mt(atoi(yyyy), atoi(MM), atoi(dd), atoi(hh), atoi(mm), atoi(ss));

  return SQD_Tools::mt2jd(mt);
}

/* ======= Bz2_Cache ======= */

/*
* Read the cache file and remove any entries from it where the file is currently
* non-existing. Helps to keep the cache small(ish).
*
* Once dealt with, the file is rewritten (if needed) and kept open with
* append access.
*/
Bz2_Cache::Bz2_Cache(const char *fn) : mapping_m(), mapping(), f(0)
{
  if (!fn)
    return;  // no persistence requested (fast to start up but slower to use)

  string fn_store;

  /*
  * If name contains '~' replace it with $(HOME).
  */
  if (begins_with(fn, "~/"))
  {
    const char *home = getenv("HOME");
    if (!home)
    {
      throw E_LOG_FATAL0("HOME env.var. not set");
    }
    fn_store = string(home) + (fn + 1);
    fn = fn_store.c_str();
  }
  else if (*fn != '/')
  {
    throw E_LOG_FATAL("Cache filename needs to start with '/' or '~/' (%s)", fn);
  }

  unsigned changed = 0;

  /* 'man fopen':
   *  "a+ Open for reading and appending (writing at end of file). The file
   *   is created if it does not exist. The initial file position for reading
   *   is at the beginning of the file, but output is always appended to the
   *   end of the file."
   */
  f = fopen(fn, "a+");
  if (!f)
  {
    throw E_LOG_FATAL("Unable to open %s for appending.", fn);
  }

  LOG_OK("Reading BZ2 cache... %s", fn);
  uint64_t t0 = now_ms();

  // Read existing contents of the file in
  //
  char buf[PATH_MAX + 20];  // enough for "filename=yyyymmddhhmmss\n"

  while (fgets(buf, sizeof(buf), f))
  {  // key=val\n
    char *p = strchr(buf, '=');
    if (!p)
    {
      LOG_WARNING("Bad cache line (removed): %s", buf);
      changed++;  // recreate the cache so bad lines won't stick along
      continue;
    }
    *(p++) = '\0';
    char *p2 = strchr(p, '\n');
    if (p2)
      *p2 = '\0';

// Check if file 'buf' is still there?
//
// NOTE: Checking that the files are still there takes too long (around 30sec
//      for Q3 test configs). We'll leave all valid-looking entries in the cache
//      and failing to open them (on demand) will show if they're indeed gone.
//      --AKa 30-Nov-2009
//
// NB: Use of 'stat' may be faster than 'fopen()', if we really need to
//      check validity here.
#if 0
        FILE *ff= fopen(buf,"r");
        if (!ff) {
            changed++;
            continue;
        }
        fclose(ff);
#endif

    JDay t = str2JDay(buf, p);
    if (!t)
    {
      changed++;  // bad time; fix the cache
    }
    else
    {
      mapping[buf] = t;  // overwrite if existing (same key may exist multiple times,
                         // last one applies)
    }
  }

  if (changed)
  {
    unsigned n = mapping.size();
    LOG_DEBUG("Writing changes to BZ2 cache (%d -> %d lines)", n + changed, n);

    fclose(f);
    f = fopen(fn, "w");
    if (!f)
    {
      throw E_LOG_FATAL("Unable to open %s for writing", fn);
    }

    for (map<string, JDay>::const_iterator it = mapping.begin(); it != mapping.end(); ++it)
    {
      write_LOCKED(it->first, it->second);
    }
    flush();
  }

  LOG_DEBUG("BZ2 cache ready (%d ms)", now_ms() - t0);
}

/*
* Never entered in server mode
*/
Bz2_Cache::~Bz2_Cache()
{
  mapping_m.Destroy();

  if (f)
    fclose(f);
}

/*
* Read cache
*/
JDay Bz2_Cache::get(const string &key)
{
  JDay ot;

  {
    ClaimMutex lock(mapping_m);
    // init_LOCKED();   // load on demand

    map<string, JDay>::const_iterator it = mapping.find(key);
    ot = (it != mapping.end()) ? it->second : (JDay)0 /*no entry*/;
  }  // release 'mapping_m'

  return ot;
}

/*
* Write cache
*/
void Bz2_Cache::set(const string &key, JDay ot)
{
  {
    ClaimMutex lock(mapping_m);
    // init_LOCKED();

    // Beware of duplicate entries (they would grow the cache file)
    //
    map<string, JDay>::const_iterator it = mapping.find(key);
    if (it != mapping.end())
    {
      JDay ot2 = it->second;

      if (ot == ot2)
        return;  // already there

      string tmp1 = ot.toString();
      string tmp2 = ot2.toString();

      LOG_WARNING(
          "Two origintimes for same file: %s != %s (%s)", tmp1.c_str(), tmp2.c_str(), key.c_str());

      // We'll append the new value - there will be duplicates
      // until next time the cache gets corrected for some other
      // reason (that will keep the later entry only).
    }

    mapping[key] = ot;
    if (f)
    {
      write_LOCKED(key, ot);
    }
  }
}

void Bz2_Cache::write_LOCKED(const string &key, JDay ot)
{
  assert(f);
  string tmp = ot.toString();
  fprintf(f, "%s=%s\n", key.c_str(), tmp.c_str());
}

/*
* Flush changes to the disk.
*/
void Bz2_Cache::flush() const
{
  if (f)
    fflush(f);  // can be done without locking
}
