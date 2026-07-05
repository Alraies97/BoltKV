#pragma once

#include "../storage/storage_engine.hpp"
#include <string>
#include <vector>

namespace BoltKV {

/**
 * @brief High-Performance TCP Server using Linux epoll.
 * 
 * Drives the non-blocking event loop to handle concurrent connections
 * and routes parsed network commands directly into the BoltKV StorageEngine.
 */
class TcpServer {
    public:
      /**
       * @param port The port number to listen on (e.g., 6379 or 8080).
       * @param engine Reference to the core thread-safe storage engine.
       */
      TcpServer(int port, StorageEngine& engine);
      ~TcpServer();

      TcpServer(const TcpServer&) = delete;
      TcpServer& operator=(const TcpServer&) = delete;

      /**
       * @brief Starts the epoll event loop and begins accepting connections.
       * This call is blocking as it runs the infinite execution loop.
       */
void start();

    private:
       void setup_socket();
       void set_nonblocking(int fd);
    
       void handle_client_data(int client_fd);
            std::string process_command(const std::string& raw_command);

            int port_;
            int server_fd_;
            int epoll_fd_;
            StorageEngine& engine_;
            
};

} // namespace BoltKV