#include "storage/storage_engine.hpp"
#include "network/tcp_server.hpp"
#include <iostream>

int main() {
    try {

        BoltKV::StorageEngine engine;
        engine.load_aof();
        
        BoltKV::TcpServer server(6380, engine); 
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}