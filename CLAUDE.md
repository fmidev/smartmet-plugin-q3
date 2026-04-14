# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is Q3?

Q3 is a SmartMet Server plugin that exposes a Lua scripting interface over HTTP. Clients send Lua code via `GET /q3?code=<url-encoded-lua>` along with optional parameters (`validtime`, `origintime`, `projection`, `gridsize`). The plugin creates an isolated LuaJIT state per request, executes the script with access to weather data tracks and Cairo graphics, and returns JSON or PNG responses.

## Build commands

```bash
make                # Build q3.so (shared plugin library)
make debug          # Debug build (same target, different flags from makefile.inc)
make clean          # Remove q3.so, .lch files, obj/
make format         # clang-format on q3/*.h q3/*.cpp
make install        # Install q3.so to $(plugindir)
make rpm            # Build RPM package
make test           # Run integration tests (cd test && make test)
```

There is no `.clang-format` in this repo; `make format` uses the inherited style from makefile.inc or a parent directory's `.clang-format`.

## Testing

Tests use `smartmet-plugin-test`, not Boost.Test. The test runner starts a SmartMet Server instance with the plugin loaded and replays HTTP requests:

```bash
cd test && make test
# Runs: smartmet-plugin-test --handler=/q3 --reactor-config=cnf/reactor.conf --timeout 300 --num-threads 1
```

- Test inputs: `test/input/*.get` (HTTP GET requests with URL-encoded Lua)
- Expected outputs: `test/output/*` (matched by basename)
- `track_00_warmup.get` runs first to initialize data before real tests
- Test data and server config: `test/cnf/reactor.conf`

There are no unit tests or individual test executables. All tests run through the single `smartmet-plugin-test` invocation.

## Build pipeline: Lua to bytecode

Lua source files in `lua/` are compiled to LuaJIT bytecode and embedded as C headers at build time:

```
lua/*.lua  →  luajit -b  →  piped through bin2c.lua  →  q3/*.lch (C byte arrays)
```

All C++ objects depend on the `.lch` files, so Lua changes trigger a full C++ recompile.

## Architecture

### Plugin loading

`Plugin.cpp` implements the SmartMet `create()`/`destroy()` C API. The `Plugin` class inherits from both `Q3Server` (the actual logic) and `SmartMetPlugin` (the server interface). It registers handlers for `/q3` and `/q3/` endpoints.

### Request flow

1. `Plugin.cpp`: HTTP request → `BS_RequestResponse` adapter → `Q3Server::native_handler()`
2. `Server.cpp`: Parses query parameters, creates a `Session`
3. `Session.cpp`: Creates isolated LuaJIT state, loads embedded Lua modules from `.lch` bytecode, executes user code
4. Result (number, string, table, Matrix, Cairo surface) is serialized as JSON or PNG

### Core C++ classes

- **Q3Engine** (`Q3Engine.cpp/h`) — manages named Track objects (data sources), parses config
- **Track** (`Track.cpp/h`) — watches file masks for SQD data files, manages data lifecycle with configurable retention
- **Session** (`Session.cpp/h`) — per-request LuaJIT state with sandboxed environment; binds C++ functions to Lua
- **SQD_Data** (`SQD_Data.cpp/h`) — reads FMI's querydata format (grid-based binary weather data)
- **Matrix** (`Matrix.cpp/h`) — 2D float matrix with NaN for missing values; element-wise operations
- **NA_Data/NA_Param/NA_Level** — NewBase library wrappers for parameter and level queries
- **LuaWrap** (`LuaWrap.cpp/h`) — exception-safe LuaJIT state wrapper

### Lua modules (in `lua/`)

- `q3.lua` — main API bindings (track access syntax sugar via proxies)
- `utilities.lua` — large utility library (76KB)
- `contour.lua` — contour line generation
- `cross.lua` — time series and cross-section extraction
- `newcairo.lua` — Cairo graphics bindings for PNG rendering
- `config.lua`, `proto.lua`, `type.lua`, `assert.lua`, `json.lua` — supporting modules

### Dependencies

Linked libraries: `smartmet-newbase`, `smartmet-spine`, `smartmet-tron`, `luajit`. Pkg-config requires: `geos`, `gdal`, `cairo`, `configpp`.

### Lua sandboxing

The Session strips `io`, `dofile`, `loadfile`, `load`, and `coroutine` from the Lua environment. User scripts cannot access the filesystem or create coroutines.

## Configuration

Plugin config uses libconfig format (`cnf/q3plugin.conf`). Key settings:
- `killtime` — max script execution time (default 20s)
- `refresh` — data refresh interval (default 3min)
- `rootdir` — base path prepended to relative file masks
- Track definitions map names (e.g., `HIR`, `EC`, `MEPS`) to SQD file masks with optional archive paths
