#include "reactor_echo_server.h"
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> g_running{true};

int main() {
    try {
        ReactorEchoServer server("0.0.0.0", 8888);
        
        std::cout << "服务器启动中... 按 Ctrl+C 停止服务器" << std::endl;
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "程序正常退出" << std::endl;
    return 0;
}
