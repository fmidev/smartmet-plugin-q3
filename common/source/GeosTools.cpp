// ======================================================================
/*!
 * \brief Geos tools
 */
// ======================================================================

#include "GeosTools.h"
#include <geos/geom/Coordinate.h>
#include <geos/geom/Point.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/LineString.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/MultiPoint.h>
#include <geos/geom/MultiLineString.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/CoordinateSequence.h>
#include <boost/numeric/conversion/cast.hpp>
#include <stdexcept>

#include "PathAdapterBase.h"

using namespace geos::geom;

namespace
{
  // ----------------------------------------------------------------------
  /*!
   * \brief Handle LinearRing
   */
  // ----------------------------------------------------------------------

  void contourFromLinearRing(const LinearRing * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	for (unsigned long i=0, n=geom->getNumPoints(); i<n-1; ++i) {
		if (i == 0)
			pathAdapter->moveto(geom->getCoordinateN(boost::numeric_cast<int>(i)));
		else
			pathAdapter->lineto(geom->getCoordinateN(boost::numeric_cast<int>(i)));
	}
  }

  // ----------------------------------------------------------------------
  /*!
   * \brief Handle LineString
   */
  // ----------------------------------------------------------------------

  void contourFromLineString(const LineString * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	unsigned long n = geom->getNumPoints();

	for (unsigned long i=0; i<n; ++i) {
		if (i == 0)
			pathAdapter->moveto(geom->getCoordinateN(boost::numeric_cast<int>(i)));
		else
			pathAdapter->lineto(geom->getCoordinateN(boost::numeric_cast<int>(i)));
	}

//	if ((!(geom->isClosed())) && (n > 2))
//		pathAdapter->lineto(geom->getCoordinateN(boost::numeric_cast<int>(0)));
  }

  // ----------------------------------------------------------------------
  /*!
   * \brief Handle Polygon
   */
  // ----------------------------------------------------------------------

  void contoursFromPolygon(const Polygon * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	contourFromLineString(geom->getExteriorRing(), pathAdapter);

	for (size_t i=0, n=geom->getNumInteriorRing(); i<n; ++i)
		contourFromLineString(geom->getInteriorRingN(i),pathAdapter);
  }

  // ----------------------------------------------------------------------
  /*!
   * \brief Handle MultiLineString
   */
  // ----------------------------------------------------------------------

  void contoursFromMultiLineString(const MultiLineString * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	for (size_t i=0, n=geom->getNumGeometries(); i<n; ++i)
		contourFromLineString(dynamic_cast<const LineString *>(geom->getGeometryN(i)), pathAdapter);
  }

  // ----------------------------------------------------------------------
  /*!
   * \brief Handle MultiPolygon
   */
  // ----------------------------------------------------------------------

  void contoursFromMultiPolygon(const MultiPolygon * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	for (size_t i=0, n=geom->getNumGeometries(); i<n; ++i)
		contoursFromPolygon(dynamic_cast<const Polygon *>(geom->getGeometryN(i)), pathAdapter);
  }

  // ----------------------------------------------------------------------
  /*!
   * \brief Handle GeometryCollection
   */
  // ----------------------------------------------------------------------

  void contoursFromGeometryCollection(const GeometryCollection * geom, PathAdapterBase * pathAdapter)
  {
	if (geom == NULL || geom->isEmpty())
		return;

	for (size_t i=0, n=geom->getNumGeometries(); i<n; ++i)
		SmartMet::Q3GeosTools::getContours(geom->getGeometryN(i), pathAdapter);
  }
}

namespace SmartMet
{
  namespace Q3GeosTools
  {
	// ----------------------------------------------------------------------
	/*!
	* \brief Extract contours from geos geometry
	*/
	// ----------------------------------------------------------------------

	void getContours(const Geometry * geom,PathAdapterBase * pathAdapter)
	{
		if ( const LinearRing * lr = dynamic_cast<const LinearRing*>(geom) )
			contourFromLinearRing(lr, pathAdapter);
		else if ( const LineString * ls = dynamic_cast<const LineString*>(geom) )
			contourFromLineString(ls, pathAdapter);
		else if ( const Polygon * p = dynamic_cast<const Polygon*>(geom) )
			contoursFromPolygon(p, pathAdapter);
		else if ( const MultiLineString * ml = dynamic_cast<const MultiLineString*>(geom) )
			contoursFromMultiLineString(ml, pathAdapter);
		else if ( const MultiPolygon * mpg = dynamic_cast<const MultiPolygon*>(geom) )
			contoursFromMultiPolygon(mpg, pathAdapter);
		else if ( const GeometryCollection * g = dynamic_cast<const GeometryCollection*>(geom) )
			contoursFromGeometryCollection(g, pathAdapter);
		else if ( dynamic_cast<const Point*>(geom) )
			;
		else if ( dynamic_cast<const MultiPoint*>(geom) )
			;
		else
			throw std::runtime_error("Encountered an unsupported GEOS geometry component");
	}
  }
}
