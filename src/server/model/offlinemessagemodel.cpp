#include "offlinemessagemodel.hpp"
#include "db.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "json.hpp"
#include <muduo/base/Logging.h>

using json = nlohmann::json;

// ---- 工具函数：生成当前时间字符串 ----
static string now_time() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return buf;
}

// ---- 插入离线消息 ----
bool OfflineMsgModel::insert(int to_userid, int from_userid,
                             const string &content, int groupid,
                             const string &send_time) {
    string sendTime = send_time.empty() ? now_time() : send_time;

    MySQL mysql;
    if (!mysql.connect()) return false;

    MYSQL *conn = mysql.getConnection();

    // content 转义缓冲区（原始内容最长可能很长，这里暂定 4096，可根据需要调整）
    const size_t MAX_CONTENT_LEN = 4096;
    char content_buf[MAX_CONTENT_LEN * 2 + 1] = {0};  // 转义后最大长度
    mysql_real_escape_string(conn, content_buf, content.c_str(), content.size());

    // SQL 缓冲区扩大为 8192，足以容纳最长的转义内容
    const size_t SQL_BUF_SIZE = 8192;
    char sql[SQL_BUF_SIZE] = {0};
    int needed = 0;

    if (groupid == 0) {
        needed = snprintf(sql, SQL_BUF_SIZE,
                 "INSERT INTO offline_message (to_userid, from_userid, groupid, content, send_time) "
                 "VALUES (%d, %d, NULL, '%s', '%s')",
                 to_userid, from_userid, content_buf, sendTime.c_str());
    } else {
        needed = snprintf(sql, SQL_BUF_SIZE,
                 "INSERT INTO offline_message (to_userid, from_userid, groupid, content, send_time) "
                 "VALUES (%d, %d, %d, '%s', '%s')",
                 to_userid, from_userid, groupid, content_buf, sendTime.c_str());
    }

    // 检查是否因长度不足被截断
    if (needed < 0 || needed >= (int)SQL_BUF_SIZE) {
        // 输出错误日志或直接返回失败
        LOG_ERROR << "SQL buffer too small, needed " << needed << " bytes";
        return false;
    }

    return mysql.update(sql);
}

// ---- 删除某用户所有离线消息 ----
bool OfflineMsgModel::remove(int to_userid) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM offline_message WHERE to_userid = %d", to_userid);
    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}

// ---- 删除指定消息ID ----
bool OfflineMsgModel::removeById(int64_t msgid) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM offline_message WHERE id = %ld", msgid);
    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}

// ---- 查询结构体列表 ----
vector<OfflineMessage> OfflineMsgModel::query(int to_userid) {
    vector<OfflineMessage> result;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT id, to_userid, from_userid, ifnull(groupid,0), content, send_time, is_read "
             "FROM offline_message WHERE to_userid = %d ORDER BY send_time", to_userid);

    MySQL mysql;
    if (!mysql.connect()) return result;

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        OfflineMessage m;
        m.id          = row[0] ? atoll(row[0]) : 0;
        m.to_userid   = row[1] ? atoi(row[1]) : 0;
        m.from_userid = row[2] ? atoi(row[2]) : 0;
        m.groupid     = row[3] ? atoi(row[3]) : 0;
        m.content     = row[4] ? row[4] : "";
        m.send_time   = row[5] ? row[5] : "";
        m.is_read     = row[6] ? atoi(row[6]) : 0;
        result.push_back(m);
    }
    mysql_free_result(res);
    return result;
}

// ---- 查询并返回 JSON 字符串列表（兼容旧接口） ----
vector<string> OfflineMsgModel::queryAsJson(int to_userid) {
    vector<string> jsons;
    auto msgs = query(to_userid);
    for (const auto &m : msgs) {
        json j;
        j["msgid"]    = m.id;
        j["id"]       = m.from_userid;   // 旧字段名：发送者 id
        j["msg"]      = m.content;       // 旧字段名：消息内容
        j["groupid"]  = m.groupid;
        j["time"]     = m.send_time;
        j["is_read"]  = m.is_read;
        // 注意：原 JSON 里可能还有 "name"，这里无法获取发送者姓名，需要业务层补充
        jsons.push_back(j.dump());
    }
    return jsons;
}

// ---- 标记消息已读 ----
bool OfflineMsgModel::markAsRead(int64_t msgid) {
    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE offline_message SET is_read=1 WHERE id=%ld", msgid);
    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}