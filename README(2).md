# ⚡ BoltKV

A high-performance, in-memory key-value storage engine built from scratch in **C++17**. BoltKV pairs a thread-safe hash-map storage core with a non-blocking TCP server driven by Linux `epoll`, and ships with a Python/Streamlit dashboard for live management and monitoring.

## 🚀 Key Features

- **Fast CRUD Engine** — O(1) `SET` / `GET` / `DEL` operations backed by `std::unordered_map`.
- **Thread-Safe** — Reads and writes are protected by a `std::shared_mutex` (shared locks for reads, exclusive locks for writes), so concurrent clients never race on the registry.
- **Non-Blocking Networking** — A single-threaded event loop built on Linux `epoll` (edge-triggered) handles many concurrent client connections without spawning a thread per connection.
- **AOF Persistence** — Every write (`SET`/`DEL`) is appended to an on-disk Append-Only File (`bolt_log.aof`) and flushed immediately, so state survives a crash or restart.
- **Crash Recovery** — On startup, the engine replays `bolt_log.aof` to rebuild the in-memory registry.
- **Redis-Style Wire Protocol** — Simple line-based commands (`SET`, `GET`, `DEL`) with Redis-like response prefixes (`+OK`, `$value`, `:1`/`:0`, `-ERR`), so existing Redis client libraries can talk to it.
- **Web Dashboard** — A minimal Streamlit UI (`dashboard.py`) for running commands and viewing server status through the `redis-py` client.
- **Unit Tests** — Basic CRUD and concurrent-access tests wired up via CMake/CTest.

## 📁 Project Structure

```text
BoltKV/
├── include/
│   ├── network/
│   │   └── tcp_server.hpp       # TCP server / epoll event loop interface
│   └── storage/
│       └── storage_engine.hpp   # Storage engine interface
├── src/
│   ├── network/
│   │   └── tcp_server.cpp       # epoll-based server implementation
│   ├── storage/
│   │   └── storage_engine.cpp   # Storage engine + AOF persistence
│   └── main.cpp                 # Entry point
├── tests/
│   ├── CMakeLists.txt
│   └── storage_test.cpp         # CRUD + concurrency unit tests
├── dashboard.py                 # Streamlit web dashboard
├── CMakeLists.txt
├── .gitignore
└── README.md
```

## 🏗️ Architecture

- **`StorageEngine`** (`storage/storage_engine.hpp/.cpp`) owns an `unordered_map<string, string>` protected by a `shared_mutex`. `set()`/`del()` take an exclusive (write) lock and append the operation to the AOF file; `get()`/`size()` take a shared (read) lock, allowing many concurrent readers.
- **`TcpServer`** (`network/tcp_server.hpp/.cpp`) opens a non-blocking listening socket, registers it with `epoll`, and runs a single-threaded loop that accepts new connections and reads client data as it arrives. Each request is parsed and dispatched to the `StorageEngine`, and the result is written back to the socket.
- **`main.cpp`** wires the two together: it constructs a `StorageEngine`, replays the AOF log via `load_aof()`, then starts a `TcpServer` listening on port **6380**.

## 📡 Wire Protocol

Commands are plain text, one per request, in the form `COMMAND KEY [VALUE]`:

| Command          | Description                  | Response                                  |
|------------------|-------------------------------|--------------------------------------------|
| `SET key value`  | Insert or update a key        | `+OK`                                      |
| `GET key`        | Retrieve a value              | `$value` or `$-1 (Key Not Found)`          |
| `DEL key`        | Delete a key                  | `:1` (deleted) or `:0` (not found)         |
| *(anything else)*| Unrecognized command          | `-ERR Unknown Command`                     |

Because the response format loosely follows Redis conventions, BoltKV can be queried using a standard Redis client (as the dashboard does via `redis-py`), though it is **not** a full Redis protocol (RESP) implementation.

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

On startup, BoltKV loads any existing `bolt_log.aof` file to recover previous state, then starts listening on **port 6380**.

You can talk to it with `netcat` or `redis-cli`:

```bash
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
- Basic CRUD correctness (`set`/`get`/`del`/`size`)
- Thread-safety under concurrent writes from multiple threads

## 📊 Web Dashboard

A Streamlit dashboard is included for interacting with a running BoltKV instance.

```bash
pip install streamlit redis
streamlit run dashboard.py
```

It connects to BoltKV on `localhost:6380` (via `redis-py`) and lets you:
- Execute `GET` / `SET` / `DEL` commands from a simple form
- View basic server status/metrics

## 💾 Persistence

Every successful `SET` or `DEL` is appended to `bolt_log.aof` in the working directory and flushed to disk immediately. On the next startup, `StorageEngine::load_aof()` replays this log line by line to restore the registry to its last known state. The `.aof` file is excluded from version control via `.gitignore`.

## 📈 Benchmark Results

Measured with a custom async Python benchmark client (`benchmark.py`), 50 concurrent workers issuing 5,000 requests each (250,000 total operations) against the server on `localhost:6380`:

| Metric                     | Result              |
|----------------------------|----------------------|
| Total Operations           | 250,000              |
| Concurrent Workers         | 50                   |
| Total Throughput           | **23,390.35 QPS**    |
| Avg Latency per Request    | **0.0428 ms**        |
| Total Execution Time       | 10.688 seconds       |

## ⚠️ Notes & Limitations

- Single-threaded event loop: throughput is bound by one core (no worker-thread pool for I/O).
- The AOF log grows unbounded — there is no compaction/rewrite mechanism.
- The wire protocol is a simplified, Redis-*inspired* format rather than full RESP, so not all Redis clients/features will work correctly.
- No authentication, encryption, or replication support.
