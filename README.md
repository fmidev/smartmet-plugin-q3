# SmartMet Plugin Q3

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem.

Q3 is a [SmartMet Server](https://github.com/fmidev/smartmet-server) plugin that provides a Lua scripting interface for querying and processing weather data. Client requests are HTTP GET calls where the `code` parameter is a Lua script. The script runs in a sandboxed LuaJIT state with access to configured data tracks, matrix math, coordinate functions, and Cairo graphics.

Q3 reads FMI querydata (SQD) files, which are a grid-based binary format used by the Finnish Meteorological Institute. It is primarily used to serve aviation weather products and other derived weather products that require custom Lua calculations.

## HTTP API

```
GET /q3?code=<url-encoded-lua>&[<globals>]
```

### Standard parameters

| Parameter    | Description |
|--------------|-------------|
| `code`       | URL-encoded Lua script to execute |
| `validtime`  | Validity time: `YYYYMMDDHHMM`, `NOW`, `TODAY`, `NOW+N` (hours), `TODAY+N` |
| `origintime` | Model run time (same formats as `validtime`) |
| `projection` | Output projection string (e.g. `stereographic,20,90,60:6,51.3,49,70.2`) |
| `gridsize`   | Output grid size as `X,Y` |
| `decimals`   | Decimal places in numeric output (default: 0) |
| `callback`   | JSONP callback function name |

Any additional query parameters are passed as string globals to the Lua script.

### Response

- Numeric or string values: JSON array
- Matrix data: JSON 2D array
- Cairo surface: PNG image (`Content-Type: image/png`)
- Multiple return values: JSON array of the above

### Examples

```
# Return the plugin version
GET /q3?code=return+RPM_VERSION

# Temperature from HIR track at current validtime
GET /q3?code=return+HIR.T

# Wind speed at 850 hPa
GET /q3?code=return+HIR%7Bhpa%3D850%7D.WS

# Temperature difference between two models
GET /q3?code=return+HIR.T-EC.T

# Cross-section time series at a single point
GET /q3?code=return+cross(HIR,T,latlon(60.17,24.94),time_range_h(NOW,NOW%2B24,1),{ground=true})
```

## Configuration

The configuration file uses [libconfig](https://hyperrealm.github.io/libconfig/) format and is installed at `/etc/smartmet/plugins/q3plugin.conf`. A template is in `cnf/q3plugin.conf`.

### Global settings

```
log = syslog local2       # stderr | syslog <facility>
killtime = 20sec          # max script execution time
refresh = 3min            # interval for reloading data file lists
rootdir = /smartmet/      # prepended to relative file mask paths
relative_uv = false       # true if U/V wind components are grid-relative
package_path = /usr/share/luajit-2.1/?.lua   # Lua addon search path
package_cpath = /usr/lib64/?.so              # binary addon search path
```

### Health checks

```
healthcheck(3min) = *                        # all metrics every 3 minutes
healthcheck(60min) = / /smartmet/data        # disk usage every hour
```

### Track definitions

Tracks are named data sources. Each track lists file masks (relative to `rootdir`) that match SQD files. The plugin watches these for new data.

```
HIR {
    runs = 6h     # model run frequency: affects -1,-2,... origintime indexing
    { data/hirlam/eurooppa/pinta/querydata/*_hirlam_eurooppa_pinta.sqd }
    { data/hirlam/eurooppa/painepinta/querydata/*.sqd }
    { data/hirlam/eurooppa/mallipinta/querydata/*_hirlam_eurooppa_mallipinta.sqd }
}

EC {
    runs = 12h
    { ecmwf/pinta/*_ecmwf_pinta.sqd }
    { ecmwf/painepinta/*_ecmwf_painepinta.sqd }
    { ecmwf/mallipinta/ecmwf_mallipinta_*.sqd }
}
```

Archive data (bzip2-compressed) uses `/**/` as a directory wildcard:

```
EC_ARCHIVE {
    { archive/ecmwf/**/*.sqd.bz2 }
}
```

Track-level defaults (override global):

```
MEPS {
    runs = 6h
    relative_uv = false
    { data/metcoop/scandinavia/control/surface/querydata/*.sqd }
}
```

## Lua Query Language

Each request runs a fresh LuaJIT state. The Lua environment is sandboxed: `io`, `dofile`, `loadfile`, `load`, and `coroutine` are removed.

### Globals available to scripts

| Global       | Type    | Description |
|--------------|---------|-------------|
| `validtime`  | JDay    | From the `validtime` URL parameter (default: NOW) |
| `origintime` | JDay    | From the `origintime` URL parameter |
| `projection` | string  | From the `projection` URL parameter |
| `gridsize`   | table   | From the `gridsize` URL parameter (`{x=N, y=N}`) |
| `NOW`        | JDay    | Current UTC time, rounded to the nearest hour |
| `TODAY`      | JDay    | Current UTC date at 00:00 |
| `RPM_VERSION`| string  | Plugin version from the RPM build |

JDay values support arithmetic: `NOW+6` adds 6 hours, `validtime.year/month/day/hour/min` are readable fields. `validtime.yday` is the day of year.

### Accessing weather data

The simplest form fetches a parameter at the current `validtime` from the nearest model run:

```lua
return HIR.T        -- temperature from HIR track
return EC.WS        -- wind speed from EC track
```

To specify options, call the track as a function:

```lua
-- At a specific pressure level
return HIR{ hpa=850 }.T

-- Multiple pressure levels
local r, err = HIR{ hpa={850, 700, 500}, params={T, WS} }
assert(r, err)
return r{ hpa=850 }.T

-- Hybrid (model) levels
return HIR{ hybrid=true, params={T, WS} }

-- All levels including height field
return HIR{ height=true, params={T} }

-- Flight level (in hundreds of feet, i.e. FL50 = 5000 ft)
return HIR{ flight=50 }.WS

-- Specific origintime (negative index = Nth most recent run)
return HIR[-1].T     -- previous run
return HIR[-2].T     -- run before that

-- Sounding (radiosonde) data
return HIR{ sounding=true }
```

Track calls return `nil, error_string` on failure; use `assert(r, err)` or `pcall()`.

### Matrix operations

Track data is returned as a 2D matrix of floating-point values. Standard Lua operators and math functions work element-wise:

```lua
local diff = HIR.T - EC.T           -- pointwise subtraction
local rh = 100 * pow((112 - 0.1*HIR.T + HIR.DP) / (112 + 0.9*HIR.T), 8)

-- Scalar functions applied element-wise
local capped = min(HIR.T, 0)        -- cap at 0
local m = max(HIR.WS, EC.WS)

-- Aggregate functions
local mean_t = avg(HIR.T)
local total  = sum(HIR.T)
local count  = count(HIR.T)         -- non-NaN count

-- Gradient and advection
local dir, mag = grad(HIR.P)        -- 2D gradient; returns (direction matrix, magnitude matrix)
local adv_t = adv(HIR.T) * 1e5     -- advection (requires U, V in the same track call)
```

Missing values are represented as NaN and propagate through arithmetic.

### Iterating over grid points

```lua
-- Iterate all points; 'pos' is a MatrixPos, 'v' is the value
for pos, v in points(HIR.T) do
    if v > 0 then HIR.T[pos] = 0 end
end

-- Functional form
local capped = foreach(HIR.T, function(v) return (v > 2) and 2 or v end)

-- Neighbourhood average within 10 km
local result = {}
for pos, subm in points(HIR.T, { range_km=10.0 }) do
    result[latlon(pos)] = avg(subm)
end
```

### Iterating over vertical levels

```lua
local r, err = HIR{ height=true, params={T, WS} }
assert(r, err)

local minT = matrix()   -- all-NaN matrix
for g in grids_by_level(r) do
    minT = min(minT, g.T)
end
return minT

-- Find the maximum wind speed and its pressure level
return MAXZ(r, WS, 0, 5000)   -- max WS between 0 and 5000 m
```

### Coordinate functions

```lua
-- Named place (requires fminames addon)
require "fminames"
local pos = latlon("Helsinki")
local pos = latlon("60°12'N 24°57'E")

-- Explicit coordinates (lat, lon)
local pos = latlon(60.17, 24.94)
local pos = lonlat(24.94, 60.17)   -- note reversed order

-- Point extraction from a matrix
local t_hki = HIR.T[ latlon(60.17, 24.94) ]

-- Multiple points
local temps = HIR.T[ { latlon(60.17, 24.94), latlon(61.50, 23.77) } ]

-- Grid lon/lat matrices
local lons, lats = LONLAT()

-- Distance
local km = distance_km( latlon(60.17, 24.94), latlon(61.50, 23.77) )

-- Area mask: boolean matrix true inside the given polygon
local mask = areamask( { latlon(60.17, 24.94), latlon(61.50, 23.77), ... } )
```

### Time series and cross-sections

`cross()` extracts a series of values along locations and/or times:

```lua
-- Time series at a single point (ground level)
local times = time_range_h(NOW, NOW+24, 1)     -- every hour for 24 h
return cross(HIR, T, latlon(60.17, 24.94), times, {ground=true})

-- Time series at multiple points
local locs = { latlon(60.17, 24.94), latlon(61.50, 23.77) }
return cross(HIR, T, locs, times, {ground=true})

-- Vertical cross-section at fixed times
local locs = { latlon(61.2, 23.1), latlon(65.6, 24.7) }
local levels = { 950, 900, 850, 700, 500, 300 }
return cross(HIR, T, locs, validtime, { hpa=levels })

-- Route cross-section (different time at each location)
local route_times = { NOW, NOW+1, NOW+2 }
return cross(HIR, T, locs, route_times, {ground=true})

-- Time range helpers
local tr_h    = time_range_h(NOW, NOW+24, 3)       -- every 3 hours
local tr_mins = time_range_mins(NOW, NOW+6, 30)    -- every 30 minutes
```

### Rolling validtime

The `validtime` global can be modified within a script to fetch data at different times:

```lua
local t0 = EC.T
local t = {}
for i = 1, 8 do
    validtime = validtime + 24    -- advance by 24 hours
    t[i] = EC.T - t0
end
return unpack(t)
```

### Utility functions

```lua
concat(tbl, sep)          -- join table values into a string
GSIZE()                   -- returns gridsize from globals (x, y)
DUMP(val, ...)            -- debug: returns a string representation
```

### Cairo graphics

When a script returns a Cairo surface object, the response is a PNG image.

```lua
require 'newcairo'

local cs, cr = newcairo.surface(600, 400)   -- width, height in pixels
cr.set_source_rgb(1, 1, 1).paint()          -- white background

-- Draw circles
cr.circle(300, 200, 50)
  .set_source_rgba(1, 0, 0, 0.8)
  .fill()

-- Draw lines
cr.move_to(0, 0).line_to(600, 400)
  .set_source_rgb(0, 0, 0)
  .set_line_width(2)
  .stroke()

-- Draw text
cr.move_to(10, 20)
  .set_font_size(14)
  .show_text("Hello")

return cs   -- returns PNG image
```

Contour lines and filled contours over a data grid:

```lua
require 'newcairo'
local cs, cr = newcairo.surface(500, 600)

-- Contour lines at fixed intervals
local contours = contour(HIR.T, { -20, -10, 0, 10, 20 })
for _, c in ipairs(contours) do
    cr.new_path()
    -- c is a list of (x, y) grid coordinate pairs forming one contour line
end
return cs
```

## Building

```sh
make         # build q3.so
make install # install to $(plugindir) = /usr/share/smartmet/plugins/
make clean
make rpm     # build RPM package
```

Dependencies: `smartmet-library-spine`, `smartmet-library-newbase`, `smartmet-library-tron`, `luajit`, `geos`, `gdal`, `cairo`, `libconfig++`, `proj`.

## Installing

From RPM:

```sh
sudo dnf install smartmet-plugin-q3
```

The plugin installs:
- `/usr/share/smartmet/plugins/q3.so` — plugin shared library
- `/etc/smartmet/plugins/q3plugin.conf` — configuration (not replaced on upgrade)
- `/usr/share/q3plugin/fonts/*.ttf` — TTF fonts for Cairo text rendering

Register the plugin in the SmartMet Server configuration by adding it to the plugins list. Example `smartmet.conf` fragment:

```
[plugins]
q3 = {
    name   = "q3"
    config = "/etc/smartmet/plugins/q3plugin.conf"
}
```

## Licence

MIT — see [LICENSE](LICENSE).
