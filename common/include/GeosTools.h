// ======================================================================
/*!
 * \brief Various GEOS related tools
 */
// ======================================================================

#pragma once

#include <geos/geom/Geometry.h>

namespace geos { namespace geom { class Geometry; } }
class PathAdapterBase;

namespace SmartMet
{
  namespace Q3GeosTools
  {
	void getContours(const geos::geom::Geometry * geom,PathAdapterBase * pathAdapter);
  }
}
