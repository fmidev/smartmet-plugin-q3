/*
 * Labelizer.h
 *
 * Determining places for labels on contour lines.
 */
#ifndef LABELIZER_H
#define LABELIZER_H

#include <string>
#include <vector>

#include <values.h>

#include "Vectors.h"
// 29-Dec-2011 PKi: Tools.h instead of Profiler_ms.h
//
//#include "Profiler_ms.h"
#include "Tools.h"

#include "ContourInfo.h"

// 20-Nov-2011 PKi: Conflict with class
//
#undef ContourMatrix

/*
 */
class Labelizer {
public:
  typedef Vectors::Point Point;
  typedef Vectors::BoundingBox BoundingBox;

  /*
   * Label name & size & drawing parameters
   *
   * Using the 'lp' as a pointer is risky, but we know the specs remain
   * stable throughout our lifespan. Tried it without, as a copy, but C++
   * was starting to be bitchy about it... --AKa 3-Feb-2009
   */
  struct FontSpec {
    // Note: Cannot declare these 'const' because then it blocks use at
    //       vectors. What's the point of 'const' when C++ pro-actively
    //       works against it, anyways... :&    --AKa 3-Feb-2009
    //
    /*const*/ std::string label;
    /*const*/ Vectors::Vector size; // width & height in data coordinates
    const ContourInfo_Line::LabelParams *lp;

    FontSpec(const std::string &label_, const Vectors::Vector &size_,
             const ContourInfo_Line::LabelParams *lp_)
        : label(label_), size(size_), lp(lp_) {}
    FontSpec(const FontSpec &other)
        : label(other.label), size(other.size), lp(other.lp) {}
    FontSpec &operator=(const FontSpec &) = default;
  };

  class TiltedRect {
  private:
    typedef Vectors::PointAndVector PointAndVector;

    /* 1---2
     *  | c |
     *  0---3    Note that the box can be tilted, too (hard to show in ASCII)
     */
    // Cannot have 'const' here due to our own constructor (not able to set
    // up 'side[0..3]' in constructor list).    --AKa 19-Jan-2009
    //
    /*const*/ PointAndVector side[4];

    // Cannot have 'const' here, either, due to default 'operator=()' not
    // liking a const.  --AKa 19-Jan-2009
    //
    /*const*/ Point center;

    /*const*/ BoundingBox bb_cached; // cached for speed

  public:
    TiltedRect(const Point &p, const Vectors::Vector &size, double tilt_rad_);

    bool intersects(const TiltedRect &other) const;
    bool intersects(const PointAndVector &) const;

    const Point &get_center() const { return center; }
    const Point &get_corner(unsigned i) const { return side[i].p; }
    const PointAndVector &get_side(unsigned i) const { return side[i]; }

    const BoundingBox &get_bounding_box() const { return bb_cached; }

  private:
    bool contains(const Point &) const;
  };

  /*
   * Callback object; derive from this and make an implementation for
   * drawing out the actual labels.
   */
  class Drawer {
  public:
    virtual ~Drawer() {}

    virtual void label(const Point &p, const std::string &label,
                       const ContourInfo_Line::LabelParams &label_specs,
                       double tilt_rad) = 0;

    // for debugging (need not be overridden)
    //
    // Note: need to use unique names ('mark_xxx' etc.) in case not all of these
    //       are defined in a deriving class (some C++ feeeeeeature)
    //
    virtual void mark_candidate(const Vectors::Point &p, double goodness,
                                const char *str = 0) {}
    virtual void mark_tr(const TiltedRect &tr, const char *str = 0) {}
    virtual void mark_bb(const BoundingBox &bb, const char *str = 0) {}
    virtual void mark_vector(const Vectors::PointAndVector &v,
                             const char *str = 0) {}
  };

  struct Config {
    double limit_along_line; // min. distance between labels on the same curve
                             // (center to center)
    double limit_direct; // min. distance to any other label (center to center)

    bool allow_overlapping_labels; // can labels overlap each other (faster)
    bool allow_overlapping_lines; // can labels overlap other iso lines (faster)

    double
        dampen_corners_limit_deg; // corners bigger than this will get dampened
    double dampen_corners;        // damping factor (<1.0) for such corners

    double boost_horizontal_limit_deg; // lines less than this are taken as
                                       // horizontal
    double boost_horizontal; // boost factor (>1.0) for such (horizontal) points

    double curve_optimization; // Optimization fine tuning; affects processing
                               // speed How big areas of curves are stored as
                               // subcurves (chunks); in pixels

    int decimals; // Number of label decimals

    // Struct has default values; the rest can be set explicitly by the
    // application using it.
    //
    Config()
        : limit_along_line(0.0), limit_direct(0.0),
          allow_overlapping_labels(false), allow_overlapping_lines(false),
          dampen_corners_limit_deg(8.0), dampen_corners(0.8),
          boost_horizontal_limit_deg(3.0), boost_horizontal(1.5),
          curve_optimization(0.0), // 0 = not used (all one chunk; slooooow)
          decimals(1) {}
  };

  Labelizer(const Config &config_, Drawer &debug_drawer_, unsigned data_w,
            unsigned data_h)
      : config(config_), current_chain(0), chains(), last_point(),
        debug_drawer(debug_drawer_),
        bb(Point(0, 0), Point(data_w - 1, data_h - 1)) {}

  ~Labelizer();

  void add_point(const Point &xy, const FontSpec &fs);
  void add_point(const Point &xy);
  void close();

  std::vector<TiltedRect> done(double scale);

  void draw_labels(Drawer &drawer);

  class Chain;

  // 02-Dec-2011 PKi: Labelizer configuration
  //
  static const Labelizer::Config &loadDefaultCfg(lua_State *L, int idx);
  static const Labelizer::Config &loadUserCfg(lua_State *L, int idx,
                                              Labelizer::Config &lCfg);
  static const Labelizer::Config &
  loadCfg(lua_State *L, int idx, Labelizer::Config &lCfg, bool uConfig);
  static int pushLabelizerCfg(lua_State *L);

  // 20-Dec-2011 PKi: Labelizer configuration field table. Must be in sync with
  // Labelizer::Config
  //
  //			   TODO: Union for field pointers
  //
  typedef enum { T_NONE = 0, T_DOUBLE, T_BOOL, T_INT } _configFldType;
  typedef struct {
    const char *fld;   // "limitalongline", ...
    _configFldType vt; // T_DOUBLE, T_BOOL, T_INT
    double *dp;        // nullptr or &Labelizer::lblCfg.limit_along_line, ...
    bool *bp; // nullptr or &Labelizer::lblCfg.allow_overlapping_lines, ...
    int *ip;  // nullptr or &Labelizer::lblCfg.decimals
  } _configFlds;

private:
  struct PointScore;
  class ChainChunk;
  class ChainIterator;

  const Config config;
  Chain *current_chain;        // what we're currently gathering
  std::vector<Chain *> chains; // all gathered chains so far

  Point last_point;

  Profiler_ms SORT_PROF;
  Profiler_ms CANDIDATE_PROF;
  Profiler_ms LIMIT_ALONG_LINE_PROF;
  Profiler_ms LIMIT_DIRECT_PROF;
  Profiler_ms LIMIT_CROSSING_LINES_PROF;
  Profiler_ms LIMIT_OVERLAPPING_LABELS_PROF;

  Drawer &debug_drawer;

  /*
   * Storing of labels already decided upon.
   */
  struct ChosenLabel {
  public:
    Point center;
    double rad;
    FontSpec fs;
    // const std::string label;
    // const Vectors::Vector size;    // width & height in data coordinates
    // const ContourInfo_Line::LabelParams lp;

    ChosenLabel(const Point &center_, double rad_, const FontSpec &fs_)
        : center(center_), rad(rad_), fs(fs_) {}
    ChosenLabel(const ChosenLabel &o)
        : center(o.center), rad(o.rad), fs(o.fs) {}
    ChosenLabel &operator=(const ChosenLabel &) = default;
  };
  std::vector<ChosenLabel> chosenlabels;

  BoundingBox bb; // data limits (0,0 .. grid.width()-1, grid.height()-1)

  void RejectNearby(const ChainIterator &chosen, double limit_along_line,
                    bool is_closed);

  bool LimitAlongTheLine(const ChainIterator &chosen, double limit,
                         bool is_closed);
  bool LimitDirect(const Point &xy, const std::vector<TiltedRect> &done,
                   double limit);
  bool LimitCrossingLines(const TiltedRect &,
                          const std::vector<Chain *> &chains,
                          const Chain *our_chain);
  bool LimitOverlappingLabels(const TiltedRect &,
                              const std::vector<TiltedRect> &done);

  // 02-Dec-2011 PKi: Labelizer configuration from Q3 configuration file
  //
  static Labelizer::Config lblCfg;
  static const Labelizer::_configFlds *getConfigFlds();
};

#endif
