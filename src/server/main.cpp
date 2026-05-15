#include "chatserver.hpp"
#include <iostream>
#include "chatservice.hpp"
#include "connectionpool.h"
#include <signal.h>
using namespace std;

// 全局事件循环指针
EventLoop *g_loop = nullptr;
//处理服务器ctrl+c结束后，重置服务器user状态
void resetHandler(int)
{ 
    //ChatService::instance()->reset();
    if (g_loop) {
        g_loop->quit();   // 唤醒 loop，让它正常返回
    }
}
int main(int argc, char *argv[])
{
    int port = 6000;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    std::cout << "Starting server on port: " << port << std::endl; 
    // 1. 初始化数据库连接池（必须在任何数据库操作之前调用）
    if (!ConnectionPool::instance()->init("127.0.0.1", 3306, "my_admin", "123456", "chat", 4, 16)) {
        cerr << "Failed to init connection pool" << endl;
        return 1;
    }
    // 2. 注册信号处理
    signal(SIGINT,resetHandler);
    EventLoop loop;
    g_loop = &loop;     // 赋值给全局指针
    // 3. 重置所有可能残留的 online 状态（启动时清理）
    ChatService::instance()->reset();


    // 4. 启动服务器
    InetAddress addr("127.0.0.1", port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    // 循环退出后，再执行一次重置，保证优雅关闭时的状态清理
    ChatService::instance()->reset();
    return 0;
}