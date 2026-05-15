#ifndef GROUPMEMBERMODEL_H
#define GROUPMEMBERMODEL_H

#include <vector>
#include <utility>   // for pair
#include "user.hpp"  // 为了返回 User 时用
using namespace std;

// 群成员角色（对应 grouprole 字段）
enum GroupRole {
    ROLE_NORMAL = 0,
    ROLE_CREATOR = 1,
    // 可扩展 2: admin 等
};

// 操作 group_member 表的接口
class GroupMemberModel
{
public:
    // 向群组添加成员（可指定角色，默认普通成员）
    bool insert(int groupid, int userid, int role = ROLE_NORMAL);

    // 从群组移除成员
    bool remove(int groupid, int userid);

    // 查询某个群组的所有成员（返回用户信息列表）
    vector<User> queryUsersByGroup(int groupid);

    // 查询某个用户加入的群组ID列表
    vector<int> queryGroupsByUser(int userid);

    // 查询用户在指定群组中的角色
    int getRole(int groupid, int userid);

    // 更新用户角色（例如设置管理员）
    bool updateRole(int groupid, int userid, int newrole);
};

#endif