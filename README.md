# web-server

A small, dependency-light HTTP/1.1 static file server written in modern C++20.
It uses a thread pool, an in-memory file cache, inotify-based hot reload, and
ships a built-in `/statistics` dashboard and `/docs` route explorer.

Everything is vendored under `lib/` (no system packages required beyond a C++20
compiler and CMake).

## Features

- **HTTP/1.1** with keep-alive, `GET`/`HEAD`, MIME detection, conditional error
  responses and correct `Content-Length` framing.
- **Thread pool**: one accept thread feeds a work queue consumed by N workers.
- **Routing**: map a request path to a file or a directory (longest-prefix match;
  directories serve `index.html` and their sub-paths).
- **Static file cache**: in-memory, bounded by a total byte budget, with TTL and
  LRU eviction; entries are shared via `shared_ptr` so readers are always safe.
- **Live watching** (inotify): invalidate a cached file as soon as it changes, and
  **hot-reload the configuration** without restarting the server.
- **Statistics**: live counters (requests, bytes, connections, workers, cache
  hit ratio), process RSS/CPU, exposed as JSON and a dashboard, with optional
  JSON Lines logging of periodic samples.
- **Route docs**: a `/docs` JSON endpoint plus a dashboard listing every route.
- **Graceful shutdown** on `SIGINT`/`SIGTERM`.
- **Tests** (Catch2) and a **benchmark harness** (micro + load).

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for how it works and
[`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) for the full config reference.

## Project layout

```
CMakeLists.txt        build definition (core lib + app + tests + bench)
config.toml           example/demo configuration
demo/                 a small demo website served by the example config
static/               dashboards (statistics.html, docs.html)
src/
  main.cpp            CLI, config loading, server startup, signal handling
  config/             Config struct, TOML load/validate/example, route lookup
  io/                 file read/write helpers
  log/                spdlog wrapper (init/level/shutdown + level helpers)
  net/                POSIX socket RAII + listen/connect/accept/send/recv
  http/               HTTP/1.1 message parsing (headers, body, status)
  cache/              thread-safe in-memory cache + CacheManager (cache-or-disk)
  stats/              counters, snapshot, system probes, sampler/reporter
  watch/              inotify-based file/dir watcher
  server/             the Server: accept loop, thread pool, request handling
  util/               small string helpers
test/                 Catch2 test suite
bench/                micro + load benchmark harness
lib/                  vendored: cxxopts, nlohmann/json, spdlog, toml++, Catch2
```

## Requirements

- Linux (the socket and inotify layers use POSIX APIs).
- A C++20 compiler (GCC 11+ / Clang 14+).
- CMake 3.16+.

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

Targets: `web-server` (the server), `web_server_tests`, `web_server_bench`.

## Run

```sh
./build/web-server --config config.toml
```

Then open:

- http://127.0.0.1:6868/ — the demo site
- http://127.0.0.1:6868/statistics.html — live statistics dashboard
- http://127.0.0.1:6868/docs.html — route documentation

Stop with `Ctrl+C` (graceful shutdown).

## Command line

| Option | Description |
|---|---|
| `-h, --help` | print help |
| `-v, --version` | print version |
| `-c, --config PATH` | configuration file to load |
| `-e, --example PATH` | write an example configuration file and exit |
| `-t, --validate PATH` | validate a config file and exit (non-zero if invalid) |
| `--verbose` | enable debug logging |

## Configuration

The server is configured with a TOML file. Minimal example:

```toml
[server]
host = "127.0.0.1"
port = 6868

[routes]
"/" = "./demo"

[cache]
ttl = 60

[logging]
path = "-"        # "-" logs to the console
```

Full reference (all sections, fields, defaults and semantics):
[`docs/CONFIGURATION.md`](docs/CONFIGURATION.md).
Generate a documented template with `./build/web-server --example my.toml`.

## Tests

```sh
./build/web_server_tests          # or: ctest --test-dir build
```

## Benchmarks

```sh
./build/web_server_bench                       # micro + load, defaults
./build/web_server_bench --micro-only          # micro benchmarks only
./build/web_server_bench --load-only --clients 32 --requests 10000
./build/web_server_bench --close --no-cache    # connection-per-request, no cache
```

The load benchmark starts a server in-process on an ephemeral port and reports
throughput (req/s, MB/s) and latency percentiles (p50/p90/p99).

## Notes & limitations

- Concurrency is **threads within a single process** (shared cache and counters).
  Multiple processes (e.g. `SO_REUSEPORT`) are not implemented.
- Directory watching is **single-level** (sub-directories are not watched).
- Config hot reload applies routes/timeouts/size limits live; `host`, `port`,
  `threads`, cache and stats settings require a restart (logged as a warning).
