#include "storage/storage_engine.hpp"
#include <mutex>
#include <sstream>
#include <iostream>

namespace BoltKV {

StorageEngine::StorageEngine() {
    // فتح الملف بنمط الـ Append (إضافة البيانات لنهاية الملف دون مسح القديم)
    aof_file_.open("bolt_log.aof", std::ios::app);
}

void StorageEngine::append_to_aof(const std::string& cmd, const std::string& key, const std::string& value) {
    if (aof_file_.is_open()) {
        if (cmd == "SET") {
            aof_file_ << "SET " << key << " " << value << "\n";
        } else if (cmd == "DEL") {
            aof_file_ << "DEL " << key << "\n";
        }
        aof_file_.flush(); // إجبار نظام التشغيل على الكتابة الفورية على القرص وعدم تأخيرها في الـ Buffer
    }
}

void StorageEngine::set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    registry_[key] = value;
    append_to_aof("SET", key, value); // تسجيل عملية الكتابة
}

std::optional<std::string> StorageEngine::get(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    auto it = registry_.find(key);
    if (it != registry_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool StorageEngine::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    bool existed = registry_.erase(key) > 0;
    if (existed) {
        append_to_aof("DEL", key); // تسجيل عملية الحذف في حال كان المفتاح موجوداً فعلاً
    }
    return existed;
}

size_t StorageEngine::size() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return registry_.size();
}

void StorageEngine::load_aof() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    std::ifstream file("bolt_log.aof");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cmd, key, value;
        ss >> cmd >> key;
        
        if (cmd == "SET") {
            std::getline(ss >> std::ws, value);
            registry_[key] = value;
        } else if (cmd == "DEL") {
            registry_.erase(key);
        }
    }
    std::cout << "💾 [BoltKV] Loaded recovery state from AOF. Active registry entries: " << registry_.size() << "\n";
}

} // namespace BoltKV