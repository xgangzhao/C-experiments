#include "epoll_echo_server.h"
#include <csignal>
#include <iostream>

EpollEchoServer* g_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\n接收到信号 " << signal << ", 正在关闭服务器..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main() {
    std::cout << "启动 Epoll Echo 服务器..." << std::endl;
    
    try {
        EpollEchoServer server("0.0.0.0", 8888);
        g_server = &server;
        
        // 设置信号处理
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        std::cout << "服务器已启动，使用 telnet localhost 8888 进行测试" << std::endl;
        std::cout << "按 Ctrl+C 停止服务器" << std::endl;
        
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "服务器已关闭" << std::endl;
    return 0;
}
