#include "storage/storage_engine.hpp"
#include <mutex>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace BoltKV
{

StorageEngine::StorageEngine()
{
    aof_file_.open("bolt_log.aof", std::ios::app);
}

void StorageEngine::append_to_aof(const std::string& cmd, const std::string& key, const std::string& value)
{
    if (aof_file_.is_open())
    {
        std::string line;
        if (cmd == "SET")
        {
            line = "SET " + key + " " + value + "\n";
            aof_file_ << line;
        }
        else if (cmd == "DEL")
        {
            line = "DEL " + key + "\n";
            aof_file_ << line;
        }
        aof_file_.flush(); 

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
        std::ofstream tmp_file(tmp_filename, std::ios::out | std::ios::binary);

        if (!tmp_file.is_open())
        {
            is_compacting_ = false;
            return;
        }

        for (const auto& [key, value] : snapshot)
        {
            tmp_file << "SET " << key << " " << value << "\n";
        }
        tmp_file.close();

        {
            std::unique_lock<std::shared_mutex> lock(rw_mutex_);

            std::ofstream append_tmp(tmp_filename, std::ios::app | std::ios::binary);
            if (append_tmp.is_open())
            {
                for (const auto& cmd_line : aof_rewrite_buf_)
                {
                    append_tmp << cmd_line;
                }
                aof_rewrite_buf_.clear(); 
                append_tmp.close();
            }

            if (aof_file_.is_open())
            {
                aof_file_.close();
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

            aof_file_.open("bolt_log.aof", std::ios::app);
        }

        is_compacting_ = false;
    });

    background_worker.detach();
}

} // namespace BoltKV