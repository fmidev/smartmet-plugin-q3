/*
 * PATHADAPTERBASE.H
 */
#ifndef PATHADAPTERBASE_H
#define PATHADAPTERBASE_H

namespace geos {
namespace geom {
class Coordinate;
}
} // namespace geos

class PathAdapterBase {
public:
  virtual void moveto(const geos::geom::Coordinate &coordinate) = 0;
  virtual void lineto(const geos::geom::Coordinate &coordinate) = 0;
};

#endif // PATHADAPTERBASE_H
