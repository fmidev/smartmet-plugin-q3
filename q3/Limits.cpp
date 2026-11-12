/*
 * LIMITS.CPP
 */
#include "Limits.h"
#include "MatrixPos.h"
#include "Lua.h"

namespace Limits
{
int64_t max_image_pixels = 25000000;
int64_t max_grid_cells = 25000000;

void check_image_size(lua_State *L, double w, double h) {
  // Negated comparisons so that NaN is rejected too
  if (!(w > 0) || !(h > 0))
    luaL_error(L, "Invalid surface size %f x %f", w, h);

  if (max_image_pixels > 0 &&
      !(w * h <= static_cast<double>(max_image_pixels)))
    luaL_error(L, "Too large surface %f x %f (max %f pixels)", w, h,
               static_cast<double>(max_image_pixels));
}

void check_grid_size(lua_State *L, int64_t cols, int64_t rows) {
  if (cols <= 0 || rows <= 0)
    luaL_error(L, "Invalid grid size %f x %f", static_cast<double>(cols),
               static_cast<double>(rows));

  // Both factors fit in 32 bits, so the 64-bit product cannot overflow
  if (max_grid_cells > 0 && cols * rows > max_grid_cells)
    luaL_error(L, "Too large grid %f x %f (max %f cells)",
               static_cast<double>(cols), static_cast<double>(rows),
               static_cast<double>(max_grid_cells));
}

void check_grid_size(lua_State *L, const MatrixPos &gs) {
  check_grid_size(L, gs.getX(), gs.getY());
}
} // namespace Limits
