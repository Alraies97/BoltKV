# ⚡ BoltKV

A high-performance, sharded, in-memory key-value storage engine built from scratch in **C++17**. BoltKV features multi-core throughput via key sharding, configurable durability with fsync policies, basic authentication, and a non-blocking TCP server driven by Linux `epoll`.

## 🚀 Key Features

- **Sharded Multi-Core Design** — Keys are distributed across 16 shards, each with its own `std::shared_mutex`, allowing full utilization of multiple CPU cores.
- **Fast CRUD Engine** — O(1) `SET` / `GET` / `DEL` operations per shard, backed by `std::unordered_map`.
- **Configurable Durability (Fsync Policies)** — Choose from three persistence modes:
  - `ALWAYS`: `fsync()` after every write for maximum durability.
  - `EVERY_SEC`: Background thread that `fsync()`s every second for balanced performance and safety (default).
  - `NONE`: Rely on OS page cache for maximum performance.
- **Basic Authentication** — Shared-secret handshake mechanism to protect against unauthorized access (default password: `chronexis_admin_secure`).
- **AOF Persistence & Safe Crash Recovery** — Each shard maintains its own Append-Only File (AOF, `bolt_log_<shard_id>.aof`), and incomplete compaction files (`bolt_log_<shard_id>.tmp`) are automatically cleaned up on startup.
- **Background AOF Compaction** — Each shard automatically rewrites its AOF file when the number of operations exceeds a threshold to avoid unbounded file growth.
- **Non-Blocking Networking** — A single-threaded event loop built on Linux `epoll` (edge-triggered) handles many concurrent client connections without spawning a thread per connection.
- **Redis-Style Wire Protocol** — Simple line-based commands with Redis-like response prefixes.

## 📁 Project Structure

```text
BoltKV/
├── include/
│   ├── network/
│   │   └── tcp_server.hpp       # TCP server / epoll event loop + authentication
│   └── storage/
│       └── storage_engine.hpp   # Sharded storage engine interface
├── src/
│   ├── network/
│   │   └── tcp_server.cpp       # epoll-based server implementation
│   ├── storage/
│   │   └── storage_engine.cpp   # Sharded storage engine + AOF persistence + compaction
│   └── main.cpp                 # Entry point
├── tests/
│   ├── CMakeLists.txt
│   └── storage_test.cpp         # CRUD + concurrency unit tests
├── benchmark/
│   ├── benchmark.py             # Basic throughput / latency benchmark
│   ├── compaction_stress_test.py
│   └── concurrent_benchmark.py
├── dashboard.py                 # Streamlit web dashboard (legacy)
├── CMakeLists.txt
├── .gitignore
├── LICENSE
└── README.md
```

## 🏗️ Architecture

### Core Components

1. **`StorageEngine`** (`storage/storage_engine.hpp/.cpp`) - Represents a single shard:
   - Owns an `unordered_map<string, string>` protected by a `shared_mutex`.
   - Manages shard-specific AOF files (`bolt_log_<shard_id>.aof` / `bolt_log_<shard_id>.tmp`).
   - Implements configurable fsync policies (ALWAYS / EVERY_SEC / NONE).
   - Runs background AOF compaction when the number of operations exceeds a threshold.

2. **`ShardedDatabase`** (`storage/storage_engine.hpp/.cpp`) - Manages 16 shards:
   - Routes keys to shards using `std::hash<std::string>{}(key) % NUM_SHARDS`.
   - Aggregates results for `size()` across all shards.
   - Loads and rewrites AOF files for all shards.

3. **`TcpServer`** (`network/tcp_server.hpp/.cpp`) - Networking and authentication layer:
   - Non-blocking `epoll` event loop (edge-triggered) handling concurrent connections.
   - Connection state tracking (authenticated / not authenticated).
   - Supports `AUTH` command and rejects unauthorized commands.

4. **`main.cpp`** - Wires everything together:
   - Constructs a `ShardedDatabase`, loads all AOF files via `load_all_aofs()`.
   - Starts a `TcpServer` listening on port **6380**.

## 📡 Wire Protocol

Commands are plain text, one per request, in the form `COMMAND [ARGUMENTS]`:

| Command          | Description                  | Response                                  |
|------------------|-------------------------------|--------------------------------------------|
| `AUTH password`  | Authenticate with server      | `+OK` or `-ERR invalid password`           |
| `SET key value`  | Insert or update a key        | `+OK` (requires auth)                      |
| `GET key`        | Retrieve a value              | `$value` or `$-1 (Key Not Found)` (requires auth) |
| `DEL key`        | Delete a key                  | `:1` (deleted) or `:0` (not found) (requires auth) |
| `COMPACT`        | Trigger AOF compaction        | `+OK AOF Log Compacted Cleanly` (requires auth) |
| *(anything else)*| Unrecognized command          | `-ERR Unknown Command`                     |

Unauthenticated clients receive `-ERR authentication required` for any command except `AUTH`.

## 🔧 Building

Requires a C++17 compiler, CMake ≥ 3.15, and a Linux environment (the server depends on `epoll`).

```bash
mkdir build && cd build
cmake ..
make
```

This produces the `BoltKV_db` executable along with the `storage_engine` and `network_server` libraries.

## ▶️ Running the Server

```bash
./build/BoltKV_db
```

On startup, BoltKV loads all existing `bolt_log_<shard_id>.aof` files to recover previous state, then starts listening on **port 6380**.

You can talk to it with `netcat`:

```bash
# First, authenticate
printf 'AUTH chronexis_admin_secure\r\n' | nc localhost 6380
# +OK

# Then execute commands
printf 'SET name BoltKV\r\n' | nc localhost 6380
# +OK

printf 'GET name\r\n' | nc localhost 6380
# $BoltKV

printf 'DEL name\r\n' | nc localhost 6380
# :1
```

## 🧪 Running Tests

Tests are built via CTest as part of the CMake project:

```bash
cd build
ctest --output-on-failure
```

`storage_test.cpp` covers:
- Basic CRUD correctness (using `ShardedDatabase`)
- Thread-safety under concurrent writes from multiple threads

## 💾 Persistence & Durability

Each shard maintains its own independent AOF file (`bolt_log_<shard_id>.aof`):
- Every successful `SET` or `DEL` is appended to the corresponding shard's AOF file.
- The fsync policy is configurable (default: `EVERY_SEC`).
- On startup, incomplete compaction files (`bolt_log_<shard_id>.tmp`) are deleted before loading the AOF.
- When a shard's operation count exceeds the compaction threshold, a background rewrite process snapshots the current state, appends any buffered operations, and atomically replaces the old AOF.

## 📄 License

MIT License — see `LICENSE` for details.
