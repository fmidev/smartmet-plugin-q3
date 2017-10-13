/*
 * Q3ENGINE.H                   Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Features of a generic Q3 server
 */
#ifndef Q3_ENGINE_H
#define Q3_ENGINE_H

#include <map>
#include <vector>
#include <string>
#include <ostream>

#include "Tools.h"

/*
*/
class Q3Engine
{
 public:
  Q3Engine(const char *conf);
  ~Q3Engine();

  class Track;

  std::vector<std::string> getNames() const;
  const Track *getTrack(const char *name) const;

  static int getAddonConfigSetting(lua_State *L);

  static string_or_null /*mime*/ result_(lua_State *L, std::ostream &os, unsigned i);

 private:
  // Trackers of the whole configuration (used for initializing globals
  // for session Lua states)
  //
  std::map<std::string, Track *> trackers;

  // Storage for addon configuration settings (e.g. include.file, fminames.dbhost etc)
  //
  static std::map<std::string, std::string> addonConfigSettings;
 
  // Disallow copying and assigning
  //
  Q3Engine(const Q3Engine &other);
  Q3Engine &operator=(const Q3Engine &other);
};

#endif
// Q3ENGINE_H
