#ifndef EPOLL_ECHO_SERVER_H
#define EPOLL_ECHO_SERVER_H

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>

class EpollEchoServer {
public:
    EpollEchoServer(const std::string& host = "localhost", int port = 8888);
    ~EpollEchoServer();
    
    void start();
    void stop();

private:
    struct ClientData {
        int fd;
        sockaddr_in addr;
        std::string recv_buffer;
        std::string send_buffer;
        
        ClientData(int socket_fd, const sockaddr_in& client_addr) 
            : fd(socket_fd), addr(client_addr) {}
    };
    
    void setup_server_socket();
    void setup_epoll();
    void event_loop();
    void handle_accept();
    void handle_read(int client_fd);
    void handle_write(int client_fd);
    void close_connection(int client_fd);
    void set_non_blocking(int fd);
    void add_epoll_event(int fd, uint32_t events);
    void modify_epoll_event(int fd, uint32_t events);
    void remove_epoll_event(int fd);
    
    std::string host_;
    int port_;
    int server_fd_;
    int epoll_fd_;
    bool running_;
    
    std::map<int, std::shared_ptr<ClientData>> clients_;
    
    static const int MAX_EVENTS = 64;
};

#endif // EPOLL_ECHO_SERVER_H
