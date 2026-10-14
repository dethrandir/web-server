# Architecture

`web-server` is organized as a set of small modules under `src/`, each in the
`web_server::<module>` namespace. Everything except `main.cpp` is compiled into a
static library, `web_server_core`, which both the application, the test suite and
the benchmark harness link against.

## Modules

| Module | Namespace | Responsibility |
|---|---|---|
| `config` | `web_server::config` | `Config`/`Route`, TOML load/validate/example, longest-prefix `find_route` |
| `io` | `web_server::io` | file read/write (`read_text`, `read_bytes`, `write_bytes`, ...) |
| `log` | `web_server::log` | spdlog wrapper: `init`, `set_level`, `shutdown`, and `trace..critical` helpers |
| `net` | `web_server::net` | `Socket` (RAII) + `listen_tcp`/`connect_tcp`/`accept`/`send`/`receive`, timeouts, `local_port` |
| `http` | `web_server::http` | HTTP/1.1 parsing: headers, body split, request/response, reason phrases |
| `cache` | `web_server::cache` | thread-safe in-memory cache (`Cache`) and the cache-or-disk `CacheManager` |
| `stats` | `web_server::stats` | atomic `Counters`, `Snapshot`, system probes, JSON, background `Reporter` |
| `watch` | `web_server::watch` | inotify-based `Watcher` for files and directories |
| `server` | `web_server::server` | the `Server`: accept loop, thread pool, request handling |
| `util` | `web_server::util` | string helpers (`trim`, `split`, ...) |

## Threading model

```
                 +---------------------+
   clients  -->  |  listening socket    |
                 +----------+----------+
                            | accept()
                 +----------v----------+
                 |   acceptor thread    |  (1)
                 +----------+----------+
                            | enqueue
                 +----------v----------+
                 |  work queue (mutex   |
                 |  + condition var)    |
                 +----------+----------+
                            | dequeue
                 +----------v----------+
                 |  worker threads      |  (N, from config `threads`)
                 +---------------------+
```

- One **acceptor** thread accepts connections and pushes them onto a queue.
- **N worker** threads pop connections and handle them (keep-alive aware).
- The listening socket gets a 1-second receive timeout so `stop()` can interrupt
  `accept()` without closing the socket from another thread (closing does not
  reliably wake a blocked `accept`).

C++ has no GIL, so threads use all cores. Running several processes would only add
isolation (and duplicate the cache), not throughput.

## Request lifecycle

1. `accept()` → a `net::Socket` (RAII) is queued.
2. A worker reads from the socket into a growable buffer until a full request is
   present: header block (`\r\n\r\n`) plus `Content-Length` bytes of body.
   Size limits (`max_header_bytes`, `max_body_bytes`) guard against abuse.
3. `http::parse_request` splits the message; the method must be `GET`/`HEAD`
   (otherwise `405`). The target is percent-decoded and the query string dropped.
4. Built-in endpoints (if enabled) are matched first: `/docs` and `/statistics`
   (JSON) and their dashboard pages.
5. Otherwise `config::find_route` picks the longest matching route prefix.
   - file route → serve the file; a non-empty remainder is `404`.
   - directory route → join the remainder (rejecting `..` → `403`); a directory
     resolves to `index.html`.
6. The file content comes from `CacheManager` (cache hit or disk), and a response
   is built (`Content-Type` from the extension, `Content-Length`, `Date`,
   `Server`, `Connection`).
7. Keep-alive: the connection is reused unless the client sent
   `Connection: close` or the version is HTTP/1.0.

Response codes: `200/404/403/405/413/431/500` plus `400/...` for malformed input.

## Cache

`cache::Cache` is a thread-safe map `filesystem::path -> shared_ptr<CacheEntry>`:

- Bounded by a total **byte budget** with **LRU** eviction.
- Each entry has a **TTL**; expired entries are dropped on access.
- `get` returns `shared_ptr<const CacheEntry>`, so a reader keeps the bytes alive
  even if another thread evicts the entry concurrently.
- Hit/miss/eviction counters feed the statistics endpoint.
- Entries larger than `cache.max_file_size` are never cached.

`cache::CacheManager` hides all of this from the server: `get(path)` returns the
content from cache when possible and from disk otherwise. A TTL of `0` disables
caching entirely.

## Statistics

`stats::Counters` holds atomic counters bumped on the hot path (requests, response
classes, bytes, connections, worker busy/queue gauges). A `stats::Reporter`
background thread samples these plus process RSS/CPU on an interval, computes CPU%
as a delta between samples, writes a timestamped JSON line to the configured file
(JSON Lines), and caches the latest snapshot for the endpoint. CPU% requires a
delta, which is why it is sampled rather than measured on demand.

## Watching (inotify)

`watch::Watcher` wraps inotify on a background `poll()` loop:

- Files are watched through their **parent directory** (filtered by file name) so
  atomic saves (write + rename) and deletions are detected.
- Several registrations can share one inotify watch descriptor (same directory).
- Used for two things:
  - **Cache invalidation**: when a route target changes, the cached file (or the
    whole directory prefix) is evicted immediately.
  - **Config hot reload**: when the config file changes, it is reloaded.

## Configuration reload

The running config is held behind
`std::atomic<std::shared_ptr<const config::Config>>`; each connection takes a
snapshot, so reads are race-free.

`Server::reload()` applies **routes, timeouts and size limits** live. Changes to
`host`, `port`, `threads`, cache settings, stats settings and watch settings are
restart-only and are reported with a warning. The applied config keeps the running
values for those fields, so the snapshot stays consistent.
