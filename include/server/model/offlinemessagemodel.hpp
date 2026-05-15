#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include <string>
#include <vector>
#include <cstdint>

using namespace std;

// 表示一条离线消息的结构体（便于业务层使用）
struct OfflineMessage {
    int64_t id;          // 消息ID
    int to_userid;       // 接收者
    int from_userid;     // 发送者
    int groupid;         // 群组ID，私聊为 0
    string content;      // 消息内容
    string send_time;    // 发送时间，格式 "YYYY-MM-DD HH:MM:SS"
    int is_read;         // 0 未读，1 已读
};

// 提供离线消息表的操作接口
class OfflineMsgModel {
public:
    // 新增：插入一条离线消息（推荐使用）
    bool insert(int to_userid, int from_userid, const string &content,
                int groupid = 0, const string &send_time = "");

    // 删除某个接收者的所有离线消息
    bool remove(int to_userid);

    // 删除某一条特定的离线消息（按消息ID）
    bool removeById(int64_t msgid);

    // 查询某个用户的离线消息，返回结构体列表（推荐）
    vector<OfflineMessage> query(int to_userid);

    // 查询某个用户的离线消息，返回 JSON 字符串列表（兼容旧接口）
    vector<string> queryAsJson(int to_userid);

    // 将某条消息标记为已读
    bool markAsRead(int64_t msgid);
};

#endif