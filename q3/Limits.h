/*
 * LIMITS.H
 *
 * Caps for C-side allocations whose size is controlled by a script. These are
 * not seen by the Lua allocator budget nor interrupted by the killtime hook.
 */
#ifndef Q3_LIMITS_H
#define Q3_LIMITS_H

#include <cstdint>
#include <lua.hpp>

class MatrixPos;

namespace Limits
{
// Maximum number of pixels (width*height) in a Cairo surface (0 for no limit)
extern int64_t max_image_pixels;

// Maximum number of cells (columns*rows) in a grid or matrix (0 for no limit)
extern int64_t max_grid_cells;

// Raise a Lua error if the surface size is invalid or too large
void check_image_size(lua_State *L, double w, double h);

// Raise a Lua error if the grid size is invalid or too large
void check_grid_size(lua_State *L, int64_t cols, int64_t rows);
void check_grid_size(lua_State *L, const MatrixPos &gs);
} // namespace Limits

#endif
// Q3_LIMITS_H
