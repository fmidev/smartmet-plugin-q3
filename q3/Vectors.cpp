/*
 * Vectors.cpp
 *
 * Vector support for Labelizer.
 *
 * Changes:
 *   AKa 12-Dec-2008: Taken apart from Labelizer sources.
 */

#include "Vectors.h"

// 29-Dec-2011 PKi: LogTools.h instead of QDLog.h
//
//#include "QDLog.h"
#include "LogTools.h"

#ifdef SELFTEST_ENABLE
#include <math.h>
#include <stdexcept>
#endif

using namespace std;

namespace Vectors {

#ifdef SELFTEST_ENABLE
static class selftester {
public:
  selftester() { // Executed at process start (once)
    Vector::selftest();
    PointAndVector::selftest();
  }
} _noname;
#endif

void Point::operator+=(const Vector &v) {
  x += v.get_dx();
  y += v.get_dy();
}
void Point::operator-=(const Vector &v) {
  x -= v.get_dx();
  y -= v.get_dy();
}

Point Point::operator+(const Vector &v) const {
  return Point(x + v.get_dx(), y + v.get_dy());
}
Point Point::operator-(const Vector &v) const {
  return Point(x - v.get_dx(), y - v.get_dy());
}

/*
 * Return 'true' if the ranges [a..a+da] and [b..b+db] do not touch.
 */
static inline bool detached(double a, double da, double b, double db) {

  double a_min, a_max;

  if (da >= 0.0) {
    a_min = a;
    a_max = a + da;
  } else {
    a_min = a + da;
    a_max = a;
  }

  return (((b < a_min) && (b + db < a_min)) ||
          ((b > a_max) && (b + db > a_max)));
}

/*
 * Check if a point is within a line.
 */
bool PointAndVector::intersects(const Point &p2) const {

  Vector v(p, p2);
  if (cross_z(v) != 0.0)
    return false; // either left/right of the line (uses a tolerance internally)

  double d = dot(v); // norm() * (projection of 'p2' on 'this' vector)

  if ((d >= 0.0 && d <= norm()))
    return true; // Within the length of the line

  return false;
}

/*
 * Is 'v' within the range (ends inclusive) a..a+da?  'da' may be negative
 */
static inline bool within(double v, double a, double da) {
  if (da >= 0.0) {
    return (v >= a) && (v <= a + da);
  } else {
    return (v >= a + da) && (v <= a);
  }
}

/*
 * Judge whether 'other' is overlapping our line, by looking at the projection
 * of its end points.
 *
 * This is used INTERNALLY ONLY in cases where the two lines (with their
 * extensions) are known to be same.
 */
bool PointAndVector::judge_by_projection(const PointAndVector &other) const {
  double dot1 = dot(Vector(p, other.p));
  double dot2 = dot(Vector(p, other.end()));
  double _norm = norm();

  return (((dot1 >= 0.0) || (dot2 >= 0.0)) &&
          ((dot1 <= _norm) || (dot2 <= _norm)));
}

/*
 * Check whether two space vectors intersect (within their length, inclusive
 * end points or just touching each other at one point).
 *
 * Note: This is normally called _after_ bounding boxes have been considered.
 *       So we know the vectors are close enough, and shouldn't recheck such
 *       things for optimization.
 */
bool PointAndVector::intersects(const PointAndVector &other) const {

  double _norm = norm();
  double _o_norm = other.norm();

  /*
   * Sanity check: null vectors is basically asking, if points intersect
   * (are the same point, or are among the line)
   */
  if (fabs(_norm) < Point::epsilon) {
    if (fabs(_o_norm) < Point::epsilon) {
      return p == other.p; // intersects if the points are the same
    } else {
      return other.intersects(this->p);
    }
  } else if (_o_norm == 0.0) {
    return intersects(other.p);
  }

  /*
   * Calculate crossing of 'other' (or its extension) on our line (or its
   * extension)
   */
  double dx = get_dx(), dy = get_dy();
  double o_dx = other.get_dx(), o_dy = other.get_dy();

  if (fabs(dx) < Point::epsilon) {
    if (fabs(o_dx) < Point::epsilon) {
      /*
       * Both vectors are vertical
       */
      if (p.x == other.p.x) {
        return judge_by_projection(other); // along the same line
      }
    } else {
      /*
       * Only 'this' is vertical ('other' is not)
       * Find out the crossing and see if it's within the vectors.
       */
      if (within(p.x, other.p.x, o_dx)) {
        double y =
            other.p.y +
            (p.x - other.p.x) *
                (o_dy / o_dx); // where 'other' crosses this line (p.x, y)
        return within(y, p.y, dy);
      }
    }

  } else if (fabs(o_dx) < Point::epsilon) {
    /*
     * Other vector is vertical
     */
    return other.intersects(*this); // twist around

  } else {
    /*
     * Neither 'this' or 'other' are vertical
     *
     * Joining the line equations (of type 'y = b + (x-a)(dy/dx)', where
     * (a,b) is the point and (dy/dx) the vector) gives:
     *
     *      (b'-b)dxdx' + adx'dy - a'dxdy'
     * x = --------------------------------
     *               dx'dy - dxdy'
     */
    /*
     * Possible optimization: turn out anything where 'other' start and end
     * points are on the same side of us.
     *
     * NOTE: This does not really speed things up, so disabled.
     *       --AKa 29-Jan-2009
     */
#if 0
    {
    Vector v1( p, other.p );
    Vector v2( p, other.end() );
    
    double z1= cross_z(v1);
    double z2= cross_z(v2);
    
    if ( ((z1<0.0) && (z2<0.0)) || ((z1>_norm) || (z2>_norm)) )
        return false;
    }
#endif

    double divident =
        (other.p.y - p.y) * dx * o_dx + p.x * o_dx * dy - other.p.x * dx * o_dy;

    double divider = o_dx * dy - dx * o_dy;

    if (divider == 0.0) {
      // The lines are parallel; either 0/1/infinite points of intersection
      //
      double y = p.y - p.x * (dy / dx); // crosses Y axis at
      double o_y = other.p.y - other.p.x * (o_dy / o_dx);

      if (fabs(y - o_y) > Point::epsilon)
        return false; // separate lines

      // Same line; use dot product (projection) of 'other' start and
      // end points.
      //
      return judge_by_projection(other);

    } else {
      // If the crossing point is within 'this' and 'other' lines,
      // it's a hit.
      //
      double x = divident / divider;

      if (within(x, p.x, dx) && within(x, other.p.x, o_dx)) {
        return true; // YES, they do intersect
      }
    }
  }
  return false;
}

/*
 * Selftest of the 'Vector' class
 *
 * Use
 * <http://www.math.jyu.fi/ylemat/opetusmateriaalia/havainnollistuksia/vektorilaskentaa/30_pistetulo.html>
 * to play with vectors interactively.
 */
#ifdef SELFTEST_ENABLE
static void _assert_eps(double a, double b, unsigned line) {
  if (fabs(a - b) >= Point::epsilon) {
    throw runtime_error(
        LOG_BUG("Selftest failed on line %d (%lf != %lf)\n", line, a, b));
  }
}
#define assert_eps(a, b) _assert_eps(a, b, __LINE__)

static void _assert_gt(double a, double b, unsigned line) {
  if (a <= b) {
    throw runtime_error(
        LOG_BUG("Selftest failed on line %d (%lf <= %lf)\n", line, a, b));
  }
}
#define assert_gt(a, b) _assert_gt(a, b, __LINE__)

static void _assert_lt(double a, double b, unsigned line) {
  if (a >= b) {
    throw runtime_error(
        LOG_BUG("Selftest failed on line %d (%lf >= %lf)\n", line, a, b));
  }
}
#define assert_lt(a, b) _assert_lt(a, b, __LINE__)

void Vector::selftest() {
  Vector a(Point(5, 0));
  Vector d(Point(-5, -2));

  assert_eps(a.norm(), 5);
  assert_eps(d.norm(), sqrt(5 * 5 + 2 * 2));

  double cos_a = 1.0; // horizontal
  double cos_d = (-5.0) / d.norm();

  assert_eps(a.cos(), cos_a);
  assert_eps(d.cos(), cos_d);

  // fprintf( stderr, "cos: %lf %lf\n", d.cos(a), acos(d.cos(a)) * 180.0/M_PI );
  double theta = 158.198591 / 180.0 * M_PI;
  double cos_ad = ::cos(theta);

  assert_eps(a.cos(a), 1.0);
  assert_eps(a.cos(d), cos_d);
  assert_eps(d.cos(a), cos_d);
  assert_eps(d.cos(d), 1.0);

  assert_eps(d.cos(Vector(Point(-2, 5))), 0.0);

  assert_eps(a.dot(a), a.norm() * a.norm());
  assert_eps(a.dot(d), a.norm() * d.norm() * cos_ad);
  assert_eps(d.dot(a), d.norm() * a.norm() * cos_ad);
  assert_eps(d.dot(d), d.norm() * d.norm());

  assert_eps(a.cross_z(a), 0.0);
  assert_lt(a.cross_z(d), 0.0);
  assert_gt(d.cross_z(a), 0.0);

  assert_eps(Vector(Point(1, 0)).rad(), 0.0);
  assert_eps(Vector(Point(1, 1)).rad(), M_PI / 4.0);
  assert_eps(Vector(Point(0, 1)).rad(), M_PI / 2.0);
  assert_eps(Vector(Point(-1, 1)).rad(), M_PI * 3 / 4.0);
  assert_eps(Vector(Point(0, -1)).rad(), -M_PI / 2.0);
}
#endif

/*
 * Selftest of the 'PointAndVector' class
 */
#ifdef SELFTEST_ENABLE
void PointAndVector::selftest() {

  PointAndVector a1(Point(3, 3), Point(7, 6));
  PointAndVector a2(Point(2, -2), Point(6, 1)); // same direction as a1

  PointAndVector b1(Point(6, -3), Point(5, -1));
  PointAndVector b2(Point(5, -1), Point(4, 1));
  PointAndVector b3(Point(4, 1), Point(3, 3));

  PointAndVector c(Point(5.5, -2), Point(3.5, 2));

  assert(a1.intersects(a1));
  assert(!a1.intersects(a2));
  assert(!a1.intersects(b1));
  assert(!a1.intersects(b2));
  assert(a1.intersects(b3));
  assert(!a1.intersects(c));

  assert(!a2.intersects(a1));
  assert(a2.intersects(a2));
  assert(!a2.intersects(b1));
  assert(a2.intersects(b2));
  assert(!a2.intersects(b3));

  assert(!b1.intersects(a1));
  assert(!b1.intersects(a2));
  assert(b1.intersects(b1));
  assert(b1.intersects(b2));
  assert(!b1.intersects(b3));
  assert(b1.intersects(c));

  assert(!b2.intersects(a1));
  assert(b2.intersects(a2));
  assert(b2.intersects(b1));
  assert(b2.intersects(b2));
  assert(b2.intersects(b3));
  assert(b2.intersects(c));

  assert(b3.intersects(a1));
  assert(!b3.intersects(a2));
  assert(!b3.intersects(b1));
  assert(b3.intersects(b2));
  assert(b3.intersects(b3));
  assert(b3.intersects(c));

  assert(!c.intersects(a1));
  assert(c.intersects(a2));
  assert(c.intersects(b1));
  assert(c.intersects(b2));
  assert(c.intersects(b3));
}
#endif

} // namespace Vectors
