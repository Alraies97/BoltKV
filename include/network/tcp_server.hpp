#pragma once

#include "../storage/storage_engine.hpp"
#include <string>
#include <vector>

namespace BoltKV {
class TcpServer {
    public:
      
      TcpServer(int port, ShardedDatabase& db);
      ~TcpServer();

      TcpServer(const TcpServer&) = delete;
      TcpServer& operator=(const TcpServer&) = delete;

      
void start();

    private:
       void setup_socket();
       void set_nonblocking(int fd);
    
       void handle_client_data(int client_fd);
            std::string process_command(const std::string& raw_command);

            int port_;
            int server_fd_;
            int epoll_fd_;
            ShardedDatabase& db_;
            
};

} // namespace BoltKV