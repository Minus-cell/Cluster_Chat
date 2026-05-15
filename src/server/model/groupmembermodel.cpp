#include "groupmembermodel.hpp"
#include "db.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// 添加群成员
bool GroupMemberModel::insert(int groupid, int userid, int role)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "INSERT IGNORE INTO group_member (groupid, userid, grouprole) VALUES (%d, %d, %d)",
             groupid, userid, role);

    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}

// 移除群成员
bool GroupMemberModel::remove(int groupid, int userid)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "DELETE FROM group_member WHERE groupid = %d AND userid = %d",
             groupid, userid);

    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}

// 查询某个群组的所有成员（返回 User 列表）
vector<User> GroupMemberModel::queryUsersByGroup(int groupid)
{
    char sql[512] = {0};
    snprintf(sql, sizeof(sql),
             "SELECT u.id, u.name, u.state FROM user u "
             "INNER JOIN group_member gm ON u.id = gm.userid "
             "WHERE gm.groupid = %d", groupid);

    vector<User> vec;
    MySQL mysql;
    if (!mysql.connect()) return vec;

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return vec;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        User user;
        user.setId(atoi(row[0]));
        user.setName(row[1] ? row[1] : "");
        user.setState(row[2] ? row[2] : "offline");
        vec.push_back(user);
    }
    mysql_free_result(res);
    return vec;
}

// 查询某个用户加入的群组ID列表
vector<int> GroupMemberModel::queryGroupsByUser(int userid)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "SELECT groupid FROM group_member WHERE userid = %d", userid);

    vector<int> vec;
    MySQL mysql;
    if (!mysql.connect()) return vec;

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return vec;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        vec.push_back(atoi(row[0]));
    }
    mysql_free_result(res);
    return vec;
}

// 查询用户在指定群组中的角色
int GroupMemberModel::getRole(int groupid, int userid)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "SELECT grouprole FROM group_member WHERE groupid = %d AND userid = %d",
             groupid, userid);

    MySQL mysql;
    if (!mysql.connect()) return -1;

    MYSQL_RES *res = mysql.query(sql);
    if (!res) return -1;

    MYSQL_ROW row = mysql_fetch_row(res);
    int role = -1;
    if (row)
    {
        role = atoi(row[0]);
    }
    mysql_free_result(res);
    return role;
}

// 更新用户角色
bool GroupMemberModel::updateRole(int groupid, int userid, int newrole)
{
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "UPDATE group_member SET grouprole = %d WHERE groupid = %d AND userid = %d",
             newrole, groupid, userid);

    MySQL mysql;
    if (!mysql.connect()) return false;
    return mysql.update(sql);
}