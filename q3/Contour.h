/*
 * CONTOUR.H                               Copyright (c) 2010, Ilmatieteen
 * laitos
 */
#ifndef CONTOUR_H
#define CONTOUR_H

#include "LuaNew.h"

#include "MemMatrix.h" // 08-Dec-2011 PKi: Using MemMatrix; smoothening changes the data
#include "Tools.h"

#include <math.h>

#include "cairo.h"
#include <vector>

class Contour;
struct EdgePoint; // keep as a separate class since be bind it to Lua
                  // (not as 'Contour::EdgePoint)

/*---=== EdgePoint ===---
 */
struct EdgePointBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "EdgePoint"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef EdgePoint CAST_T;

private:
  static int __index(lua_State *L);
  static int __tostring(lua_State *L);
};

/*
 * Point anywhere within the matrix (not only in integer coordinates)
 *
 * Can also be used as a vector (vector from origin to the given point).
 */
struct Point {
  double x, y;

  Point() : x(0.0), y(0.0) {}
  Point(double x_, double y_) : x(x_), y(y_) {}
  Point(const Point &o) : x(o.x), y(o.y) {}

  Point operator+(const Point &o) const { return Point(x + o.x, y + o.y); }
  Point operator-(const Point &o) const { return Point(x - o.x, y - o.y); }
  Point operator*(double d) const { return Point(x * d, y * d); }
  Point operator/(double d) const { return Point(x / d, y / d); }

  Point &operator+=(const Point &o) {
    x += o.x;
    y += o.y;
    return *this;
  }
  Point &operator/=(double d) {
    x /= d;
    y /= d;
    return *this;
  }

  bool operator==(const Point &o) const {
    return (fabs(x - o.x) < EPSILON) && (fabs(y - o.y) < EPSILON);
  }

  double cross_z(const Point &o) const { return x * o.y - y * o.x; }
  double dot(const Point &o) const { return x * o.x + y * o.y; }
  double norm_pow2() const { return x * x + y * y; }

private:
  static constexpr double EPSILON = 1e-6;
};

struct EdgePoint : public Point, LuaNew<EdgePointBind> {
  // '.edge' provided publically; we're all family and only few places need
  // this.
  //
  bool edge; // at material edge (or hole edge); do not stroke two such points

  EdgePoint() : Point(), edge(false) {}
  EdgePoint(double x_, double y_, bool edge_ = false)
      : Point(x_, y_), edge(edge_) {}
  EdgePoint(const Point &p, bool edge_) : Point(p), edge(edge_) {}
};

/*---=== Contour ===---
 */
struct ContourBind {
public:
  static LuaNew_ID ID; // the unique key
  static void setup(lua_State *L);
  static const char *name() { return "Contour"; }
  static const char *env_mode() { return nullptr; }
  static const LuaNew_ID &id() { return ID; }
  typedef Contour CAST_T;

private:
  static int __index(lua_State *L);
  static int __len(lua_State *L);
};

class Contour : public LuaNew<ContourBind> {
public:
  Contour() : points(), offset((unsigned)-1), brange(false) { INVARIANT(); }

  void add_point(const EdgePoint &p) { points.push_back(p); }

  // size_t size() const { return points.size(); }

  static int contour(lua_State *L);
  static int contour_smoothen_one(lua_State *L);
  static int calc_slants(lua_State *L);

  // 21-Nov-2011 PKi: Draw, fill and labelize contours
  void draw_path(cairo_t *cr, bool line = true) const;
  static int drawcontours(lua_State *L);
  bool range() { return brange; }
  void range(bool br) { brange = br; }

#ifdef USE_TRON
  bool at_edge(double x, double y) const; // TRON_MODE==2 needs this
#endif

  void set_offset(); // to be called when all points are there, before pushing
                     // the contour to Lua

private:
  friend class ContourBind;

  double area() const;
  void stretch_out(double factor);

  // data members
  std::vector<EdgePoint> points;
  unsigned offset; // offset of the first point ([1]) as seen [1] in Lua (after
                   // 'set_offset()' call).
  bool brange;     // set if contour was fetched using lo/hi range

#ifndef NDEBUG
  void _INVARIANT(const char *file, unsigned line) const {

    if (offset != ((unsigned)-1)) {
      // 'set_offset()' has been called ('points' is not being added to any
      // more, and the contour can be accessed from Lua).

      unsigned n = points.size();

      assert_invariant(n > 1); // must have at least two points
      assert_invariant(offset < n);

      // If point at 'offset' has 'edge'==false, it must be a closed contour (no
      // edges, at all)
      //
      if (!points[offset].edge) {
        for (unsigned i = 0; i < n;
             i++) { // no need to use 'offset' for this check
          assert_invariant(!points[i].edge);
        }

      } else {
        // If next point has 'edge'==false, the whole contour is edge only (may
        // be useful for fillings, at times).
        //
#if 1
        if (points[(offset + 1) % n].edge) {
          for (unsigned i = 0; i < n;
               i++) { // no need to use 'offset' for this check
            assert_invariant(points[i].edge);
          }
        }
#else
        // The point following 'offset' must have 'edge'==false, making 'offset'
        // the beginning of a section.
        //
        assert_invariant(!(points[(offset + 1) % n].edge));
#endif
      }
    }
    (void)file;
    (void)line;
  }
#endif
};

/*---=== Contouring adapters ===---
 */

/*
 * This class functions as the tunnel between 'Contour.cpp' and any adapters
 * providing the actual contour calculation (s.a. FMI Tron).
 */
class ContourCollector {
public:
  ContourCollector(lua_State *L_) : L(L_) {}

  Contour *new_contour() { return new (L) Contour(); }

private:
  lua_State *L; // copy of the Lua state pointer
};

/*
 */
class ContourMatrix {
public:
  ContourMatrix(MemMatrix *m_) : m(*m_) {}

  MatrixPos::xy_t getXS() const { return m.getSize().getXS(); }
  MatrixPos::xy_t getYS() const { return m.getSize().getYS(); }

  // 08-Dec-2011 PKi: Return reference to data value; smoothening changes the
  // data
  //
  float &getValue(const MatrixPos &pos) /*const 08-Dec-2011 PKi*/ {
    return m[pos];
  }

  // 12-Mar-2012 PKi: To get data min/max values
  //
  float min() { return m.reduce_min(); }
  float max() { return m.reduce_and_store_max(); }

private:
  // 08-Dec-2011 PKi: Using MemMatrix; smoothening changes the data
  //
  // 05-Jun-2012 PKi: Now using the matrix on the stack
  //
  // const Matrix &m;
  MemMatrix &m;
};

#ifdef USE_TRON
void tron_contour(ContourCollector &cc,
                  /*const 08-Dec-2011 PKi*/ ContourMatrix &cm, float lo_val,
                  float hi_val, unsigned int smooth_length,
                  unsigned int smooth_degree, lua_State *L,
                  unsigned int thIndex, unsigned int &tos);
#endif

#endif
// CONTOUR_H
