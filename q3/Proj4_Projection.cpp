/*
 * PROJ4_PROJECTION.CPP                          Copyright (c) 2010, Ilmatieteen
 * laitos
 *
 * Ref. http://trac.osgeo.org/proj/wiki/ProjAPI
 */
#include "Proj4_Projection.h"

#include "Tools.h"

using namespace std;

/*
    Matti.Horttanainen@fmi.fi 4-Nov-2010 on which LatLon projection to use (in
FMI):
<<
    Moi
    Koska meillä on Pallo, olen käyttänyt tuollaista
    +proj=longlat +ellps=sphere +a=6371220 +b=6371220

    a ja b tuntuvat vähän vaihtelevan...

    Joskus muunnoksissa on hyvä käyttää myös

    +nadgrids=@null

    http://osgeo-org.1803224.n2.nabble.com/Difference-between-nadgrids-null-and-towgs84-0-0-0-td4543036.html
    http://proj.maptools.org/faq.html#sphere_as_wgs84

    Matti
<<
*/
const char *LATLON_DECL = "+proj=longlat +ellps=sphere +a=6371220 +b=6371220";

/*---=== Proj4_Projection ===---*/

/*
 * Proj4 projection syntax is defined by 'pj_init_plus()' API.
 *
 * Samples:
 *   "+proj=utm +zone=11 +ellps=WGS84"
 *   "+proj=merc +ellps=clrk66 +lat_ts=33"
 *   "+proj=tmerc +lon_0 +datum=WGS84"
 *
 * Ref:
 *   http://trac.osgeo.org/proj/wiki/GenParms
 */

/*
 */
Proj4_Projection::Proj4_Projection(const char *proj_)
    : Projection_provider(), pj(Fmi::CoordinateTransformation("WGS84", proj_)) {
  assert(proj_);

  INVARIANT();
}

/*
 */
/*virtual*/ Proj4_Projection::Proj4_Projection(const Proj4_Projection &o)
    : Projection_provider(), pj(o.pj) {

  INVARIANT();
}

/*
 */
Proj4_Projection::~Proj4_Projection() { ; }

/*
 * Get the relative grid coordinates (0..1, 0..1) within the projection for
 * point 'll'.
 *
 * Returns 'true' if inside the projection (or at the rim); 'false' if outside.
 */
/*virtual*/ bool Proj4_Projection::at(const LatLon &ll, double &dx,
                                      double &dy) const {

  double x_array = ll.getLat() * DEG_TO_RAD;
  double y_array = ll.getLon() * DEG_TO_RAD;

  bool st = pj.transform(x_array, y_array);

  if (!st) {
    throw E_LOG_ERROR("Proj4 transform error: %s", "at()");
  }

  dx = x_array;
  dy = y_array;

  return true;
}

/*
 */
/*virtual*/ LatLon Proj4_Projection::latlon(double dx, double dy) const {

  double x_array = dx;
  double y_array = dy;
  bool st = false;

  try {
    auto pjll = Fmi::CoordinateTransformation("WGS84", LATLON_DECL);

    st = pjll.transform(x_array, y_array);
  } catch (...) {
    st = false;
  }

  if (!st) {
    throw E_LOG_ERROR("Proj4 transform error: %s", "latlon()");
  }

  return LatLon(x_array * RAD_TO_DEG, y_array * RAD_TO_DEG);
}
