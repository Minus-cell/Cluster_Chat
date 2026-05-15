#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <vector>
using namespace std;

// 操作 chat_group 表的接口
class GroupModel
{
public:
    // 创建群组
    bool insert(Group &group);

    // 根据群组ID查询群组信息
    Group query(int groupid);

    // 根据群组名称查询群组（唯一）
    Group queryByName(const string &name);

    // 获取所有群组列表（可扩展）
    vector<Group> getAllGroups();
    // 删除群组
    bool remove(int groupid);   
};

#endif