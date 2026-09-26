/*
 * CONTOUR_TRAX.CPP
 *
 * Implementation of contouring adapter using FMI Trax library.
 *
 * Replaces the earlier Tron based adapter. The contouring semantics are kept
 * identical:
 *
 *   - contours for 'lo_val' (and optionally 'hi_val') are the boundaries of the
 *     region lo_val <= z < hi_val (NaN hi_val == +inf, lo_val 32700 == -inf)
 *   - lo_val NaN returns the limits of the data (grid edge or holes)
 *   - each point is marked as 'edge' if it lies on the edge of the data (grid
 *     edge or boundary of missing values)
 */

#include "Contour.h"

#include "Matrix.h"
#include "TronHints.h"

#include <smartmet/trax/Contour.h>
#include <smartmet/trax/Grid.h>
#include <smartmet/trax/IsobandLimits.h>
#include <smartmet/trax/SavitzkyGolay.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

/*---=== Grid adapter ===---*/

class MyGrid : public Trax::Grid {
public:
  explicit MyGrid(ContourMatrix &cm_) : cm(cm_) {}

  double x(long i, long j) const override { return i; }
  double y(long i, long j) const override { return j; }

  float operator()(long i, long j) const override {
    return cm.getValue(MatrixPos(i, j));
  }

  void set(long i, long j, float z) override {
    cm.getValue(MatrixPos(i, j)) = z;
  }

  bool valid(long i, long j) const override { return true; }
  std::size_t width() const override { return cm.getXS(); }
  std::size_t height() const override { return cm.getYS(); }

private:
  ContourMatrix &cm;
};

/*---=== Data edge detection ===---
 *
 * Trax handles missing values the same way as Tron did: cells with two or more
 * NaN corners are skipped, cells with exactly one NaN corner are contoured as
 * the triangle formed by the three valid corners. The domain of the data is
 * thus the union of the valid cells and triangles, and a point is at edge if
 * any of the probe points around it falls outside the domain.
 */
class EdgeDetector {
public:
  explicit EdgeDetector(const MyGrid &grid_)
      : grid(grid_), w(grid_.width()), h(grid_.height()) {}

  bool at_edge(double x, double y) const {
    const double eps = 1e-6;
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++)
        if ((di != 0 || dj != 0) && !inside(x + di * eps, y + dj * eps))
          return true;
    return false;
  }

private:
  bool missing(long i, long j) const { return std::isnan(grid(i, j)); }

  bool inside(double x, double y) const {
    if (x < 0 || y < 0 || x > w - 1 || y > h - 1)
      return false;

    const long i = std::min(static_cast<long>(std::floor(x)), w - 2);
    const long j = std::min(static_cast<long>(std::floor(y)), h - 2);

    const bool m00 = missing(i, j);
    const bool m10 = missing(i + 1, j);
    const bool m01 = missing(i, j + 1);
    const bool m11 = missing(i + 1, j + 1);

    const int n = m00 + m10 + m01 + m11;
    if (n == 0)
      return true;
    if (n > 1)
      return false;

    // Exactly one NaN corner: only the triangle opposite to it is inside

    const double u = x - i;
    const double v = y - j;
    if (m00)
      return u + v >= 1;
    if (m11)
      return u + v <= 1;
    if (m10)
      return u <= v;
    return u >= v; // m01
  }

  const MyGrid &grid;
  const long w;
  const long h;
};

/*---=== Contour building ===---*/

void add_ring(ContourCollector &cc, const Trax::Polyline &ring,
              const EdgeDetector *edges, Contour *&current_contour) {
  if (ring.empty())
    return;

  current_contour = cc.new_contour(); // pushed on the caller side's Lua stack

  const auto xs = ring.xcoordinates();
  const auto ys = ring.ycoordinates();

  for (std::size_t i = 0; i < xs.size(); i++) {
    const bool edge = (edges != nullptr && edges->at_edge(xs[i], ys[i]));
    current_contour->add_point(EdgePoint(xs[i], ys[i], edge));
  }
}

void build_contours(ContourCollector &cc, MyGrid &grid, float lo, float hi,
                    const EdgeDetector *edges, Contour *&current_contour) {
  Trax::IsobandLimits limits;
  limits.add(lo, hi);

  Trax::Contour contourer;
  contourer.interpolation(Trax::InterpolationType::Linear);
  contourer.closed_range(false);

  const auto results = contourer.isobands(grid, limits);

  for (const auto &geom : results) {
    for (const auto &polygon : geom.polygons()) {
      add_ring(cc, polygon.exterior(), edges, current_contour);
      for (const auto &hole : polygon.holes())
        add_ring(cc, hole, edges, current_contour);
    }
    for (const auto &line : geom.polylines())
      add_ring(cc, line, edges, current_contour);
  }
}

/*
 * Savitzky-Golay smoothening of the matrix in place.
 *
 * Smoothening length must be smaller than matrix dimensions; zero length or
 * degree means no smoothening.
 */
void smooth(MyGrid &grid, unsigned int smooth_length,
            unsigned int smooth_degree) {
  const long w = grid.width();
  const long h = grid.height();

  long length = std::min<long>({smooth_length, w - 1, h - 1});

  if (length <= 0 || smooth_degree == 0)
    return;

  const auto values = Trax::savitzky_golay(grid, w, h, length, smooth_degree);

  for (long j = 0; j < h; j++)
    for (long i = 0; i < w; i++)
      grid.set(i, j, values[i + w * j]);
}

} // namespace

/*---=== TronHintsBind ===---
 *
 * Tron hints are no longer needed by the contourer, but the (now empty) Lua
 * object is retained for backward compatibility of the Lua side API.
 */

LuaNew_ID TronHintsBind::ID;

void TronHintsBind::setup(lua_State *L) { assert(lua_istable(L, -1)); }

class TronHints : public LuaNew<TronHintsBind> {
public:
  static int is(lua_State *L) { // for 'proto.TronHints()'
    const TronHints *ll = TronHints::instance(L, 1);
    lua_pushboolean(L, ll != nullptr);
    return 1;
  }
};

/*
 */
void trax_contour(ContourCollector &cc, ContourMatrix &cm, float lo_val,
                  float hi_val, unsigned int smooth_length,
                  unsigned int smooth_degree, lua_State *L,
                  unsigned int thIndex, unsigned int &tos) {

  // If hints are requested (L is not null) and the 'TronHints' arg is nil or
  // the data will be (re)smoothened, a new TronHints object is pushed onto the
  // stack; otherwise the top of stack is adjusted down by 1 (nothing was
  // pushed) and the passed object is returned.

  if (L) {
    if ((smooth_length > 0) || !TronHints::instance(L, thIndex))
      new (L) TronHints();
    else
      tos--;
  }

  const float inf = std::numeric_limits<float>::infinity();

  std::string error;

  try {
    MyGrid grid(cm);
    Contour *current_contour = nullptr;

    if (!std::isnan(lo_val)) {
      smooth(grid, smooth_length, smooth_degree);

      EdgeDetector edges(grid);

      // Note: Range NAN,x==-inf,x and range x,32700==x,+inf

      const float lo = ((lo_val == 32700) ? -inf : lo_val);
      const float hi = (std::isnan(hi_val) ? inf : hi_val);

      if (lo < hi)
        build_contours(cc, grid, lo, hi, &edges, current_contour);

      if (current_contour && ((lo_val == 32700) || (!std::isnan(hi_val))))
        current_contour->range(true);
    } else {
      // Limits of the data (holes or grid edge)
      build_contours(cc, grid, -inf, inf, nullptr, current_contour);
    }
  } catch (const std::exception &e) {
    error = e.what();
  } catch (...) {
    error = "unknown exception";
  }

  if (!error.empty())
    luaL_error(cc.state(), "Contouring failed: %s", error.c_str());
}
