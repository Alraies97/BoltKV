#include "network/tcp_server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <sstream>
#include <vector>

constexpr int MAX_EVENTS = 64;
constexpr int BUFFER_SIZE = 1024;

namespace BoltKV
{

TcpServer::TcpServer(int port, ShardedDatabase& db) 
    : port_(port), server_fd_(-1), epoll_fd_(-1), db_(db), auth_password_("chronexis_admin_secure")
{
}

TcpServer::~TcpServer() 
{
    if (server_fd_ != -1)
    {
        close(server_fd_);
    }
    if (epoll_fd_ != -1)
    {
        close(epoll_fd_);
    }
}

void TcpServer::set_nonblocking(int fd) 
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void TcpServer::setup_socket() 
{
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) 
    {
        throw std::runtime_error("Failed to bind to port");
    }

    if (listen(server_fd_, SOMAXCONN) < 0) 
    {
        throw std::runtime_error("Listen failed");
    }

    set_nonblocking(server_fd_);
}

void TcpServer::start() 
{
    setup_socket();

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) 
    {
        throw std::runtime_error("Failed to create epoll instance");
    }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET; 
    ev.data.fd = server_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev) == -1) 
    {
        throw std::runtime_error("Failed to add server socket to epoll");
    }

    std::cout << "⚡ BoltKV Server started on port " << port_ << "...\n";

    std::vector<epoll_event> events(MAX_EVENTS);

    while (true) 
    {
        int nfds = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i) 
        {
            if (events[i].data.fd == server_fd_) 
            {
                while (true)
                {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) 
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break; 
                        }
                        break;
                    }
                    set_nonblocking(client_fd);
                    
                    epoll_event client_ev{};
                    client_ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    client_ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev);
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                handle_client_data(events[i].data.fd);
            }
            
            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                int client_fd = events[i].data.fd;
                authenticated_clients_.erase(client_fd);
                close(client_fd);
            }
        }
    }
}

void TcpServer::handle_client_data(int client_fd) 
{
    char buffer[BUFFER_SIZE];
    std::string raw_request;

    while (true) 
    {
        std::memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0) 
        {
            raw_request.append(buffer, bytes_read);
        }
        else if (bytes_read == -1) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; 
            }
            authenticated_clients_.erase(client_fd);
            close(client_fd);
            return;
        }
        else 
        { 
            authenticated_clients_.erase(client_fd);
            close(client_fd);
            return;
        }
    }

    if (!raw_request.empty()) 
    {
        std::string response = process_command(raw_request, client_fd);
        write(client_fd, response.c_str(), response.length());
    }
}

std::string TcpServer::process_command(const std::string& raw_command, int client_fd) 
{
    std::stringstream ss(raw_command);
    std::string cmd, key, value;
    ss >> cmd >> key;

    for (auto &c : cmd)
    {
        c = toupper(c);
    }

    bool is_authenticated = false;
    auto it = authenticated_clients_.find(client_fd);
    if (it != authenticated_clients_.end())
    {
        is_authenticated = it->second;
    }

    if (!is_authenticated)
    {
        if (cmd == "AUTH")
        {
            std::getline(ss >> std::ws, value);
            if (!value.empty() && value.back() == '\r')
            {
                value.pop_back();
            }
            if (value == auth_password_)
            {
                authenticated_clients_[client_fd] = true;
                return "+OK\r\n";
            }
            else
            {
                return "-ERR invalid password\r\n";
            }
        }
        else
        {
            return "-ERR authentication required\r\n";
        }
    }

    if (cmd == "SET")
    {
        std::getline(ss >> std::ws, value);
        if (!value.empty() && value.back() == '\r')
        {
            value.pop_back();
        }
        
        db_.set(key, value);
        return "+OK\r\n";
    } 
    else if (cmd == "GET") 
    {
        auto val = db_.get(key);
        if (val) 
        {
            return "$" + *val + "\r\n";
        }
        return "$-1 (Key Not Found)\r\n";
    } 
    else if (cmd == "DEL") 
    {
        bool deleted = db_.del(key);
        return deleted ? ":1\r\n" : ":0\r\n";
    }
    else if (cmd == "COMPACT")
    {
        db_.rewrite_all_aofs();
        return "+OK AOF Log Compacted Cleanly\r\n";
    }

    return "-ERR Unknown Command\r\n";
}

} // namespace BoltKV