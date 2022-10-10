/*
 * HEALTHCHECK.H                       Copyright (c) 2010, Ilmatieteen laitos
 *
 * Logging CPU, disk usage etc. periodically to the remote logging service.
 */
#ifndef HEALTHCHECK_H
#define HEALTHCHECK_H

#include "Tools.h"

/*
 */
class HealthCheck {
public:
  HealthCheck(unsigned period_ms_, const char *conf);

  static void *thread(void *me_v);

private:
  // private data:
  //
  pthread_t thread_h; // (not used; could be used to terminate the thread)
  unsigned period_ms;

  bool cpu;  // report CPU load
  bool mem;  // report memory usage
  bool swap; // (maybe we don't need this)
             // ...

  std::vector<std::string> paths; // report disk usage on these
  std::vector<std::string>
      net_interfaces; // report network usage on these ("eth0" etc.)
};

#endif
// HEALTHCHECK_H
