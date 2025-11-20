#include "reactor_echo_server.h"

ReactorEchoServer::ReactorEchoServer(const std::string& host, int port) 
    : host_(host), port_(port), server_fd_(-1), running_(false), max_fd_(0) {
    
    FD_ZERO(&master_read_fds_);
    FD_ZERO(&master_write_fds_);
}

ReactorEchoServer::~ReactorEchoServer() {
    stop();
}

void ReactorEchoServer::set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void ReactorEchoServer::setup_server_socket() {
    // 创建服务器socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("setsockopt failed");
    }
    
    // 绑定地址
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_fd_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Bind failed");
    }
    
    // 监听
    if (listen(server_fd_, 128) < 0) {
        close(server_fd_);
        throw std::runtime_error("Listen failed");
    }
    
    // 设置为非阻塞
    set_non_blocking(server_fd_);
    
    // 添加到读集合
    FD_SET(server_fd_, &master_read_fds_);
    max_fd_ = std::max(max_fd_, server_fd_);
    
    std::cout << "Echo服务器启动在 " << host_ << ":" << port_ << std::endl;
}

void ReactorEchoServer::start() {
    try {
        setup_server_socket();
        running_ = true;
        event_loop();
    } catch (const std::exception& e) {
        std::cerr << "启动服务器失败: " << e.what() << std::endl;
        stop();
    }
}

void ReactorEchoServer::stop() {
    running_ = false;
    
    // 关闭所有客户端连接
    for (auto& client : clients_) {
        close(client.first);
    }
    clients_.clear();
    
    // 关闭服务器socket
    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    FD_ZERO(&master_read_fds_);
    FD_ZERO(&master_write_fds_);
    
    std::cout << "服务器已关闭" << std::endl;
}

void ReactorEchoServer::event_loop() {
    struct timeval timeout;

    std::cout << "进入事件循环..." << std::endl;
    
    while (running_) {
        // 设置超时时间为1秒
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        
        // 复制主文件描述符集合
        read_fds_ = master_read_fds_;
        write_fds_ = master_write_fds_;

        // 调试信息：显示当前监听的fd
        std::cout << "调用 select(), max_fd_ = " << max_fd_ << std::endl;
        
        // 等待事件
        int activity = select(max_fd_ + 1, &read_fds_, &write_fds_, nullptr, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            std::cerr << "select error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (activity == 0) {
            // 超时，可以在这里处理定时任务
            std::cout << "select() 超时，继续等待..." << std::endl;
            continue;
        }

        std::cout << "select() 返回 " << activity << " 个活跃事件" << std::endl;
        
        // 检查所有文件描述符
        for (int fd = 0; fd <= max_fd_; fd++) {
            if (FD_ISSET(fd, &read_fds_)) {
                if (fd == server_fd_) {
                    // 新连接
                    handle_accept();
                } else {
                    // 客户端数据可读
                    handle_read(fd);
                }
            }
            
            if (FD_ISSET(fd, &write_fds_)) {
                // 客户端可写
                handle_write(fd);
            }
        }
    }
}

void ReactorEchoServer::handle_accept() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (true) {
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // 没有更多待处理的连接
                break;
            } else {
                std::cerr << "Accept error: " << strerror(errno) << std::endl;
                break;
            }
        }
        
        // 设置为非阻塞模式
        set_non_blocking(client_fd);
        
        // 创建客户端数据
        auto client_data = std::make_shared<ClientData>(client_fd, client_addr);
        clients_[client_fd] = client_data;
        
        // 添加到读集合
        FD_SET(client_fd, &master_read_fds_);
        max_fd_ = std::max(max_fd_, client_fd);
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "接受来自 " << client_ip << ":" << ntohs(client_addr.sin_port) 
                  << " 的新连接 (fd: " << client_fd << ")" << std::endl;
    }
}

void ReactorEchoServer::handle_read(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }
    
    auto& client_data = it->second;
    char buffer[4096];
    
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            client_data->recv_buffer.append(buffer, bytes_read);
            
            // 输出接收到的数据
            std::cout << "从客户端 " << client_fd << " 收到 " << bytes_read 
                      << " 字节: " << std::string(buffer, bytes_read);
            
            // Echo逻辑：将接收到的数据放入发送缓冲区
            client_data->send_buffer.append(buffer, bytes_read);
            
            // 如果发送缓冲区有数据，监听写事件
            if (!client_data->send_buffer.empty() && !client_data->waiting_for_write) {
                FD_SET(client_fd, &master_write_fds_);
                client_data->waiting_for_write = true;
            }
            
        } else if (bytes_read == 0) {
            // 客户端关闭连接
            std::cout << "客户端 " << client_fd << " 断开连接" << std::endl;
            close_connection(client_fd);
            break;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // 没有更多数据可读
                break;
            } else {
                // 读取错误
                std::cerr << "从客户端 " << client_fd << " 读取错误: " << strerror(errno) << std::endl;
                close_connection(client_fd);
                break;
            }
        }
    }
}

void ReactorEchoServer::handle_write(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }
    
    auto& client_data = it->second;
    
    if (client_data->send_buffer.empty()) {
        // 没有数据要发送，移除写监听
        FD_CLR(client_fd, &master_write_fds_);
        client_data->waiting_for_write = false;
        return;
    }
    
    ssize_t bytes_sent = send(client_fd, 
                             client_data->send_buffer.data(), 
                             client_data->send_buffer.size(), 
                             0);
    
    if (bytes_sent > 0) {
        std::cout << "向客户端 " << client_fd << " 发送 " << bytes_sent << " 字节" << std::endl;
        client_data->send_buffer.erase(0, bytes_sent);
        
        // 如果所有数据都已发送，移除写监听
        if (client_data->send_buffer.empty()) {
            FD_CLR(client_fd, &master_write_fds_);
            client_data->waiting_for_write = false;
        }
    } else {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "向客户端 " << client_fd << " 发送错误: " << strerror(errno) << std::endl;
            close_connection(client_fd);
        }
    }
}

void ReactorEchoServer::close_connection(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it != clients_.end()) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &it->second->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "关闭连接 " << client_ip << ":" << ntohs(it->second->addr.sin_port) 
                  << " (fd: " << client_fd << ")" << std::endl;
        
        clients_.erase(it);
    }
    
    FD_CLR(client_fd, &master_read_fds_);
    FD_CLR(client_fd, &master_write_fds_);
    close(client_fd);
    
    // 更新max_fd_
    if (client_fd == max_fd_) {
        max_fd_ = server_fd_;
        for (const auto& client : clients_) {
            max_fd_ = std::max(max_fd_, client.first);
        }
    }
}
