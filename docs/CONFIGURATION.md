# Configuration reference

The server reads a TOML file (`--config PATH`). Generate a documented template
with `./build/web-server --example my.toml`, and check a file with
`./build/web-server --validate my.toml`.

All sections and keys are optional; omitted keys keep their defaults.

## `[server]`

| Key | Type | Default | Description |
|---|---|---|---|
| `host` | string | `"0.0.0.0"` | address to bind |
| `port` | integer (1–65535) | `8080` | port to bind; `0` picks a free port |
| `threads` | integer | `0` | worker threads; `0` = one per CPU core |
| `keep_alive_timeout` | seconds | `5` | idle time before a keep-alive connection is closed |
| `read_timeout` | seconds | `15` | time allowed to receive a complete request |
| `max_header_bytes` | size | `16384` | request header block limit (exceeded → `431`) |
| `max_body_bytes` | size | `1048576` | request body limit (exceeded → `413`) |

## `[routes]`

A table mapping a **request path** to a **file or directory** to serve.

```toml
[routes]
"/"        = "./public"
"/logo"    = "./assets/logo.svg"
```

- Matching is **longest-prefix**: the route whose path is the longest prefix of
  the request path wins. A route ending in `/` (e.g. `"/"`) matches any sub-path.
- A **directory** target serves `index.html` for the directory itself and
  `target/<remainder>` for sub-paths.
- A **file** target serves that file; a non-empty remainder is `404`.
- Path traversal (`..`) is rejected with `403`.
- Unknown paths return `404`.

## `[cache]`

| Key | Type | Default | Description |
|---|---|---|---|
| `ttl` | seconds | `0` | how long a cached file stays valid; `0` disables caching |
| `max_size` | size | `1G` | total memory budget (LRU eviction beyond it) |
| `max_file_size` | size | `64M` | files larger than this are served but never cached |

## `[docs]`

Built-in route documentation (off by default).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | enable the endpoint and page |
| `path` | string | `"/docs"` | JSON endpoint |
| `page` | string | `"/docs.html"` | dashboard page path |
| `page_file` | string | `"static/docs.html"` | dashboard file on disk |

## `[stats]`

Live statistics endpoint, dashboard and optional JSON Lines output (off by default).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | enable the endpoint, page and sampler |
| `path` | string | `"/statistics"` | JSON endpoint |
| `page` | string | `"/statistics.html"` | dashboard page path |
| `page_file` | string | `"static/statistics.html"` | dashboard file on disk |
| `file` | string | `""` | JSON Lines file; empty = no file output |
| `interval` | seconds | `5` | sampling interval (also the CPU% delta window) |

## `[watch]`

inotify-based file watching (off by default).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | master switch |
| `config_reload` | bool | `false` | reload this config file when it changes |
| `invalidate_cache` | bool | `true` | evict cached files as soon as they change |

## `[logging]`

| Key | Type | Default | Description |
|---|---|---|---|
| `path` | string | `""` | log file path; empty or `"-"` logs to the console |

## Values

- **Size** accepts a plain integer (bytes) or a suffixed string:
  `B`, `K`/`KB`, `M`/`MB`, `G`/`GB`, `T`/`TB` (case-insensitive), e.g. `"1G"`,
  `"64M"`, `512`.
- **Seconds** are plain integers.

## Validation

`--validate` checks, among other things:

- `server.host` is not empty;
- `server.port` is in range;
- `cache.ttl` is not negative; size values parse;
- every route `path` starts with `/`;
- every route `target` exists;
- `logging.path` is not a directory.

Exit code is `0` when valid, non-zero (with reasons) otherwise.

## Config reload policy

When `[watch] config_reload = true`, editing the config file reloads it live:

- **Applied immediately**: `[routes]`, `keep_alive_timeout`, `read_timeout`,
  `max_header_bytes`, `max_body_bytes`.
- **Requires restart** (logged as a warning, ignored otherwise): `host`, `port`,
  `threads`, `[cache]`, `[stats]`, `[docs]`, `[watch]`, `[logging]`.

## Example

```toml
[server]
host = "127.0.0.1"
port = 6868
threads = 4

[routes]
"/" = "./demo"

[cache]
ttl = 60
max_size = "64M"

[docs]
enabled = true

[stats]
enabled = true
file = "stats.jsonl"
interval = 2

[watch]
enabled = true
config_reload = true
invalidate_cache = true

[logging]
path = "-"
```
