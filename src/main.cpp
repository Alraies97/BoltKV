#include "storage/storage_engine.hpp"
#include "network/tcp_server.hpp"
#include <iostream>

int main() {
    try {

        BoltKV::ShardedDatabase db;
        db.load_all_aofs();
        
        BoltKV::TcpServer server(6380, db); 
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}