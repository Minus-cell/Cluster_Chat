#ifndef PUBLIC_H
#define PUBLIC_H
/*
server和client的公共文件
*/
enum EnMsgType
{
    LOGIN_MSG = 1,    // 登录消息
    LOGIN_MSG_ACK,    // 登录响应消息
    REG_MSG,          // 注册消息
    REG_MSG_ACK,      // 注册响应消息
    ONE_CHAT_MSG,     // 聊天信息
    ADD_FRIEND_MSG,   // 添加好友信息
    CREATE_GROUP_MSG, // 创建群组
    CREATE_GROUP_ACK, //响应消息
    JOIN_GROUP_MSG,   // 加入群组
    JOIN_GROUP_MSG_ACK,//响应消息
    LEAVE_GROUP_MSG,  // 退出群组
    LEAVE_GROUP_MSG_ACK,
    GROUP_CHAT_MSG,   // 群聊消息
    GROUP_INFO_MSG,   // 查询群组信息/成员
    GROUP_INFO_MSG_ACK, // 查询群组信息/成员响应消息
    KICK_MSG_ACK = 100 // 被踢下线通知（服务端主动发送给被顶号的客户端）
};

// 登录相关错误码
enum LoginErrno {
    LOGIN_OK            = 0,    // 登录成功
    LOGIN_USER_NOT_EXIST = 1,   // 用户不存在
    LOGIN_WRONG_PASSWORD = 2,   // 密码错误
    LOGIN_ALREADY_ONLINE = 3,    // 已在线
    LOGIN_KICKED = 100   // 账号在其他地方登录（被踢）
};

// 注册相关错误码
enum RegisterErrno {
    REGISTER_OK             = 0,   // 注册成功
    REGISTER_USER_EXISTS    = 1,   // 用户名已存在（需在 model 中检测）
    REGISTER_NAME_EMPTY     = 2,   // 用户名为空
    REGISTER_PWD_EMPTY      = 3,   // 密码为空
    REGISTER_NAME_TOO_LONG  = 4,   // 用户名过长
    REGISTER_PWD_TOO_SHORT  = 5    // 密码过短
};

// 添加群组相关错误码
enum GroupErrno {
    GROUP_OK              = 0,
    GROUP_NOT_EXIST       = 1,   // 群组不存在
    GROUP_ALREADY_EXIST   = 2,   // 群组名已存在
    GROUP_USER_EXISTS     = 3,   // 用户已经在群中
    GROUP_USER_NOT_FOUND  = 4,   // 用户不在群中
    GROUP_PERMISSION_DENY = 5    // 权限不足（非群主操作等）
};

// 以后可以继续扩展好友、群组等错误码
#endif

