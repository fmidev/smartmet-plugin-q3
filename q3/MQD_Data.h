/*
 * MQD_DATA.H                            Copyright (c) 2010, Ilmatieteen laitos
 *
 * Adapter for MQD file format (Meteorological Query Data)
 *
 * The MQD format is a further development of FMI SQD data format. It has a
 * human readable text mode header and allows 1-to-1 data transfer from SQD and
 * back (on the parts that are used by q3, that is - SQD format may have
 * features that q3 does not use, they may be lost in such a transfer).
 *
 * In *addition* to SQD (version 7) data capabilities, MQD provides:
 *
 *   - ability to have any set of validtimes (need not be evenly spaced)
 *   - ability to have any set of levels (SQD allows one file to only contain
 * one type of levels, either ground, hybrid or pressure)
 *   - ability to store unit names for parameters
 *   - ability to have dynamic grid size; each time/level/parameter combination
 * ("tile") may have its own grid resolution (SQD has a file-wide, fixed grid
 * size).
 *   - ability to have any parameter grid within the data be compressed, to save
 * disk space. Often used parameters (or levels, or times) may be left
 * uncompressed while others are compressed. (SQD has no built-in support for
 * compression. Compressing whole SQD files makes accessing them tremendously
 * slow.)
 *   - uncompressed float data is stored 16-byte aligned, to allow fast SSE
 * access without need to copy to memory (using file memory mapping)
 *   - ability to store data of various kinds (float, uint16, uint8, uint4,
 * bool), per parameter. This (together with compression) enables small file
 * sizes and takes away the need for complicated "merged parameters". (SQD files
 *       employ merged parameters to keep file sizes small. This complicates
 * code.)
 *
 * TBD: Currently, all data is stored as float. Support for 'uint16', 'uint8',
 * 'uint4' and 'bool' storage types remains to be done.
 *
 * Revised: 22-Oct-2010
 */
#ifndef MQD_DATA_H
#define MQD_DATA_H

#ifndef MQD_ENABLED
#error "Should not have read this header."
#endif

#include "NA_Data.h"
#include "NA_Level.h"
#include "NA_Param.h"

class Matrix;
class MemoryMap;

/*
 * Covers a single MQD file ('Raw')
 */
class MQD_Data : public NA_Data {
public:
  MQD_Data(const std::string &fn) throw(E_BAD_FILE);
  ~MQD_Data();

#ifdef METQU
  MQD_Data(const NA_Info &info); // values not initialized
#endif

  static bool is_mqd_file(const char *fn);

  /*
   * Data storage type (DO NOT CHANGE; USED IN FILE FORMAT)
   */
  enum MQD_StorageType { FLOAT = 0, UINT2, UINT4, UINT8, UINT16 };

  /*
   * Interpolation method (DO NOT CHANGE; USED IN FILE FORMAT)
   */
  enum MQD_Method { NEAREST = 0, LINEAR, LINEAR_DEG, LINEAR_LON };

  /*
   * Param tile for each time, level, param combo (stored in order at the
   * beginning of the binary block).
   */
  struct MQD_StoredTile {
  public:
    MQD_StoredTile(MQD_StorageType st, MQD_Method method_, const MatrixPos &gs_,
                   size_t compressed_size_, size_t block_offset);

    MQD_StorageType getStorageType() const;
    MQD_Method getMethod() const;
    MatrixPos getGridSize() const;
    size_t getCompressedSize() const;
    size_t getBlockOffset() const;

  private:
    const uint32_t gs_x, gs_y;
    const uint32_t compressed_block_size; // 0 for not
    const uint64_t offset;                // bits 0..47: block offset
                                          // bits 48..58: 0 (unused)
                                          // bits 59..61: data storage type
                                          // bits 62..63: interpolation method
  };

  struct Tile {
  public:
    Tile(const MQD_StoredTile &stile); // initialization from a file
#ifdef METQU
    Tile(NA_Param::e_Datatype dt_, NA_Param::e_Interpolation method_,
         const Matrix &m,
         bool compress); // interpolation from a matrix
#endif

    const MatrixPos &getGridSize() const { return gs; }

    const float *getPtr() const { return block; }
#ifdef METQU
    float *getPtr() { return block; }
#endif

  private:
    const void *mm_block; // raw data if memory mapped

    float *block; // allocated block (SSE aligned); for read-write or access of
                  // non-float stored or compressed data

    MatrixPos gs;

    NA_Param::e_Datatype dt;
    NA_Param::e_Interpolation method;
    bool compressed;
  };

#ifdef METQU
  /*virtual*/ bool isReadOnly() const { return is_readonly; }

  /*virtual*/ void output(std::ostream &out, ProgressCallback *cb) const;
#endif

  /*virtual*/ CONST_IF_SERVER Matrix *
  push_NativeMatrix(lua_State *L, const JDay &vt, const NA_Level &lev,
                    const NA_Param &p) CONST_IF_SERVER throw();

  static NA_Info read_info(const char *fn) throw(E_BAD_FILE);

  // We don't calculate pressure levels from hybrid data (at least, not yet).
  //
  /*virtual*/ bool providesPressureLevelsFromHybrid() const { return false; }

  // public for 'MQD_Matrix'
  //
#ifdef METQU
  Tile &getTile(const JDay &vt, const NA_Level &lev,
                const NA_Param &p) throw(E_READONLY, E_NO_MATCH);
#endif
  const Tile &getTile(const JDay &vt, const NA_Level &lev,
                      const NA_Param &p) const throw(E_NO_MATCH);

private:
  // data fields
  //
  const MemoryMap *mm; // non-nullptr if memory mapping used (keeps the 'block'
                       // pointer valid)
  const void *block;   // binary block (memory mapped)

  Tile *const tiles;

#ifdef METQU
  const bool is_readonly;
#endif

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {
#ifndef METQU
    assert_invariant(mm); // always memory-mapped
#endif
    assert_invariant(tiles);
  }
#endif
};

#endif
// MQD_DATA_H
