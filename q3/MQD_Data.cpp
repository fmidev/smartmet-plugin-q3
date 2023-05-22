/*
 * MQD_DATA.CPP                          Copyright (c) 2009-10, Ilmatieteen
 * laitos
 *
 * Revised: 22-Oct-2010
 */
#ifdef MQD_ENABLED

#include "MQD_Data.h"
#include "MQD_Tools.h"

#include "LuaWrap.h"
#include "Tools.h"

#ifdef __SSE__
#include "SSE.h"
#endif

#include "MQD_Matrix.h"
#include "MemMatrix.h"

static const char CTRLD_CTRLZ[] = {0x04, 0x1a};

#include <fstream>

using namespace std;

static char mqd_reader_chunk[] =
#include "mqd_reader.lch"

    static const char SPACES[15] = {' '};

/*---=== Helpers ===---*/

/*
 * Write 0..'n'-1 paddings to get the current writing position of 'out' aligned.
 *
 * 'n':  4 (for uint32|float alignment)
 *       16 (for SSE alignment)
 */
#if 0
static void pad_to( ostream &out, unsigned n ) {
    static const char ZEROES[15]= {0};

    size_t pos= out.tellp();
    if (pos%n) {
        out.write( ZEROES, n-(pos%n) );
    }
}
#endif

/*
 * Align a number to next even 4.
 *
 * This is used for getting a full 16-byte aligned (4 floats) breadth for SSE
 * operations. Data tiles are guaranteed to have the extra bytes (if needed)
 * available after their actual data.
 */
#if 0 // def METQU
static MatrixPos::offset_t align_up_4( MatrixPos::offset_t n ) {
    const unsigned ALIGN= 16 / sizeof(float);   // 4
    
    return (n % ALIGN) ? (n - n%ALIGN + ALIGN) : n;
}
#endif

/*---=== MQD_Data ===---*/

/*
 */
MQD_Data::MQD_Data(const string &fn)
    : NA_Data(read_info(fn.c_str())),
      mm(new MemoryMap(
          fn.c_str(), 0 /*TBD getBinaryOffset()*/,
          0 /*TBD getBinaryLength()*/)), // mmap to the end of the file
#ifdef METQU
// tiles( (float *) mm->getPointer() )
#else
// tiles( (const float *) mm->getPointer() )
#endif
      tiles(nullptr)
#ifdef METQU
      ,
      is_readonly(true)
#endif
{

  // Check that the file was all there (that tail is not lost in copying)
  //
  size_t len_actual = mm->getFileSize();
  size_t len_expected = 0; // TBD getBinaryOffset() + getBinaryLength();

  if (len_actual < len_expected) {
    throw E_LOG_BAD_FILE(fn, string_fmt("Partial file: length %ul < %u",
                                        len_actual, len_expected));
  }

  INVARIANT();
}

/*
 * Create an in-memory MQD_Data object (contents initialized by caller).
 */
#ifdef METQU
MQD_Data::MQD_Data(const NA_Info &info)
    : NA_Data(info), mm(0), // not associated to a file
      tiles(0 /* TBD sse_alloc( getBinaryLength() ) */), is_readonly(false) {

  INVARIANT();
}
#endif

/*
 */
MQD_Data::~MQD_Data() {
  INVARIANT();

#ifdef METQU
  if (!mm) {
    sse_free(tiles);
    return;
  }
#endif
  delete mm; // 'tiles' is just a shadow to memory mapped memory; let be
}

/*
 * Check if a file is MQD format (starts with "--MQD 1.0\n") or not.
 */
bool MQD_Data::is_mqd_file(const char *fn) {
  ifstream f(fn);
  if (f.is_open() && f.good()) {
    string tmp;
    getline(f, tmp);

    if (begins_with(tmp.c_str(), "--MQD 1.0 ")) {
      return true; // 'f' is automatically closed
    }
  }
  return false; // 'f' automatically closed
}

/*
 * Return a pointer to the memory containing given time, level and parameter.
 *
 * Internal data structure is:
 *   times
 *       levels
 *           params
 *               xy (padded to even 16 bytes)
 */
#ifdef METQU
MQD_Data::Tile &MQD_Data::getTile(const JDay &vt, const NA_Level &lev,
                                  const NA_Param &p) throw(E_READONLY,
                                                           E_NO_MATCH) {
  if (is_readonly) {
    throw E_READONLY();
  }

  throw E_LOG_BUG0("TBD");
  /*
      // Get the right address from const 'getTile()' and then cast it to
     writable
      //
      return const_cast<const MQD_Data*>(this)->getTile( vt, lev, p );   //
     throws E_NO_MATCH
  */
}
#endif

const MQD_Data::Tile &MQD_Data::getTile(const JDay &vt, const NA_Level &lev,
                                        const NA_Param &p) const
    {
  int time_index = find_index<JDay>(getTimes(), vt);
  if (time_index < 0) {
    throw E_NO_MATCH(vt);
  }

  int level_index = find_index<NA_Level>(getLevels(), lev);
  if (level_index < 0) {
    throw E_NO_MATCH(lev);
  }

  int param_index = find_index<NA_Param>(getParams(), p);
  if (param_index < 0) {
    throw E_NO_MATCH(p);
  }

  unsigned long times_n = getTimes().size();
  unsigned long levels_n = getLevels().size();
  unsigned long params_n = getParams().size();

  assert((unsigned)time_index < times_n);
  (void)times_n;
  assert((unsigned)level_index < levels_n);
  assert((unsigned)param_index < params_n);

  unsigned long grids_to_pass =
      time_index * (levels_n * params_n) + level_index * params_n + param_index;

  (void)grids_to_pass;
  throw E_LOG_BUG0("TBD");
}

/*
 * Write contents of an 'MQD_Data' object to a file.
 */
#ifdef METQU
void MQD_Data::output(ostream &out, ProgressCallback *cb) const {

  const vector<JDay> &times = getTimes();
  const vector<NA_Level> &levels = getLevels();
  const vector<NA_Param> &params = getParams();

  // Header portion
  //
  out << "--MQD 1.0\n" // MUST BE LIKE THIS (used for format detection)
         "--\n"
         "-- MQD data format (C) Ilmatieteen Laitos 2009-10\n"
         "--\n"
         "origintime= "
      << getOriginTime().toString()
      << "\n"
         "times= "
      << getTimes()
      << "\n"
         "levels= "
      << getLevels()
      << "\n"
         "params= "
      << params
      << "\n"
         "projection= "
      << getProjection()
      << "\n"
         "\n"
         "-- End of header portion. Rest of the file is binary and needs to "
         "remain properly aligned.\n";

  // Newline after '^D^Z' is important to let Lua 'f:lines()' read the end mark
  // as a line of its own.
  //
  // Writing is padded (with spaces) so that the location after '\n' is 16 byte
  // aligned.
  //
  size_t pos =
      ((size_t)out.tellp()) + sizeof(CTRLD_CTRLZ) + 1; // without padding

  out.write(CTRLD_CTRLZ, sizeof(CTRLD_CTRLZ));
  if (pos % 16) {
    out.write(SPACES, 16 - (pos % 16));
  }
  out << "\n";

#ifndef NDEBUG
  assert(out.tellp() % 16 == 0);
#endif

  // TBD: Write grid index table to 'out'
  //
  //  Grid size for each grid
  //  Index from base of the index table where the data actually is

  (void)times;
  (void)levels;
#if 0
    MatrixPos::offset_t values_n= getGridSize().getN();
    size_t padding_n= values_per_tile() - values_n;
    
    assert( padding_n <= 3 );

    // Write out the lot. Write out all gaps as NAN.
    //
    static const char NANS[3*4]= { 0xff };      // NAN if read as floats

    const float *p= tiles;

    unsigned long progress_end= times.size() * levels.size() * params.size();
    unsigned long progress_i=1;  // 1..tiles_n

    for( vector<JDay>::const_iterator time_it= times.begin();
        time_it != times.end();
        ++time_it ) {
        string time_str= time_it->toString();    // avoid converting on each progress inner loop
        
        for( vector<NA_Level>::const_iterator level_it= levels.begin();
            level_it != levels.end();
            ++level_it ) {
            for( vector<NA_Param>::const_iterator p_it= params.begin();
                p_it != params.end();
                ++p_it ) {
                
                out.write( (const char *)p, values_n*sizeof(float) );
                if (padding_n>0) {
                    out.write( (const char *)NANS, padding_n*sizeof(float) );
                }
                p += values_per_tile();

                if (cb) {
                    cb->progress( progress_i++, progress_end, 
                                    time_str.c_str(),
                                    level_it->getName().c_str(),
                                    p_it->toString().c_str() );
                }
            }
        }
    }
#endif
}
#endif

/*
 * Extract header information from an MQD file.
 */
NA_Info MQD_Data::read_info(const char *fn) {
  assert(fn);

  // Standard libraries for 'mqd_reader.lua'.
  //
  static const luaL_Reg my_stdlibs[] = {
      {"", luaopen_base},               // 'select' etc. basic tools
      {LUA_TABLIBNAME, luaopen_table},  // 'table.*'
      {LUA_STRLIBNAME, luaopen_string}, // 'string.*'
      {LUA_IOLIBNAME, luaopen_io},      // 'io.*'
      {nullptr, nullptr}};

  LuaWrap L(my_stdlibs, mqd_reader_chunk, sizeof(mqd_reader_chunk));

  // [1]: chunk function (run it once, it will return the 'mqd_header' function)
  //
  int st =
      lua_pcall(L, 0 /*args*/, 1 /*return values*/, 0 /*no error func needed*/);
  //
  // 0 (ok)
  // LUA_ERRRUN: runtime error
  // LUA_ERRMEM: no memory
  // (LUA_ERRERR: not for us)

  if (st != 0) {
    throw E_LOG_BUG("%s", lua_tostring(L, -1));
  }

  L_ASSERT(lua_gettop(L) == 1);
  L_ASSERT(lua_isfunction(L, 1));

  // Call the function
  //
  lua_pushstring(L, fn);
  lua_pushlstring(L, CTRLD_CTRLZ, sizeof(CTRLD_CTRLZ)); // end marker

  st =
      lua_pcall(L, 2 /*args*/, 2 /*return values*/, 0 /*no error func needed*/);
  //
  // 0 (ok)
  // LUA_ERRRUN: runtime error
  // LUA_ERRMEM: no memory
  // (LUA_ERRERR: not for us)

  if (st != 0) {
    throw E_LOG_BUG("%s", lua_tostring(L, -1));
  }

  // [1]: { producer= str, ... }
  // [2]: position after the header portion (directly after last line)
  //
  L_ASSERT(lua_istable(L, 1));
  L_ASSERT(lua_isnumber(L, 2));

  // Check that the header was 16-byte aligned
  //
  size_t pos = lua_tointeger(L, 2);
  if (pos % 16) {
    // TBD: Give error - someone has edited the header fields
  }
  size_t binary_offset = pos;

  // [1]: table read from the header
  //
  const int table_idx = 1;

  unsigned version = 0;
  JDay ot;
  vector<JDay> times;

  // Levels have been output using 'NA_Level' names, so we'll also read them in
  // that way.
  vector<NA_Level> levels;
  vector<NA_Param> params;
  string projection;

  lua_pushnil(L); // first key
  while (lua_next(L, table_idx)) {
    // [-1]: value
    // [-2]: key

    const char *key = lua_tostring(L, -2);
    if (!key) {
      throw E_LOG_BAD_FILE(fn,
                           string_fmt("Bad header: %s as key (expected string)",
                                      L_typename(-2)));
    }

    const char *s = lua_tostring(L, -1);

    // LOG_DEBUG( "%s: %s", key, s );

    try {
      if (strcmp(key, "version") == 0) {
        version = lua_tointeger(L, -1);
      } else if (strcmp(key, "origintime") == 0) {
        ot = JDay(L, -1);
        if (!ot) {
          // Don't use 'luaL_error'; we've crafted the state and it wouldn't
          // lead anywhere (good)
          //
          throw E_LOG_BAD_FILE(
              fn, string_fmt("Bad origintime: %s", L_string_or_typename(-1)));
        }
      } else if (strcmp(key, "times") == 0) {
        times = vector_of_times(L, -1); // can throw E_USAGE
      } else if (strcmp(key, "levels") == 0) {
        levels = vector_of_levels(L, -1); // can throw E_USAGE
      } else if (strcmp(key, "params") == 0) {
        params = vector_of_params(L, -1); // can throw E_USAGE
      } else if (strcmp(key, "projection") == 0) {
        projection = s;
      } else {
        // Unexpected header fields are not an error; this is similar to XML,
        // ignoring new keys.
        //
        LOG_WARNING("Unexpected header key opening %s (ignored): %s", fn, key);
      }
    } catch (const E_ANY &e) {
      // Convert problems reported in individual header fields (E_USAGE) into
      // E_BAD_FILE
      //
      throw E_LOG_BAD_FILE(fn, e.what());
    }

    lua_pop(L, 1);
  }

  // Check that fields are meaningful, and everything existed
  //
  (void)version; // currently not checked

  try {
    NA_Info info(ot, times, levels, params, Projection(projection.c_str()));

    info.setExtra_fn(fn);
    return info;
  } catch (const E_ANY &e) {
    // Any surprises mean the file header was somehow bad
    //
    throw E_BAD_FILE(fn, e.what());
  }

  (void)binary_offset;
}

/*
 * Returns the number of bytes the binary part is _supposed_ to be taking (it is
 * not confirmed that the file is that long; maybe we should do that, and look
 * for some 'completeness' tags after the binary data?)
 */
#if 0
size_t MQD_Data::getBinaryLength() const {

    // TBD: Need to go through grid index for this

#if 1
    throw E_LOG_BUG0("TBD");
#else
    size_t bytes_per_tile_= values_per_tile() * sizeof(float);

    return bytes_per_tile_ * getTimes().size() * getLevels().size() * getParams().size();
#endif
}
#endif

/*
 */
CONST_IF_SERVER Matrix *
MQD_Data::push_NativeMatrix(lua_State *L, const JDay &vt, const NA_Level &lev,
                            const NA_Param &p) CONST_IF_SERVER
    throw(/*E_BUG*/) {

  // Give a proxy matrix to the data (allowing to change it unless we're
  // read-only).
  //
  return L ? new (L) MQD_Matrix(this, vt, lev, p)
           : ::new MQD_Matrix(this, vt, lev, p);
}

#endif
// MQD_ENABLED
