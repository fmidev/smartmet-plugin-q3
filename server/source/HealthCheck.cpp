/*
* HEALTHCHECK.CPP                    Copyright (c) 2010, Ilmatieteen laitos
*
* Run a separate thread to report CPU, disk usage etc. periodically to the
* (remote) logging service.
*
* Log output is in JSON format (see <http://www.json.org>).
*/
#include "HealthCheck.h"
#include "Tools.h"

#include <string>
#include <stdexcept>

#include <iostream>
#include <fstream>

#include <sys/time.h>
#include <sys/resource.h>
// getrusage()

#include <sys/statvfs.h>
#include <errno.h>

using namespace std;

/* NOTE:
*
* We can use this to get the memory used BY THIS PROCESS only.
<<
cat /proc/1234/smaps
It will tell you exactly how much memory it is using at that time. More importantly, it will divide
the memory into private and shared, so you can tell how much memory your instance of the program is
using, without including memory shared between multiple instances of the program.
<<
*/

/*
* CPU info
*
* Information about the CPU that does not change on-the-fly. Logged once.
<<
$ more /proc/cpuinfo
processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU           X5365  @ 3.00GHz
stepping	: 8
cpu MHz		: 3000.104
cache size	: 4096 KB
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36
clflush dts acpi mmx fxsr sse sse2 ss syscall lm constant_tsc pni ssse3 cx16
lahf_lm
bogomips	: 6000.20
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU           X5365  @ 3.00GHz
stepping	: 8
cpu MHz		: 3000.104
cache size	: 4096 KB
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36
clflush dts acpi mmx fxsr sse sse2 ss syscall lm constant_tsc pni ssse3 cx16
lahf_lm
bogomips	: 5999.39
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:
<<
*/
[[maybe_unused]]
static string cpu_info()
{
  unsigned cores = 0;
  string_or_null model;

#ifdef __linux__
  ifstream fs("/proc/cpuinfo");
  char buf[300];  // must fit all lines, otherwise 'getline()' stops iterating

  while (fs.getline(buf, sizeof(buf)))
  {
    if (begins_with(buf, "model name"))
    {
      cores++;
      if (!model)
      {
        const char *p = strchr(buf, ':');
        assert(p);
        model = p + 2;
      }
    }
  }
#else
#error "Not implemented for this OS."
#endif
  assert(cores > 0);
  assert(model.c_str());

  return string_fmt("{ cores:%d, model:\"%s\" }", cores, model.c_str());
}

/*
* CPU load averages
*
* We can either read the Linux '/proc/loadavg' directly (see 'man proc') or use the
* more portable 'getloadavg' call (from BSD).
*
* Ref: <http://blog.scoutapp.com/articles/2009/07/31/understanding-load-averages>
*      <http://en.wikipedia.org/wiki/Load_(computing)>
*
* Rule of thumb:
*   If the second or third (5min, 15min) average, divided by the number of CPU cores,
*   is staying > 0.7, there's too little headroom (but really, read the Ref above).
*   --AKa 19-Jan-10
*
<<
$ more /proc/loadavg
0.04 0.02 0.00 1/105 7584
<<
*/
[[maybe_unused]]
static string cpu_stat()
{
  double avg[3];

#ifdef __linux__
  ifstream fs("/proc/loadavg");
  fs >> avg[0] >> avg[1] >> avg[2];
#else
  int rc = getloadavg(avg, sizeof(avg) / sizeof(avg[0]));
  if (rc < 3)
    return "";  // unobtainable
#endif

  return string_fmt("{ 1: %.2lf, 5: %.2lf, 15: %.2lf }", avg[0], avg[1], avg[2]);
}

/*
* Memory load
*
* We're reading '/proc/meminfo' which is also the mechanism below Linux 'free' command (so there's
* no benefit in running that command in a shell).
*
* Ref:  <http://www.redhat.com/advice/tips/meminfo.html>
*
* Fields we use are marked with "<<".
* Fields we DON'T NEED are commented within "()"
* Other fields can be potentially useful
<<
$ more /proc/meminfo
MemTotal:      3866404 kB       << total usable (physical RAM - kernel usage)
MemFree:        228272 kB       <<
Buffers:        236148 kB       << "mostly useless as metric nowadays" (is it, why?)
Cached:        3092428 kB       << "memory in pagecache, minus 'SwapCached'"
SwapCached:          0 kB       << "Memory that once was swapped out, is swapped back in but still
also is in the swapfile"
Active:         782640 kB       "Memory that has been used more recently and usually not reclaimed
unless absolutely necessary"
Inactive:      2588808 kB
HighTotal:           0 kB       (0 in x86_64)
HighFree:            0 kB       (0 in x86_64)
LowTotal:      3866404 kB       (same as 'MemTotal' in x86_64)
LowFree:        228272 kB       (same as 'MemFree' in x86_64)
SwapTotal:     2096472 kB       << total amount of swap
SwapFree:      2096392 kB       << free swap memory (currently used swap is SwapTotal-SwapFree)
Dirty:              88 kB       ("Dirty means "might need writing to disk or swap." Takes more work
to free.")
Writeback:           0 kB
AnonPages:       42788 kB
Mapped:          19284 kB       <<
Slab:           240220 kB       <<
PageTables:       8808 kB
NFS_Unstable:        0 kB
Bounce:              0 kB
CommitLimit:   4029672 kB
Committed_AS:   155608 kB       << "estimate of how much RAM you would need to make a 99.99%
guarantee that there never is OOM (out of memory) for this workload"
VmallocTotal: 34359738367 kB
VmallocUsed:      1560 kB
VmallocChunk: 34359736567 kB
HugePages_Total:     0
HugePages_Free:      0
HugePages_Rsvd:      0
Hugepagesize:     2048 kB
<<
*/
[[maybe_unused]]
static string mem_stat(bool initial)
{
#ifdef __linux__
  long memtotal_kB = 0, memfree_kB = 0, buffers_kB = 0, cached_kB = 0, swapcached_kB = 0,
       swaptotal_kB = 0, swapfree_kB = 0, mapped_kB = 0, slab_kB = 0, committed_AS_kB = 0;

  ifstream fs("/proc/meminfo");
  char buf[100];  // must fit all lines, otherwise 'getline()' stops iterating

  while (fs.getline(buf, sizeof(buf)))
  {
    long *addr = nullptr;
    switch (*buf)
    {
      case 'B':
        if (begins_with(buf, "Buffers:"))
        {
          addr = &buffers_kB;
          break;
        }
        break;
      case 'C':
        if (begins_with(buf, "Cached:"))
        {
          addr = &cached_kB;
          break;
        }
        if (begins_with(buf, "Committed_AS:"))
        {
          addr = &committed_AS_kB;
          break;
        }
        break;
      case 'M':
        if (begins_with(buf, "Mapped:"))
        {
          addr = &mapped_kB;
          break;
        }
        if (begins_with(buf, "MemFree:"))
        {
          addr = &memfree_kB;
          break;
        }
        if (begins_with(buf, "MemTotal:"))
        {
          addr = &memtotal_kB;
          break;
        }
        break;
      case 'S':
        if (begins_with(buf, "Slab:"))
        {
          addr = &slab_kB;
          break;
        }
        if (begins_with(buf, "SwapCached:"))
        {
          addr = &swapcached_kB;
          break;
        }
        if (begins_with(buf, "SwapFree:"))
        {
          addr = &swapfree_kB;
          break;
        }
        if (begins_with(buf, "SwapTotal:"))
        {
          addr = &swaptotal_kB;
          break;
        }
        break;
      default:
        break;
    }

    if (addr)
    {
      const char *p = strchr(buf, ':');
      if (p)
      {
        *addr = strtol(p + 1, nullptr, 10);
      }
    }
  }

  (void)mapped_kB;  // not used

  /*
  * Note: What we want to report is completely up to us. Change these setups if more information
  *       is required. However, it may be wise to keep the original figures and names, instead
  *       of trying to be too 'smart'?    --AKa 15-Feb-10
  */
  if (initial)
  {
    return "";  // not used
    // return string_fmt( "{ total_MB: %.2lf }", memtotal_kB/1024.0 );
  }
  else
  {
    long used_kB = memtotal_kB - (memfree_kB + buffers_kB + cached_kB + swapcached_kB + slab_kB);
    long swapped_kB = swaptotal_kB - swapfree_kB;

    return string_fmt(
        "{ used_MB: %.2lf, mapped_MB: %.2lf, swapped_MB: %.2lf, free_MB: %.2lf, committed_AS_MB: "
        "%.2lf }",
        used_kB / 1024.0,
        mapped_kB / 1024.0,
        swapped_kB / 1024.0,
        memfree_kB / 1024.0,
        committed_AS_kB / 1024.0);
  }

// return string_fmt( "{ total_kB: %ld, free_kB: %ld, mapped_kB: %ld }", total_kB, free_kB,
// mapped_kB );
#else
#error "Not implemented for this OS."
#endif
}

/*
* Possibly nothing here for us?
*
* Ref: <http://www.linuxjournal.com/article/2396>
*
* NOTE: Weirdly, we don't see the "page <in> <out>" counters in here (from 'crash.fmi.fi') - maybe
because it is
*       a virtual machine? Those could be used for swapping statistics.
<<
$ more /proc/stat
cpu  81161294 60356 32203738 119420615 111049 2226260 2063938 0
cpu0 42587651 35676 15689098 59948150 53466 75585 233975 0
cpu1 38573642 24679 16514639 59472465 57582 2150675 1829963 0
intr 5101588981 1186416393 9 0 3 3 0 5 0 0 0 0 0 107 0 11034936 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 897883 0 0 0 0 0 0 0 3903239642 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0 0 0 0 0
ctxt 35365973351
btime 1262674561
processes 882506
procs_running 1
procs_blocked 0
<<
*/

/*
* This seems interesting: 'man getrusage'
<<
 struct rusage {
    struct timeval ru_utime; // user time used
    struct timeval ru_stime; // system time used
    long   ru_maxrss;        // maximum resident set size
    long   ru_ixrss;         // integral shared memory size
    long   ru_idrss;         // integral unshared data size
    long   ru_isrss;         // integral unshared stack size
    long   ru_minflt;        // page reclaims
    long   ru_majflt;        // page faults
    long   ru_nswap;         // swaps
    long   ru_inblock;       // block input operations
    long   ru_oublock;       // block output operations
    long   ru_msgsnd;        // messages sent
    long   ru_msgrcv;        // messages received
    long   ru_nsignals;      // signals received
    long   ru_nvcsw;         // voluntary context switches
    long   ru_nivcsw;        // involuntary context switches
};
<<
*/
#if 0  // not used
static string rusage() {
    struct rusage ru;

    int rc= getrusage( RUSAGE_SELF, &ru );
    assert(rc==0);

    ostringstream ss;
    ss << "{" 
        " maxrss:" << ru.ru_maxrss <<       // 0 on crash.fmi.fi
        " ixrss:" << ru.ru_ixrss <<         // -''-
        " idrss:" << ru.ru_idrss <<         // -''-
        " isrss:" << ru.ru_isrss <<         // -''-
        " minflt:" << ru.ru_minflt <<
        " majflt:" << ru.ru_majflt <<
        " nswap:" << ru.ru_nswap <<
    " }";
    return ss.str();
}
#endif

/*
* Disk usage
*
* What we want to show is similar to the 'df -h' output (with "mounted on" as our parameter):
<<
Filesystem            Size  Used Avail Use% Mounted on
/dev/sda3              18G   15G  2.6G  85% /
/dev/sda1             122M   13M  104M  11% /boot
tmpfs                 1.9G     0  1.9G   0% /dev/shm
meru:/pal/data        212G  134G   78G  64% /smartmet/data
meru:/smartmet/brainstorm 87G   60G   28G  69% /smartmet/brainstorm
suuli:/arch/pal       1.5T  1.3T  229G  85% /smartmet/archive
<<
*/
[[maybe_unused]]
static string_or_null disk_usage(const char *path)
{
  struct statvfs d;
  //
  // unsigned long  f_bsize;    // file system block size         <<<
  // unsigned long  f_frsize;   // fragment size                  <
  // fsblkcnt_t     f_blocks;   // size of fs in f_frsize units   <
  // fsblkcnt_t     f_bfree;    // # free blocks                  <<<
  // fsblkcnt_t     f_bavail;   // # free blocks for non-root     <<<
  // fsfilcnt_t     f_files;    // # inodes
  // fsfilcnt_t     f_ffree;    // # free inodes
  // fsfilcnt_t     f_favail;   // # free inodes for non-root
  // unsigned long  f_fsid;     // file system ID
  // unsigned long  f_flag;     // mount flags
  // unsigned long  f_namemax;  // maximum filename length

  int rc = statvfs(path, &d);
  if (rc != 0)
  {
    LOG_ERROR("'fstatvfs() gave errno: %d", errno);
    return nullptr;
  }

  // Note: 'f_bavail' and 'f_bfree' seem to be same (crash.fmi.fi) so we only
  //      report one (the "available for non-root" is fine).  --AKa 19-Jan-10

  const double GB = 1024.0 * 1024.0 * 1024.0;
  double total_GB = ((double)d.f_frsize) * d.f_blocks / GB;
  double avail_GB = ((double)d.f_bsize) * d.f_bavail / GB;

  return string_fmt("{ total_GB: %.3lf, avail_GB: %.3lf }", total_GB, avail_GB);
}

/*
* Network usage
*
<<
...
<<
*/
[[maybe_unused]] static string_or_null net_usage(const char *ethX) 
{
  return "";  // TBD - if needed
}

/*---=== HealthCheck ===---
*/

/*
*/
HealthCheck::HealthCheck(unsigned period_ms_, const char *conf)
    : thread_h(), period_ms(period_ms_), cpu(false), mem(false), swap(false), paths()
{
  assert(period_ms > 0);
  assert(conf);

  // Check each entry split by white space
  //
  const char *p = conf;
  while (true)
  {
    const char *p2 = strchr(p, ' ');
    string tmp = p2 ? string(p, p2 - p) : string(p);

    if (tmp == "cpu")
    {
      cpu = true;
    }
    else if (tmp == "mem")
    {
      mem = true;
    }
    else if (tmp == "swap")
    {
      swap = true;
    }
    else if (tmp == "*")
    {
      cpu = mem = swap = true;
    }
    else if (tmp[0] == '/')
    {
      paths.push_back(tmp);
    }
    else if (begins_with(tmp.c_str(), "eth"))
    {
      net_interfaces.push_back(tmp);
    }
    else
    {
      throw E_LOG_USAGE("Unknown healthcheck entry: %s", tmp.c_str());
    }

    if (!p2)
      break;

    while (isspace(*p2))
      ++p2;
    p = p2;
  }

  PTHREAD_CALL(pthread_create, &thread_h, nullptr, thread, (void *)this);
}

/*
*/
void *HealthCheck::thread(void *me_v)
{
  const HealthCheck &my = *((const HealthCheck *)me_v);

  // Log one-time values here (number of CPU cores, physical memory)
  //
  LOG_STAT("CPU %s", cpu_info().c_str());

  while (true)
  {
    Sleep_ms(my.period_ms);

    string s = "";

    if (my.cpu)
    {
      LOG_STAT("CPU %s", cpu_stat().c_str());
    }
    if (my.mem)
    {
      LOG_STAT("MEM %s", mem_stat(false).c_str());
    }
    if (my.swap)
    {
      // TBD - if needed
    }
    for (vector<string>::const_iterator it = my.paths.begin(); it != my.paths.end(); ++it)
    {
      const char *path [[maybe_unused]] = it->c_str();
      LOG_STAT("DISK %s %s", path, disk_usage(path).c_str());
    }
    for (vector<string>::const_iterator it = my.net_interfaces.begin();
         it != my.net_interfaces.end();
         ++it)
    {
      const char *ethX [[maybe_unused]] = it->c_str();
      LOG_STAT("NET %s %s", ethX, net_usage(ethX).c_str());
    }
  }

  return 0;  // never
}
