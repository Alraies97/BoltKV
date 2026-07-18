#include "storage/storage_engine.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

void test_basic_crud() {
    BoltKV::ShardedDatabase db;
    
    db.set("test_key", "test_value");
    auto val = db.get("test_key");
    assert(val.has_value() && val.value() == "test_value");
    
    assert(db.size() == 1);
    
    bool deleted = db.del("test_key");
    assert(deleted == true);
    assert(db.size() == 0);
    
    std::cout << "✅ Basic CRUD Tests Passed!" << std::endl;
}

void test_concurrent_access() {
    BoltKV::ShardedDatabase db;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&db, i]() {
            db.set("key_" + std::to_string(i), "value_" + std::to_string(i));
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    assert(db.size() == 100);
    std::cout << "✅ Concurrent Thread-Safety Tests Passed!" << std::endl;
}

int main() {
    std::cout << "🧪 Running BoltKV Automated Unit Tests..." << std::endl;
    test_basic_crud();
    test_concurrent_access();
    std::cout << "🎉 All Tests Passed Successfully!" << std::endl;
    return 0;
}