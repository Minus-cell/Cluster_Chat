#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>

using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "redis.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "json.hpp"
#include "friendmodel.hpp"
#include "group.hpp"
#include "groupmembermodel.hpp"
#include "groupmodel.hpp"
using json = nlohmann::json;
//处理消息的事件回调方法类型
using MsgHandler=std::function<void(const TcpConnectionPtr &conn,json &js,Timestamp)>;


//聊天服务器业务类    ->单例模式
class ChatService
{

public:     
    //获取单例对象接口函数
    static ChatService* instance();

    //处理登录业务
    void login(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //服务器异常崩溃，重置方法
    void reset();
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);

    // 群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void joinGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void leaveGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获取群详情+成员列表
    void queryGroupInfo(const TcpConnectionPtr &conn, json &js, Timestamp time); 
    // 处理 redis 发布的消息
    void handleRedisMessage(int channel, std::string msg);


private:
    ChatService();
    
    
    //存储消息id和其对应的业务处理方法
    unordered_map<int,MsgHandler> _msgHandleMap;
    
    //存储在线用户的通信连接
    unordered_map<int,TcpConnectionPtr> _userConnMap;
    
    //数据操作类对象
    UserModel _userModel;

    //定义互斥锁,保证_userConnMap线程安全
    mutex _connMutex;
    //离线消息操作类对象
    OfflineMsgModel _offlineMsgModel;
    //好友操作类对象
    FriendModel _friendModel;
    //群组操作对象
    GroupModel _groupModel;
    GroupMemberModel _groupMemberModel;

    Redis _redis;
};


#endif