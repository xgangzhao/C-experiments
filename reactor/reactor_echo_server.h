#ifndef REACTOR_ECHO_SERVER_H
#define REACTOR_ECHO_SERVER_H

#include <sys/select.h>
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
#include <functional>
#include <memory>
#include <cstring>
#include <algorithm>

class ReactorEchoServer {
public:
    ReactorEchoServer(const std::string& host = "localhost", int port = 8888);
    ~ReactorEchoServer();
    
    void start();
    void stop();

private:
    struct ClientData {
        int fd;
        sockaddr_in addr;
        std::string recv_buffer;
        std::string send_buffer;
        bool waiting_for_write;
        
        ClientData(int socket_fd, const sockaddr_in& client_addr) 
            : fd(socket_fd), addr(client_addr), waiting_for_write(false) {}
    };
    
    void setup_server_socket();
    void event_loop();
    void handle_accept();
    void handle_read(int client_fd);
    void handle_write(int client_fd);
    void close_connection(int client_fd);
    void set_non_blocking(int fd);
    
    std::string host_;
    int port_;
    int server_fd_;
    bool running_;
    fd_set read_fds_;
    fd_set write_fds_;
    fd_set master_read_fds_;
    fd_set master_write_fds_;
    int max_fd_;
    
    std::map<int, std::shared_ptr<ClientData>> clients_;
};

#endif // REACTOR_ECHO_SERVER_H
