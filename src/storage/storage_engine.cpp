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
        if (cmd == "SET")
        {
            aof_file_ << "SET " << key << " " << value << "\n";
        }
        else if (cmd == "DEL")
        {
            aof_file_ << "DEL " << key << "\n";
        }
        aof_file_.flush(); 
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
    std::string tmp_filename = "bolt_log.tmp";
    std::ofstream tmp_file(tmp_filename, std::ios::out | std::ios::binary);

    if (!tmp_file.is_open())
    {
        std::cerr << "[Compaction] Failed to open temporary AOF file" << std::endl;
        return;
    }

    {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        for (const auto& [key, value] : registry_)
        {
            tmp_file << "SET " << key << " " << value << "\n";
        }
    } 

    tmp_file.close();

    // المرحلة الثانية: استبدال الملفات بشكل ذري (آمن)
    {
        // نأخذ قفل كتابة كامل (Unique) لضمان عدم قيام أي عميل بالكتابة أثناء عملية التبديل
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        
        if (aof_file_.is_open())
        {
            aof_file_.close();
        }

        try
        {
            std::filesystem::rename(tmp_filename, "bolt_log.aof");
            std::cout << "⚡ [Compaction] AOF Rewrite successful. Log compacted cleanly!" << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[Compaction] Atomic rename failed: " << e.what() << std::endl;
        }

        aof_file_.open("bolt_log.aof", std::ios::app);
    }
}

} // namespace BoltKV