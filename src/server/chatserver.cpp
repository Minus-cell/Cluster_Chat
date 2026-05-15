#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"
#include "muduo/base/Logging.h"
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;


ChatServer::ChatServer(EventLoop *loop,
            const InetAddress& listenAddr,
            const string& nameArg)
            :_server(loop,listenAddr,nameArg)
            ,_loop(loop)          
{
    //注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));

    //注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));

    //设置线程数量
    _server.setThreadNum(4);


}

// 启动服务
void ChatServer::ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    //客户端断来连接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
               Buffer *buffer,
               Timestamp time)
{
    string line = buffer->retrieveAllAsString();
    try {
        json js = json::parse(line);
        auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
        msgHandler(conn, js, time);
    } catch (const json::parse_error &e) {
        LOG_ERROR << "JSON parse error: " << e.what() << ", raw: " << line;
        // 可以选择关闭连接或忽略
    }

}