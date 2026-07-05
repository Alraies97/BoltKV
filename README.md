# ⚡ BoltKV

A high-performance, ultra-fast, in-memory key-value storage engine built from scratch in C++17. Optimized for low-latency system architectures utilizing a non-blocking network event loop driven by Linux `epoll`, complemented by a beautiful real-time Streamlit Web Dashboard.

## 🚀 Key Features
* **Production-Grade Engine**: $O(1)$ CRUD operations using optimized hash structures.
* **Concurrent & Thread-Safe**: Implements a strict Read-Write lock pattern (`std::shared_mutex`) preventing data races under parallel lookups.
* **Asynchronous I/O Multiplexing**: Driven by a single-threaded Linux `epoll` architecture (Edge-Triggered mode) to scale easily to thousands of active connections.
* **AOF Persistence**: Safe against crashes via continuous Append-Only File transaction logging (`bolt_log.aof`).
* **Web Dashboard UI**: A minimalist, dark-themed management interface built with Python and Streamlit for live query execution and metrics monitoring.

## 📁 System Architecture
```text
BoltKV/
├── include/       # Interface blueprints & APIs (.hpp)
├── src/           # Implementation logic (.cpp)
├── tests/         # Automated unit testing suite
├── dashboard.py   # Python Streamlit Web UI Dashboard
└── .gitignore     # Git exclusion rules (ignores build/ and venv/)