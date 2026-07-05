#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <shared_mutex>
#include <fstream>

namespace BoltKV {

/**
 * @brief High-Performance In-Memory Storage Engine for BoltKV.
 * * Handles core CRUD operations for key-value pairs utilizing an optimized
 * in-memory hash map structure. Fully thread-safe using a Read-Write lock pattern
 * and persistent via Append-Only File (AOF) logging.
 */
class StorageEngine {
public:
    StorageEngine();
    ~StorageEngine() = default;

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    /**
     * @brief Inserts or updates a key-value pair. Thread-safe (Exclusive Lock).
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Retrieves a value by its key. Thread-safe (Shared Lock).
     */
    std::optional<std::string> get(const std::string& key);

    /**
     * @brief Deletes a key-value pair from BoltKV. Thread-safe (Exclusive Lock).
     */
    bool del(const std::string& key);

    /**
     * @brief Returns the total number of keys currently stored. Thread-safe (Shared Lock).
     */
    size_t size() const;
    
    /**
     * @brief Loads and replays the AOF log file from disk to recover state on startup.
     */
    void load_aof();

private:
    std::unordered_map<std::string, std::string> registry_;
    mutable std::shared_mutex rw_mutex_;
    std::ofstream aof_file_;

    void append_to_aof(const std::string& cmd, const std::string& key, const std::string& value = "");
};

} // namespace BoltKV