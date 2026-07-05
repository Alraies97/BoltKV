# ⚡ BoltKV

A high-performance, ultra-fast, in-memory key-value storage engine built from scratch in C++17. Optimized for low-latency system architectures utilizing a non-blocking network event loop driven by Linux `epoll`.

## 🚀 Key Features
* **Production-Grade Engine**: $O(1)$ CRUD operations using optimized hash structures.
* **Concurrent & Thread-Safe**: Implements a strict Read-Write lock pattern (`std::shared_mutex`) preventing data races under parallel lookups.
* **Asynchronous I/O Multiplexing**: Driven by a single-threaded Linux `epoll` architecture (Edge-Triggered mode) to scale easily to thousands of active connections without thread overhead.
* **AOF Persistence**: Safe against crashes via continuous Append-Only File transaction logging.

## 📁 System Architecture
```text
BoltKV/
├── include/       # Interface blueprints & APIs (.hpp)
├── src/           # Implementation logic (.cpp)
└── tests/         # Automated unit testing suite