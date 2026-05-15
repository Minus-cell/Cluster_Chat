#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表的数据操作类
class UserModel {
public:
    // User表的增加方法
    bool insert(User &user);
    //根据用户号码查询信息
    User query(int id);
    //更新用户状态
    bool updateState(User &user); 
    //用于服务器异常崩溃之后，重启服务器，改变所有在线用户的登录状态
    bool resetAllStateToOffline();
};

#endif