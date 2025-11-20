#include "epoll_echo_server.h"

EpollEchoServer::EpollEchoServer(const std::string& host, int port) 
    : host_(host), port_(port), server_fd_(-1), epoll_fd_(-1), running_(false) {
}

EpollEchoServer::~EpollEchoServer() {
    stop();
}

void EpollEchoServer::set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void EpollEchoServer::add_epoll_event(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        throw std::runtime_error("epoll_ctl ADD failed for fd " + std::to_string(fd));
    }
}

void EpollEchoServer::modify_epoll_event(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        throw std::runtime_error("epoll_ctl MOD failed for fd " + std::to_string(fd));
    }
}

void EpollEchoServer::remove_epoll_event(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        // 不抛出异常，因为fd可能已经关闭
        std::cerr << "epoll_ctl DEL failed for fd " << fd << ": " << strerror(errno) << std::endl;
    }
}

void EpollEchoServer::setup_server_socket() {
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
    
    std::cout << "Echo服务器启动在 " << host_ << ":" << port_ << std::endl;
}

void EpollEchoServer::setup_epoll() {
    // 创建epoll实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("epoll_create1 failed");
    }
    
    // 添加服务器socket到epoll，监听读事件（水平触发）
    add_epoll_event(server_fd_, EPOLLIN);
    
    std::cout << "Epoll初始化完成" << std::endl;
}

void EpollEchoServer::start() {
    try {
        setup_server_socket();
        setup_epoll();
        running_ = true;
        event_loop();
    } catch (const std::exception& e) {
        std::cerr << "启动服务器失败: " << e.what() << std::endl;
        stop();
    }
}

void EpollEchoServer::stop() {
    running_ = false;
    
    // 关闭所有客户端连接
    for (auto& client : clients_) {
        close(client.first);
    }
    clients_.clear();
    
    // 关闭epoll和服务器socket
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    std::cout << "服务器已关闭" << std::endl;
}

void EpollEchoServer::event_loop() {
    struct epoll_event events[MAX_EVENTS];
    
    std::cout << "进入事件循环..." << std::endl;
    
    while (running_) {
        // 等待事件，超时时间为1秒
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;  // 被信号中断，继续
            }
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (nfds == 0) {
            // 超时
            continue;
        }
        
        // 处理所有就绪的事件
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;
            
            // 检查错误事件
            if (event_mask & (EPOLLERR | EPOLLHUP)) {
                std::cerr << "Epoll错误事件发生在fd " << fd << std::endl;
                close_connection(fd);
                continue;
            }
            
            if (fd == server_fd_) {
                // 新连接
                handle_accept();
            } else {
                // 客户端事件
                if (event_mask & EPOLLIN) {
                    handle_read(fd);
                }
                if (event_mask & EPOLLOUT) {
                    handle_write(fd);
                }
            }
        }
    }
}

void EpollEchoServer::handle_accept() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 水平触发模式下，只需要接受一个连接
    int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Accept error: " << strerror(errno) << std::endl;
        }
        return;
    }
    
    // 设置为非阻塞模式
    set_non_blocking(client_fd);
    
    // 创建客户端数据
    auto client_data = std::make_shared<ClientData>(client_fd, client_addr);
    clients_[client_fd] = client_data;
    
    // 添加到epoll，监听读事件（水平触发）
    add_epoll_event(client_fd, EPOLLIN);
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "接受来自 " << client_ip << ":" << ntohs(client_addr.sin_port) 
              << " 的新连接 (fd: " << client_fd << ")" << std::endl;
    
    // 发送欢迎消息
    std::string welcome_msg = "Welcome to Echo Server! Send any message and I'll echo it back.\n";
    client_data->send_buffer = welcome_msg;
    
    // 修改为监听读写事件，以便发送欢迎消息
    modify_epoll_event(client_fd, EPOLLIN | EPOLLOUT);
}

void EpollEchoServer::handle_read(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }
    
    auto& client_data = it->second;
    char buffer[1024];
    
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        client_data->recv_buffer.append(buffer, bytes_read);
        
        // 输出接收到的数据
        std::cout << "从客户端 " << client_fd << " 收到 " << bytes_read 
                  << " 字节: " << std::string(buffer, bytes_read);
        
        // Echo逻辑：将接收到的数据放入发送缓冲区
        client_data->send_buffer.append(buffer, bytes_read);
        
        // 有数据要发送，确保监听写事件
        if (!client_data->send_buffer.empty()) {
            modify_epoll_event(client_fd, EPOLLIN | EPOLLOUT);
        }
        
    } else if (bytes_read == 0) {
        // 客户端关闭连接
        std::cout << "客户端 " << client_fd << " 断开连接" << std::endl;
        close_connection(client_fd);
    } else {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "从客户端 " << client_fd << " 读取错误: " << strerror(errno) << std::endl;
            close_connection(client_fd);
        }
    }
}

void EpollEchoServer::handle_write(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }
    
    auto& client_data = it->second;
    
    if (client_data->send_buffer.empty()) {
        // 没有数据要发送，只监听读事件
        modify_epoll_event(client_fd, EPOLLIN);
        return;
    }
    
    ssize_t bytes_sent = send(client_fd, 
                             client_data->send_buffer.data(), 
                             client_data->send_buffer.size(), 
                             0);
    
    if (bytes_sent > 0) {
        std::cout << "向客户端 " << client_fd << " 发送 " << bytes_sent << " 字节" << std::endl;
        client_data->send_buffer.erase(0, bytes_sent);
        
        // 如果所有数据都已发送，只监听读事件
        if (client_data->send_buffer.empty()) {
            modify_epoll_event(client_fd, EPOLLIN);
        }
    } else {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "向客户端 " << client_fd << " 发送错误: " << strerror(errno) << std::endl;
            close_connection(client_fd);
        }
        // 如果是EWOULDBLOCK，保持当前的事件监听，下次再尝试发送
    }
}

void EpollEchoServer::close_connection(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it != clients_.end()) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &it->second->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "关闭连接 " << client_ip << ":" << ntohs(it->second->addr.sin_port) 
                  << " (fd: " << client_fd << ")" << std::endl;
        
        clients_.erase(it);
    }
    
    remove_epoll_event(client_fd);
    close(client_fd);
}
