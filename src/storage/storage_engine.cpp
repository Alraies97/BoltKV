#include "storage/storage_engine.hpp"
#include <mutex>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <chrono>
#include <fstream>

namespace BoltKV
{

StorageEngine::StorageEngine()
    : fsync_policy_(FsyncPolicy::EVERY_SEC)
    , flusher_running_(false)
    , aof_file_(nullptr)
{
    aof_file_ = fopen("bolt_log.aof", "a");
    if (fsync_policy_ == FsyncPolicy::EVERY_SEC)
    {
        start_flusher_thread();
    }
}

StorageEngine::~StorageEngine()
{
    stop_flusher_thread();
    if (aof_file_)
    {
        fclose(aof_file_);
    }
}

void StorageEngine::start_flusher_thread()
{
    flusher_running_ = true;
    aof_flusher_thread_ = std::thread([this]()
    {
        while (flusher_running_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            std::lock_guard<std::mutex> lock(aof_mutex_);
            if (aof_file_)
            {
                fflush(aof_file_);
                int fd = fileno(aof_file_);
                fsync(fd);
            }
        }
    });
}

void StorageEngine::stop_flusher_thread()
{
    if (flusher_running_)
    {
        flusher_running_ = false;
        if (aof_flusher_thread_.joinable())
        {
            aof_flusher_thread_.join();
        }
    }
}

void StorageEngine::append_to_aof(const std::string& cmd, const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(aof_mutex_);
    if (aof_file_)
    {
        std::string line;
        if (cmd == "SET")
        {
            line = "SET " + key + " " + value + "\n";
            fputs(line.c_str(), aof_file_);
        }
        else if (cmd == "DEL")
        {
            line = "DEL " + key + "\n";
            fputs(line.c_str(), aof_file_);
        }
        fflush(aof_file_);

        if (fsync_policy_ == FsyncPolicy::ALWAYS)
        {
            int fd = fileno(aof_file_);
            fsync(fd);
        }

        if (is_compacting_)
        {
            aof_rewrite_buf_.push_back(line);
        }
    }
}

void StorageEngine::set(const std::string& key, const std::string& value)
{
    bool should_compact = false;

    {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        registry_[key] = value;
        append_to_aof("SET", key, value); 

        command_count_++;
        if (command_count_ >= COMPACT_THRESHOLD)
        {
            should_compact = true;
            command_count_ = 0; 
        }
    } 

    if (should_compact)
    {
        std::cout << "🤖 [Auto-Compaction] Threshold reached (" << COMPACT_THRESHOLD << " writes). Triggering rewrite...\n";
        rewrite_aof();
    }
}

std::optional<std::string> StorageEngine::get(const std::string& key)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    auto it = registry_.find(key);
    if (it != registry_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

bool StorageEngine::del(const std::string& key)
{
    bool should_compact = false;
    bool existed = false;

    {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        existed = registry_.erase(key) > 0;
        if (existed)
        {
            append_to_aof("DEL", key); 
            
            command_count_++;
            if (command_count_ >= COMPACT_THRESHOLD)
            {
                should_compact = true;
                command_count_ = 0;
            }
        }
    } 

    if (should_compact)
    {
        std::cout << "🤖 [Auto-Compaction] Threshold reached (" << COMPACT_THRESHOLD << " writes). Triggering rewrite...\n";
        rewrite_aof();
    }

    return existed;
}

size_t StorageEngine::size() const
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return registry_.size();
}

void StorageEngine::load_aof()
{
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);

    std::string tmp_filename = "bolt_log.tmp";
    if (std::filesystem::exists(tmp_filename))
    {
        try
        {
            std::filesystem::remove(tmp_filename);
            std::cout << "⚠️ [Recovery] Found and removed incomplete compaction file: " << tmp_filename << "\n";
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[Recovery] Failed to remove tmp file: " << e.what() << std::endl;
        }
    }

    std::ifstream file("bolt_log.aof");
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string cmd, key, value;
        ss >> cmd >> key;
        
        if (cmd == "SET")
        {
            std::getline(ss >> std::ws, value);
            registry_[key] = value;
        }
        else if (cmd == "DEL")
        {
            registry_.erase(key);
        }
    }
    std::cout << "💾 [BoltKV] Loaded recovery state from AOF. Active registry entries: " << registry_.size() << "\n";
}

void StorageEngine::rewrite_aof()
{
   if (is_compacting_)
    {
        return;
    }

    is_compacting_ = true;

    std::unordered_map<std::string, std::string> registry_snapshot;
    {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        registry_snapshot = registry_;
    }

    std::thread background_worker([this, snapshot = std::move(registry_snapshot)]() mutable
    {
        std::string tmp_filename = "bolt_log.tmp";
        FILE* tmp_file = fopen(tmp_filename.c_str(), "w");

        if (!tmp_file)
        {
            is_compacting_ = false;
            return;
        }

        for (const auto& [key, value] : snapshot)
        {
            fprintf(tmp_file, "SET %s %s\n", key.c_str(), value.c_str());
        }
        fclose(tmp_file);

        {
            std::unique_lock<std::shared_mutex> lock(rw_mutex_);
            std::lock_guard<std::mutex> aof_lock(aof_mutex_);

            FILE* append_tmp = fopen(tmp_filename.c_str(), "a");
            if (append_tmp)
            {
                for (const auto& cmd_line : aof_rewrite_buf_)
                {
                    fputs(cmd_line.c_str(), append_tmp);
                }
                aof_rewrite_buf_.clear(); 
                fclose(append_tmp);
            }

            if (aof_file_)
            {
                fclose(aof_file_);
                aof_file_ = nullptr;
            }

            try
            {
                std::filesystem::rename(tmp_filename, "bolt_log.aof");
                std::cout << "⚡ [Background-Compaction] AOF Rewrite finished successfully without blocking the network loop!\n";
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "[Background-Compaction] Atomic rename failed: " << e.what() << std::endl;
            }

            aof_file_ = fopen("bolt_log.aof", "a");
        }

        is_compacting_ = false;
    });

    background_worker.detach();
}

} // namespace BoltKV