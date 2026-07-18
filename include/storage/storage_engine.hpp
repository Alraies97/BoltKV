#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <shared_mutex>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>

namespace BoltKV {


class StorageEngine {
public:
    StorageEngine();
    ~StorageEngine() = default;

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    
     
    void set(const std::string& key, const std::string& value);

    std::optional<std::string> get(const std::string& key);

    bool del(const std::string& key);

    size_t size() const;
    
    void load_aof();
    void rewrite_aof();

private:
    std::unordered_map<std::string, std::string> registry_;
    mutable std::shared_mutex rw_mutex_;
    std::ofstream aof_file_;
    
    size_t command_count_ = 0;
    const size_t COMPACT_THRESHOLD = 10000;

    std::atomic<bool> is_compacting_{false};
    std::vector<std::string> aof_rewrite_buf_;

    void append_to_aof(const std::string& cmd, const std::string& key, const std::string& value = "");
};

} // namespace BoltKV