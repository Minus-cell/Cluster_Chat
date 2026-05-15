#include "groupmodel.hpp"
#include "db.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// 创建群组
bool GroupModel::insert(Group &group)
{
    MySQL mysql;
    if (!mysql.connect()) return false;

    MYSQL *conn = mysql.getConnection();
    char name_buf[128] = {0};
    char desc_buf[512] = {0};

    mysql_real_escape_string(conn, name_buf, group.getName().c_str(), group.getName().size());
    mysql_real_escape_string(conn, desc_buf, group.getDesc().c_str(), group.getDesc().size());

    char sql[1024] = {0};
    snprintf(sql, sizeof(sql),
             "INSERT INTO chat_group (groupname, groupdesc) VALUES ('%s', '%s')",
             name_buf, desc_buf);

    if (mysql.update(sql))
    {
        group.setId(mysql_insert_id(conn));
        return true;
    }
    return false;
}

// 根据群组ID查询群组信息
Group GroupModel::query(int groupid)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT id, groupname, groupdesc FROM chat_group WHERE id = %d", groupid);

    MySQL mysql;
    if (!mysql.connect()) return Group();

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return Group();

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row)
    {
        Group group(atoi(row[0]), row[1] ? row[1] : "", row[2] ? row[2] : "");
        mysql_free_result(res);
        return group;
    }
    mysql_free_result(res);
    return Group();
}

// 根据群组名称查询群组
Group GroupModel::queryByName(const string &name)
{
    MySQL mysql;
    if (!mysql.connect()) return Group();

    MYSQL *conn = mysql.getConnection();
    char name_buf[128] = {0};
    mysql_real_escape_string(conn, name_buf, name.c_str(), name.size());

    char sql[512] = {0};
    snprintf(sql, sizeof(sql), "SELECT id, groupname, groupdesc FROM chat_group WHERE groupname = '%s'", name_buf);

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return Group();

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row)
    {
        Group group(atoi(row[0]), row[1] ? row[1] : "", row[2] ? row[2] : "");
        mysql_free_result(res);
        return group;
    }
    mysql_free_result(res);
    return Group();
}

// 获取所有群组列表
vector<Group> GroupModel::getAllGroups()
{
    const char *sql = "SELECT id, groupname, groupdesc FROM chat_group";
    vector<Group> vec;

    MySQL mysql;
    if (!mysql.connect()) return vec;

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return vec;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        Group group(atoi(row[0]), row[1] ? row[1] : "", row[2] ? row[2] : "");
        vec.push_back(group);
    }
    mysql_free_result(res);
    return vec;
}

bool GroupModel::remove(int groupid) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM chat_group WHERE id = %d", groupid);
    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}