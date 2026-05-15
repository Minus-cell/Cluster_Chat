#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>

#include "user.hpp"
#include "bcrypt.h"

using namespace std;
using namespace muduo;
//获取单例对象的接口函数
ChatService* ChatService::instance()
{   
    static ChatService service;
    return &service;
}
//注册消息以及对应的handle回调
ChatService::ChatService()
{
    _msgHandleMap.insert({LOGIN_MSG,[this](const TcpConnectionPtr &conn, json &js, Timestamp time){
        login(conn, js, time);
    }});//使用lamba表达式代替bind
    _msgHandleMap.insert({REG_MSG,[this](const TcpConnectionPtr &conn,json &js,Timestamp time){
        reg(conn,js,time);
    }});

    _msgHandleMap.insert({ONE_CHAT_MSG,[this](const TcpConnectionPtr &conn,json &js,Timestamp time){
        oneChat(conn,js,time);
    }});

    _msgHandleMap.insert({ADD_FRIEND_MSG,[this](const TcpConnectionPtr &conn,json &js,Timestamp time){
        addFriend(conn,js,time);
    }});
    //群组业务处理的回调
    _msgHandleMap.insert({CREATE_GROUP_MSG, [this](auto&&... args){ this->createGroup(args...); }});
    _msgHandleMap.insert({JOIN_GROUP_MSG,  [this](auto&&... args){ this->joinGroup(args...); }});
    _msgHandleMap.insert({LEAVE_GROUP_MSG, [this](auto&&... args){ this->leaveGroup(args...); }});
    _msgHandleMap.insert({GROUP_CHAT_MSG,  [this](auto&&... args){ this->groupChat(args...); }});
    _msgHandleMap.insert({GROUP_INFO_MSG,  [this](auto&&... args){ this->queryGroupInfo(args...); }});
    //连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息回调
        _redis.init_notify_handler([this](int channel, string msg) {
                this->handleRedisMessage(channel, msg);
        });
    }

}

 //获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志，msgid没有对应的事件处理回调
    auto it=_msgHandleMap.find(msgid);
    if(it==_msgHandleMap.end())
    {
        //返回一个默认的处理器，空处理器
        return [=](const TcpConnectionPtr &conn,json &js,Timestamp time){
            LOG_ERROR<<"msgid:"<<msgid<<"can not find handler!";
        };
       
    }
    else
    {
        return _msgHandleMap[msgid];    
    }
   
}

//处理登录业务 ORM  业务层操作对象  在数据层操作数据库
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js.value("id", -1);
    string pwd = js.value("password", "");

    // 1. 查询用户
    User user = _userModel.query(id);
    if (user.getId() == -1)
    {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = LOGIN_USER_NOT_EXIST;
        conn->send(response.dump()+"\n");
        return;
    }

    // 2. 验证密码
    if (!bcrypt::validatePassword(pwd, user.getPwd()))
    {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = LOGIN_WRONG_PASSWORD;
        response["id"] = user.getId();
        conn->send(response.dump()+"\n");
        return;
    }

    // 3. 顶号处理：检查是否已有该用户的在线连接
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 强制关闭旧连接（不发送任何消息）
            it->second->shutdown();
            _userConnMap.erase(it);
        }
    }

    // 4. 更新数据库状态为 online（展示用）
    user.setState("online");
    _userModel.updateState(user);

    // 5. 将新连接加入在线列表
    {
        lock_guard<mutex> lock(_connMutex);
        _userConnMap.insert({id, conn});
    }
    //登录成功后，先redis订阅channel(id)
    _redis.subscribe(id);

    // 6. 推送离线消息（在返回登录成功之前）
    auto offlineMsgs = _offlineMsgModel.query(id);   // 返回结构体列表
    if (!offlineMsgs.empty())
    {
        for (const auto &msg : offlineMsgs)
        {
            json offlineJs;
            if (msg.groupid != 0)
            { // 群聊离线消息
                offlineJs["msgid"] = GROUP_CHAT_MSG;
                offlineJs["groupid"] = msg.groupid;
            }
            else
            { // 私聊离线消息
                offlineJs["msgid"] = ONE_CHAT_MSG;
            }
            offlineJs["id"] = msg.from_userid;
            offlineJs["msg"] = msg.content;
            offlineJs["time"] = msg.send_time;
            conn->send(offlineJs.dump() + "\n");
        }
        // 删除已推送的离线消息
        _offlineMsgModel.remove(id);
    }

    
    // 7. 构建登录成功消息
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    response["errno"] = LOGIN_OK;
    response["id"] = user.getId();
    response["name"] = user.getName();

    // 好友列表
    vector<User> userVec = _friendModel.query(id);
    if (!userVec.empty())
    {
        json friendsArr = json::array();
        for (User &u : userVec)
        {
            json friendJs;
            friendJs["id"] = u.getId();
            friendJs["name"] = u.getName();
            friendJs["state"] = u.getState();
            friendsArr.push_back(friendJs);
        }
        response["friends"] = friendsArr;
    }
    //群组列表处理
    // 查询该用户已加入的群组并返回基本信息
    vector<int> groupIdVec = _groupMemberModel.queryGroupsByUser(id);
    if (!groupIdVec.empty())
    {
        json groupsArr = json::array();
        for (int gid : groupIdVec)
        {
            Group group = _groupModel.query(gid);
            if (group.getId() != -1) // 确保群组存在
            {
                json groupJs;
                groupJs["id"] = group.getId();
                groupJs["groupname"] = group.getName();
                groupJs["groupdesc"] = group.getDesc();
                // 可按需添加 role 等信息
                groupsArr.push_back(groupJs);
            }
        }
        response["groups"] = groupsArr;
    }
    //返回登录成功
    conn->send(response.dump()+"\n");

}
//处理注册业务  -->缺点没有检查用户名和密码是否为空，没有对长度进行限制
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // ----- 1. 提取数据 -----
    string name = js["name"];
    string pwd  = js["password"];

    // ----- 2. 参数校验（按顺序检查，一旦出错立刻返回）-----
    json response;
    response["msgid"] = REG_MSG_ACK;

    // 用户名为空
    if (name.empty()) {
        response["errno"] = REGISTER_NAME_EMPTY;
        conn->send(response.dump()+"\n");
        return;
    }

    // 用户名长度限制（例如最长 20 个字符，根据业务调整）
    if (name.length() > 20) {
        response["errno"] = REGISTER_NAME_TOO_LONG;
        conn->send(response.dump()+"\n");
        return;
    }

    // 密码为空
    if (pwd.empty()) {
        response["errno"] = REGISTER_PWD_EMPTY;
        conn->send(response.dump()+"\n");
        return;
    }

    // 密码长度限制（例如最短 6 位）
    if (pwd.length() < 6) {
        response["errno"] = REGISTER_PWD_TOO_SHORT;
        conn->send(response.dump()+"\n");
        return;
    }

    // ----- 3. 构造用户对象并尝试插入 -----
    User user;
    user.setName(name);
    user.setPwd(pwd);          // 明文，Model 层会 bcrypt 加密
    bool success = _userModel.insert(user);

    // ----- 4. 根据插入结果返回响应 -----
    if (success) {
        response["errno"] = REGISTER_OK;
        response["id"]    = user.getId();
        conn->send(response.dump()+"\n");
    } else {
        // 插入失败，极大概率是用户名重复
        response["errno"] = REGISTER_USER_EXISTS;
        conn->send(response.dump()+"\n");
    }
}
void ChatService::reset()
{
    // 启动前，将所有可能残留的 online 用户置为 offline
    _userModel.resetAllStateToOffline();
}
//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    int userId = -1;
    
    // 1. 从在线 map 中找出该连接对应的用户 ID，并移除
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                userId = it->first;
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 2. 如果找到了用户，判断是否还有s其他连接（即是否已被顶号替换）
    if (userId != -1)
    {
        bool stillOnline = false;
        {
            lock_guard<mutex> lock(_connMutex);
            // 如果 map 中仍然存在该 userId，说明有另一个新连接（顶号场景），不应置为离线
            stillOnline = (_userConnMap.find(userId) != _userConnMap.end());
        }

        if (!stillOnline)
        {
            _redis.unsubscribe(userId);   // 取消订阅，不再接收消息
            // 没有其他连接，正常下线，更新数据库为 offline
            User user;
            user.setId(userId);
            user.setState("offline");
            _userModel.updateState(user);
        }
        // 如果有其他连接，什么都不做，因为新连接会负责保持 online 状态
    }
}
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    int fromid = js["id"].get<int>();            // 发送者id（客户端应提供）
    string msgContent = js["msg"];

    // 检查接收方是否在线
    TcpConnectionPtr toConn;
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            toConn = it->second;
        }
    }

    if (toConn) {
        // 在线，直接转发
        toConn->send(js.dump()+"\n");
    } 
    else {
        // 不在本机，通过 Redis 发布到 toid 频道
        _redis.publish(toid, js.dump());
    }
}
//添加好友  id  friendid
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

//创建群组
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js.value("groupdesc", "");

    json response;
    response["msgid"] = CREATE_GROUP_ACK;   

    // 检查群名合法性
    if (groupname.empty()) {
        response["errno"] = GROUP_ALREADY_EXIST; // 借用错误码，最好新建一个
        conn->send(response.dump() + "\n");
        return;
    }

    Group group;
    group.setName(groupname);
    group.setDesc(groupdesc);

    bool success = _groupModel.insert(group);
    if (!success) {
        response["errno"] = GROUP_ALREADY_EXIST; // 群名重复
        conn->send(response.dump() + "\n");
        return;
    }

    // 创建者自动成为群主
    _groupMemberModel.insert(group.getId(), userid, ROLE_CREATOR);

    response["errno"] = GROUP_OK;
    response["groupid"] = group.getId();
    conn->send(response.dump() + "\n");
}
//加入群组
void ChatService::joinGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    response["msgid"] = JOIN_GROUP_MSG_ACK ;// 或 ACK

    // 1. 检查群是否存在
    Group group = _groupModel.query(groupid);
    if (group.getId() == -1) {
        response["errno"] = GROUP_NOT_EXIST;
        conn->send(response.dump() + "\n");
        return;
    }

    // 2. 检查是否已在群中
    int role = _groupMemberModel.getRole(groupid, userid);
    if (role != -1) {
        response["errno"] = GROUP_USER_EXISTS;
        conn->send(response.dump() + "\n");
        return;
    }

    // 3. 插入成员（默认普通成员）
    bool ok = _groupMemberModel.insert(groupid, userid, ROLE_NORMAL);
    response["errno"] = ok ? GROUP_OK : GROUP_ALREADY_EXIST; // 简单处理
    conn->send(response.dump() + "\n");
}
//退出群组
void ChatService::leaveGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    response["msgid"] = LEAVE_GROUP_MSG_ACK;

    // 1. 检查用户是否在群中
    int role = _groupMemberModel.getRole(groupid, userid);
    if (role == -1) {
        response["errno"] = GROUP_USER_NOT_FOUND;
        conn->send(response.dump() + "\n");
        return;
    }

    // 2. 如果用户是群主
    if (role == ROLE_CREATOR) {
        // 查询当前群成员总数
        vector<User> members = _groupMemberModel.queryUsersByGroup(groupid);
        int memberCount = members.size();

        // 如果群内还有其他人（数量大于1），则不能退出
        if (memberCount > 1) {
            response["errno"] = GROUP_PERMISSION_DENY;
            response["hint"] = "You are the group owner. Please transfer ownership before leaving, or dissolve the group if it's empty.";
            conn->send(response.dump() + "\n");
            return;
        }
        // 群主是唯一成员，可以退出并解散群组
        // 先移除群成员记录，再删除群组
        _groupMemberModel.remove(groupid, userid);
        _groupModel.remove(groupid);    // 删除群组，级联删除所有成员记录

        response["errno"] = GROUP_OK;
        conn->send(response.dump() + "\n");
        return;
    }

    // 3. 普通成员退出
    bool ok = _groupMemberModel.remove(groupid, userid);
    response["errno"] = ok ? GROUP_OK : GROUP_USER_NOT_FOUND;
    conn->send(response.dump() + "\n");
}

// 群聊消息（转发 + 离线存储）
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int fromid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    string msgContent = js["msg"];
    string send_time = js.value("time", "");

    // 1. 获取群成员列表
    vector<User> members = _groupMemberModel.queryUsersByGroup(groupid);
    if (members.empty()) {
        // 群不存在或没有成员（异常情况）
        return;
    }

    // 2. 遍历成员，在线则直接转发，离线则存储
    for (const auto &user : members) {
        if (user.getId() == fromid) continue; // 不发给自己

        TcpConnectionPtr toConn;
        {
            lock_guard<mutex> lock(_connMutex);
            auto it = _userConnMap.find(user.getId());
            if (it != _userConnMap.end()) {
                toConn = it->second;
            }
        }

        if (toConn) {
            // 在线：转发群聊消息（带上发送者和群组信息）
            json forwardJs = js;  // 复制原消息，保留 id, groupid, msg, time
            forwardJs["msgid"] = GROUP_CHAT_MSG; // 确保 msgid 是群聊类型
            toConn->send(forwardJs.dump() + "\n");
        } else {
              // 不在本机，通过 Redis 发布到用户个人频道
            json redisJs = js;
            redisJs["msgid"] = GROUP_CHAT_MSG;
            redisJs["to"] = user.getId();       // 接收者
            _redis.publish(user.getId(), redisJs.dump());
        }
    }
}

//查询群组信息和成员列表
void ChatService::queryGroupInfo(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int groupid = js["groupid"].get<int>();
    Group group = _groupModel.query(groupid);
    if (group.getId() == -1) {
        json resp;
        resp["msgid"] = GROUP_INFO_MSG;
        resp["errno"] = GROUP_NOT_EXIST;
        conn->send(resp.dump() + "\n");
        return;
    }

    vector<User> members = _groupMemberModel.queryUsersByGroup(groupid);
    json membersArr = json::array();
    for (auto &u : members) {
        json m;
        m["id"] = u.getId();
        m["name"] = u.getName();
        m["state"] = u.getState();
        // 还可以查询角色信息，这里略
        membersArr.push_back(m);
    }

    json resp;
    resp["msgid"] = GROUP_INFO_MSG_ACK; // 或 GROUP_INFO_MSG_ACK
    resp["errno"] = GROUP_OK;
    resp["groupid"] = group.getId();
    resp["groupname"] = group.getName();
    resp["groupdesc"] = group.getDesc();
    resp["members"] = membersArr;
    conn->send(resp.dump() + "\n");
}

// 处理 redis 订阅频道收到的消息
void ChatService::handleRedisMessage(int channel, std::string msg)
{
    try
    {
        json js = json::parse(msg);
        int msgid = js["msgid"].get<int>();

        if (msgid == ONE_CHAT_MSG || msgid == GROUP_CHAT_MSG)
        {
            int toid = js["to"].get<int>(); // 接收者 id
            {
                std::lock_guard<std::mutex> lock(_connMutex);
                auto it = _userConnMap.find(toid);
                if (it != _userConnMap.end())
                {
                    // 用户在本机在线，直接转发
                    it->second->send(msg + "\n");
                    return;
                }
            }

            // 用户不在本机，存储离线消息
            int fromid = js["id"].get<int>();
            std::string content = js["msg"];
            std::string send_time = js.value("time", "");
            int groupid = js.value("groupid", 0); // 私聊为 0

            _offlineMsgModel.insert(toid, fromid, content, groupid, send_time);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "handleRedisMessage error: " << e.what();
    }
}
