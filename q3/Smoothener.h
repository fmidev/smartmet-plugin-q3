/*
 * Smoothener.h
 *
 * Author:   Asko Kauppi, 2009
 */
#ifndef SMOOTHENER_H
#define SMOOTHENER_H

#include "Vectors.h"

#include <map>
#include <vector>

/*
 * Define for testing purposes to 'fake' a hole (missing values) within the
 * material.
 */
//#define FORCE_HOLE_TESTING

/*
 * How many points are used in smoothing.
 */
#define SMOOTHENING_WINDOW_SIZE 5 // 3/5/7

/*
 * ===== PointAndEdge =====
 *
 * Point with information, whether it's at the data edge or not.
 */
class PointAndEdge : public Vectors::Point {
private:
  typedef Vectors::Point Point;

  bool edge; // 'true' if the point is at edge of a fill path (not to be
             // smoothened) 'false' for all points in a curve path (from
             // 'Tron::line()')

public:
  PointAndEdge(const Vectors::Point &p, bool edge_)
      : Vectors::Point(p), edge(edge_) {}

  bool at_edge() const { return edge; }
};

/*
 * ===== SmoothPath =====
 *
 * Collects, smoothens, contains and draws out a path of points representing
 * a certain ISO line (either curve or closed loop).
 *
 * Points at edge of the target area are marked, so they won't be smoothened.
 */
class SmoothPath {
private:
  typedef Vectors::Point Point;

  std::vector<PointAndEdge> rim;

  bool closed; // Is the path to be closed back to first point?
               // (always is, with fills)

  const unsigned WINDOW; // 3/5/7 = points to use in the smoothening averaging
  const unsigned SKIP;   // (WINDOW-1)/2

public:
  SmoothPath()
      : rim(), closed(false), // only known at 'done()'
        WINDOW(SMOOTHENING_WINDOW_SIZE),
        SKIP((SMOOTHENING_WINDOW_SIZE - 1) / 2) {}

  ~SmoothPath() {}

  void add_point(const Point &p, bool at_edge = false) {
    rim.push_back(PointAndEdge(p, at_edge));
  }

  void done(float factor, bool closed_);

  std::vector<PointAndEdge>::const_iterator begin() { return rim.begin(); }
  std::vector<PointAndEdge>::const_iterator end() { return rim.end(); }
};

#endif
// SMOOTHENER_H
