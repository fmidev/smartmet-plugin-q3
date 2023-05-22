/*
 * Vectors.h
 *
 * Vector support for Labelizer.
 *
 * Note: NOT NEEDED if Boost Linear Algebra library is being used:
 *       http://www.boost.org/doc/libs/1_37_0/libs/numeric/ublas/doc/overview.htm
 *
 *       Only available with Boost >= 1.37; Labelizer code not adopted to it,
 * yet.
 *
 * Defines:
 *       VECTORS_CACHE_NORM  Caches the norm calculations. Don't use this;
 *           tests indicate it actually slows down Q2 plugin.    --AKa
 * 27-Jan-2009
 *
 * Author:   Asko Kauppi, 2008-09
 */
#ifndef VECTORS_H
#define VECTORS_H

#include <assert.h>
#include <math.h>
#include <values.h>
// MAXFLOAT

#include <stdexcept>

namespace Vectors {

class Vector;

/*
 * Represents a single (x,y) point
 */
struct Point {
  double x, y;

  Point(double x_, double y_) : x(x_), y(y_) {}
  Point(const Point &o) = default;
  Point() : x(0.0), y(0.0) {}

  Point& operator = (const Point&) = default;

  static constexpr double epsilon = 0.0000001;

  static bool eq_epsilon(double a, double b) { return fabs(a - b) <= epsilon; }

  bool operator==(const Point &o) const {
    return eq_epsilon(x, o.x) && eq_epsilon(y, o.y);
  }

  void operator+=(const Point &o) {
    x += o.x;
    y += o.y;
  }
  void operator-=(const Point &o) {
    x -= o.x;
    y -= o.y;
  }

  void operator*=(double f) {
    x *= f;
    y *= f;
  }
  void operator/=(double f) {
    x /= f;
    y /= f;
  }

  Point operator+(const Point &o) const { return Point(x + o.x, y + o.y); }
  Point operator-(const Point &o) const { return Point(x - o.x, y - o.y); }

  void operator+=(const Vector &v);
  void operator-=(const Vector &v);

  Point operator+(const Vector &v) const;
  Point operator-(const Vector &v) const;

  Point operator/(double d) const { return Point(x / d, y / d); }
};

/*
 * Represents a single (dx,dy) vector, with no particular starting point.
 */
class Vector {
private:
  double dx, dy;
#ifdef VECTORS_CACHE_NORM
  mutable double norm_cached; // -1: not cached (needs to be calculated)
                              // >=0: cached (use this value)
#endif

  static int SIGN(double v) { return (v < 0.0) ? -1 : (v > 0.0) ? +1 : 0; }

#ifdef VECTORS_CACHE_NORM
  Vector(double dx_, double dy_, double norm_cached_)
      : dx(dx_), dy(dy_), norm_cached(norm_cached_) {}

  static double norm_raw(double dx_, double dy_) {
    return sqrt(dx_ * dx_ + dy_ * dy_);
  }
#endif

public:
#ifdef VECTORS_CACHE_NORM
  Vector() : dx(0.0), dy(0.0), norm_cached(0.0) {}
  Vector(const Vector &o) : dx(o.dx), dy(o.dy), norm_cached(o.norm_cached) {}

  Vector(double dx_, double dy_) : dx(dx_), dy(dy_), norm_cached(-1.0) {}
  Vector(const Point &p) : dx(p.x), dy(p.y), norm_cached(-1.0) {}
  Vector(const Point &a, const Point &b)
      : dx(b.x - a.x), dy(b.y - a.y), norm_cached(-1.0) {}
#else
  Vector() : dx(0.0), dy(0.0) {}
  Vector(const Vector &o) : dx(o.dx), dy(o.dy) {}

  Vector(double dx_, double dy_) : dx(dx_), dy(dy_) {}
  Vector(const Point &p) : dx(p.x), dy(p.y) {}
  Vector(const Point &a, const Point &b) : dx(b.x - a.x), dy(b.y - a.y) {}
#endif

  Vector operator*(double f) const { return Vector(f * dx, f * dy); }

  double get_dx() const { return dx; }
  double get_dy() const { return dy; }

  bool is_null() const { return (dx == 0.0) && (dy == 0.0); }

  double norm() const {
#ifdef VECTORS_CACHE_NORM
    if (norm_cached < 0.0) {
      norm_cached = sqrt(dx * dx + dy * dy);
    }
    return norm_cached;
#else
    return sqrt(dx * dx + dy * dy);
#endif
  }

  double dot(const Vector &o) const { return (dx * o.dx) + (dy * o.dy); }

  /*
   * Cross product's z component
   *
   * This is used for knowing, whether a point lies right (<0.0),
   * left (>0.0) or on the line itself (==0.0).
   */
  double cross_z(const Vector &o) const {
    double z = dx * o.dy - dy * o.dx;
    if (fabs(z) < Point::epsilon)
      return 0.0; // so close to zero, we'll tell it's zero
    else
      return z;
  }

  /*
   * Cosine of the vector's angle from horizontal plane (-1.0 .. +1.0)
   *
   * Horizontal plane gives +-1, vertical gives 0.
   *
   * Null vector gives NAN.
   */
  double cos() const {
    double n = norm();
    if (n == 0.0)
      return NAN;

    return dx / n;
  }

  /*
   * Cosine of the angle between two vectors (-1.0 .. +1.0)
   *
   * +1.0: same direction
   * -1.0: opposite direction
   * 0.0: perpendicular
   * NAN: either vector is a null vector
   */
  double cos(const Vector &o) const {
    double divider = norm() * o.norm();
    if (divider == 0.0)
      return NAN;

    return dot(o) / divider;
  }

  /*
   * Vector's angle from horizontal plane (positive counter-clockwise)
   *
   * Null vector gives NAN.
   */
  double rad() const {
    double n = norm();
    if (n == 0.0)
      return NAN;

    // 'acos()' gives the positive angles only (returns 0..PI); if
    // 'dy' is negative we need to negate the result.
    //
    return acos(dx / n) * SIGN(dy);
  }

#ifdef LABELIZER_SELFTEST_ENABLE
  static void selftest();
#endif
};

/*
 * Represents a vector with both starting point ('p') and direction.
 */
class PointAndVector : public Vector {
public:
  Point p;

  PointAndVector(const Point &a, const Point &b) : Vector(a, b), p(a) {}
  PointAndVector(const Point &p_, const Vector &v) : Vector(v), p(p_) {}

  PointAndVector(const PointAndVector &o) : Vector(o), p(o.p) {}

  // We don't really WANT this constructor to be there - we don't NEED
  // it but the COMPILER does. --AKa 19-Jan-2009
  //
  PointAndVector() : Vector(), p() {}

  Point end() const { return Point(p.x + get_dx(), p.y + get_dy()); }

  bool intersects(const Point &) const;
  bool intersects(const PointAndVector &) const;

#ifdef LABELIZER_SELFTEST_ENABLE
  static void selftest();
#endif
private:
  bool judge_by_projection(const PointAndVector &other) const;
};

/*
 * Bounding box rectangle
 */
class BoundingBox {
public:
  Point lo, hi;

  BoundingBox(const Point &lo_, const Point &hi_) : lo(lo_), hi(hi_) {}
  BoundingBox(const PointAndVector &pv) : lo(), hi() {

    // Make a bounding box for the PointAndVector.
    //
    if (pv.get_dx() < 0.0) {
      lo.x = pv.p.x;
      hi.x = pv.p.x + pv.get_dx();
    } else {
      lo.x = pv.p.x + pv.get_dx();
      hi.x = pv.p.x;
    }

    if (pv.get_dy() < 0.0) {
      lo.y = pv.p.y;
      hi.y = pv.p.y + pv.get_dy();
    } else {
      lo.y = pv.p.y + pv.get_dy();
      hi.y = pv.p.y;
    }
  }

  BoundingBox(const BoundingBox &o) : lo(o.lo), hi(o.hi) {}

  // Default bounding box has no bounds; calling 'extend()' will make
  // them.
  //
  BoundingBox() : lo(MAXFLOAT, MAXFLOAT), hi(-MAXFLOAT, -MAXFLOAT) {}

  void extend(const Point &p) {
    // Note: need to check against both min&max; first point resets them
    //       both.
    //
    if (p.x < lo.x)
      lo.x = p.x;
    if (p.x > hi.x)
      hi.x = p.x;
    if (p.y < lo.y)
      lo.y = p.y;
    if (p.y > hi.y)
      hi.y = p.y;
  }

  // TBD: These should probably be done using 'Point::epsilon' margins
  //
  bool inside_or_at_edge(const Point &p) const {
    return (p.x >= lo.x) && (p.x <= hi.x) && (p.y >= lo.y) && (p.y <= hi.y);
  }

  bool inside_or_at_edge(const BoundingBox &o) const {
    return (o.lo.x >= lo.x) && (o.hi.x <= hi.x) && (o.lo.y >= lo.y) &&
           (o.hi.y <= hi.y);
  }

  bool outside_or_at_edge(const Point &p) const {
    return ((p.x <= lo.x) || (p.x >= hi.x)) && ((p.y <= lo.y) || (p.y >= hi.y));
  }

  /*
   * Returns 'true' if the two rectangles don't overlap, or even touch each
   * other at an edge or corner. Used for eliminating unnecessary
   * intersection tests.
   */
  bool detached(const BoundingBox &o) const {
    return ((lo.x > o.hi.x) || (hi.x < o.lo.x)) ||
           ((lo.y > o.hi.y) || (hi.y < o.lo.y));
  }

  double width() const { return hi.x - lo.x; }
  double height() const { return hi.y - lo.y; }

  double area() const { return (hi.x - lo.x) * (hi.y - lo.y); }
};
}; // namespace Vectors

#endif
   // VECTORS_H
