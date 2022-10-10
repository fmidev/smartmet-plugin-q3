// ======================================================================
/*!
 * \brief Various GEOS related tools
 */
// ======================================================================

#pragma once

namespace geos {
namespace geom {
class Geometry;
}
} // namespace geos
class PathAdapterBase;

namespace SmartMet {
namespace Q3GeosTools {
void getContours(const geos::geom::Geometry *geom,
                 PathAdapterBase *pathAdapter);
}
} // namespace SmartMet
