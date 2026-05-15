# Cluster_Chat

**Muduo-based High-performance C++ Cluster IM Server**

一个采用分层架构设计的分布式集群即时通讯系统，基于 Muduo 高性能网络库开发，支持多节点水平扩展。实现了用户注册登录、一对一聊天、群组聊天、好友管理、离线消息存储等核心功能，使用 Nginx TCP 反向代理实现负载均衡，Redis 发布订阅实现跨节点消息通信。

---

## ✨ 技术栈

- **网络层**：Muduo 高性能 Reactor 网络库
- **数据库**：MySQL 8.0 + 数据库连接池
- **集群支持**：Nginx TCP 反向代理（负载均衡） + Redis 发布订阅（跨节点消息路由）
- **构建工具**：CMake 3.10+
- **序列化**：nlohmann/json 单头文件 JSON 库
- **安全**：bcrypt 密码哈希加密
- **开发环境**：Linux (CentOS 7 / Ubuntu 20.04)

---

## 🏗️ 系统架构

```plaintext
    ┌─────────────────┐     ┌─────────────────────────────────────┐
    │                 │     │              负载均衡层              │
    │    客户端集群    │────▶│         Nginx TCP Reverse Proxy     │
    │                 │     │                                     │
    └─────────────────┘     └───────────────────┬─────────────────┘
                                                │
                            ┌───────────────────┴─────────────────┐
                            │                                     │
                ┌───────────▼──────────┐             ┌────────────▼───────────┐
                │                      │             │                        │
                │  ChatServer Node 1   │◀───────────▶│   ChatServer Node 2    │
                │  (Port: 6000)        │  Redis Pub/Sub  (Port: 6001)         │
                │                      │             │                        │
                └───────────┬──────────┘             └────────────┬───────────┘
                            │                                     │
                            └───────────────────┬─────────────────┘
                                                │
                            ┌───────────────────▼─────────────────┐
                            │                                     │
                            │           数据持久化层              │
                            │  MySQL + 数据库连接池 + Redis缓存   │
                            │                                     │
                            └─────────────────────────────────────┘
```

核心组件职责
Nginx：TCP 四层负载均衡，将客户端连接均匀分发到多个 ChatServer 节点

ChatServer：业务处理节点，无状态设计，可水平扩展

Redis：

发布订阅模式实现跨节点消息通信

作为消息中间件，路由私聊及群聊消息到目标用户所在节点

MySQL：持久化存储用户、好友、群组、离线消息等数据

数据库连接池：避免频繁创建销毁数据库连接，提升高并发下的性能

## 📊 数据库设计

### User 表（用户信息）
| 字段名称 | 类型 | 说明 | 约束 |
|---------|------|------|------|
| id | INT | 用户唯一标识 | PRIMARY KEY, AUTO_INCREMENT |
| name | VARCHAR(50) | 用户名 | NOT NULL, UNIQUE |
| password | VARCHAR(255) | bcrypt 哈希密码 | NOT NULL |
| state | VARCHAR(20) | 登录状态 | NOT NULL, DEFAULT 'offline' |

### chat_group 表（群组信息）
| 字段名称 | 类型 | 说明 | 约束 |
|---------|------|------|------|
| id | INT | 群组唯一标识 | PRIMARY KEY, AUTO_INCREMENT |
| groupname | VARCHAR(50) | 群组名称 | NOT NULL, UNIQUE |
| groupdesc | VARCHAR(500) | 群组描述 | DEFAULT '' |

### friend 表（好友关系）
| 字段名称 | 类型 | 说明 | 约束 |
|---------|------|------|------|
| userid | INT | 用户ID | PRIMARY KEY, FOREIGN KEY REFERENCES user(id) |
| friendid | INT | 好友ID | PRIMARY KEY, FOREIGN KEY REFERENCES user(id) |

**索引**：`idx_friendid (friendid)`

### group_member 表（群组成员）
| 字段名称 | 类型 | 说明 | 约束 |
|---------|------|------|------|
| groupid | INT | 群组ID | PRIMARY KEY, FOREIGN KEY REFERENCES chat_group(id) |
| userid | INT | 用户ID | PRIMARY KEY, FOREIGN KEY REFERENCES user(id) |
| grouprole | TINYINT | 群内角色 | NOT NULL, DEFAULT 0 (0=普通成员, 1=创建者) |

**索引**：`idx_userid (userid)`

### offline_message 表（离线消息）
| 字段名称 | 类型 | 说明 | 约束 |
|---------|------|------|------|
| id | BIGINT | 消息唯一标识 | PRIMARY KEY, AUTO_INCREMENT |
| to_userid | INT | 接收者用户ID | NOT NULL, FOREIGN KEY REFERENCES user(id) |
| from_userid | INT | 发送者用户ID | NOT NULL, FOREIGN KEY REFERENCES user(id) |
| groupid | INT | 群组ID（私聊为NULL） | DEFAULT NULL, FOREIGN KEY REFERENCES chat_group(id) |
| content | TEXT | 消息内容 | NOT NULL |
| send_time | DATETIME | 发送时间 | NOT NULL |
| is_read | TINYINT | 是否已读 | NOT NULL, DEFAULT 0 |

**索引**：`idx_to_userid_time (to_userid, send_time)`

---

## 📡 通信协议
所有消息采用 JSON 格式，以换行符 `\n` 作为帧分隔符解决 TCP 粘包问题。

### 基础消息类型（public.hpp）
```cpp
enum EnMsgType {
    LOGIN_MSG = 1,        // 登录请求
    LOGIN_MSG_ACK = 2,    // 登录响应
    REG_MSG = 3,          // 注册请求
    REG_MSG_ACK = 4,      // 注册响应
    ONE_CHAT_MSG = 5,     // 一对一聊天消息
    ADD_FRIEND_MSG = 6,   // 添加好友请求
    CREATE_GROUP_MSG = 7, // 创建群组请求
    CREATE_GROUP_ACK = 8, // 创建群组响应
    JOIN_GROUP_MSG = 9,   // 加入群组请求
    JOIN_GROUP_ACK = 10,  // 加入群组响应
    LEAVE_GROUP_MSG = 11, // 退出群组请求
    LEAVE_GROUP_ACK = 12, // 退出群组响应
    GROUP_CHAT_MSG = 13,  // 群组聊天消息
    GROUP_INFO_MSG = 14,  // 获取群组信息请求
    GROUP_INFO_ACK = 15,  // 获取群组信息响应
    KICK_MSG_ACK = 100    // 被踢下线通知（服务端主动推送）
};
```
###核心消息示例
登录请求
```
{"msgid":1,"id":13,"password":"123456"}
```
登录成功响应

```
{"msgid":2,"errno":0,"id":13,"name":"zhangsan","friends":[],"groups":[]}
```
一对一聊天消息

```
{"msgid":5,"id":13,"to":15,"msg":"你好，在吗？","time":"2026-05-12 14:30:00"}
```
群组聊天消息

```
{"msgid":13,"id":13,"groupid":1,"msg":"大家好","time":"2026-05-13 10:00:00"}
```
## ✅ 已实现功能

### 用户系统
- 用户注册（密码 bcrypt 加密存储）
- 用户登录（支持顶号登录，旧连接自动断开）
- 退出登录
- 服务器启动时自动重置所有用户在线状态

### 好友系统
- 添加好友（双向关系）
- 好友列表展示
- 一对一实时聊天
- 离线消息存储与推送

### 群组系统
- 创建群组
- 加入群组
- 退出群组（群主仅当群内无其他成员时可退出并销毁群组）
- 群组聊天
- 群组成员列表展示

### 集群特性
- 多节点水平扩展
- Nginx 负载均衡（最少连接算法）
- Redis 跨节点消息路由（发布/订阅模式）
- 数据库连接池（高并发优化）

### 安全与优化
- 密码 bcrypt 哈希加密
- 输入验证（用户名/密码长度限制、非空检查）
- 详细的错误码与错误信息
- 在线状态内存管理（不再依赖数据库轮询）

🚀 编译与运行
环境依赖
# Ubuntu/Debian
```
sudo apt-get install g++ cmake make libmysqlclient-dev redis-server nginx
```

# CentOS/RHEL
```
sudo yum install gcc-c++ cmake make mysql-devel redis nginx
```
编译项目

# 克隆项目
```
git clone https://github.com/你的用户名/ClusterChat.git
cd ClusterChat
```

# 创建构建目录
```mkdir build && cd build```

# 编译
```
cmake ..
make -j4
```
编译完成后，可执行文件将生成在 bin/ 目录下。

## 🚀 部署配置

### 1. MySQL 配置

- 创建数据库：`CREATE DATABASE chat;`
- 导入数据库脚本：`mysql -u root -p chat < sql/chat.sql`
- 修改源码 `main.cpp` 或配置文件中的数据库连接信息

### 2. Redis 配置

- 启动 Redis 服务：`sudo systemctl start redis`
- 确保 Redis 监听在 `127.0.0.1:6379`（默认配置即可）

### 3. Nginx 配置

添加以下配置到 `/usr/local/nginx/conf/nginx.conf` 的 `http` 块外部：

```nginx
stream {
    upstream chatserver {
        least_conn;
        server 127.0.0.1:6000;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
    }

    server {
        listen 8888;
        proxy_pass chatserver;
        proxy_connect_timeout 5s;
        proxy_timeout 600s;
        tcp_nodelay on;
    }
}
```
重启 Nginx：sudo systemctl restart nginx

# 启动多个 ChatServer 节点
```
cd bin
./ChatServer 6000 &
./ChatServer 6001 &
./ChatServer 6002 &
```
# 启动客户端
```
./ChatClient 127.0.0.1 8888
```
## 🧪 测试情况

| 功能             | 测试结果 | 备注                                 |
| ---------------- | -------- | ------------------------------------ |
| 用户注册         | ✅ 通过  | 支持输入验证和重复用户名检查         |
| 用户登录         | ✅ 通过  | 支持顶号登录，旧连接自动断开         |
| 一对一聊天       | ✅ 通过  | 支持在线消息和离线消息               |
| 群组聊天         | ✅ 通过  | 支持跨节点群聊                       |
| 添加好友         | ✅ 通过  |                                      |
| 创建群组         | ✅ 通过  |                                      |
| 加入群组         | ✅ 通过  |                                      |
| 退出群组         | ✅ 通过  | 群主退出逻辑正常                     |
| 客户端异常退出   | ✅ 通过  | 服务器自动更新在线状态               |
| 多节点集群       | ✅ 通过  | 跨节点消息通信正常                   |
| 负载均衡         | ✅ 通过  | Nginx 均匀分发连接                   |

## 🐛 已知问题

- 客户端注销重新登录时，控制台可能残留 `Server closed connection` 调试输出（不影响功能）
- 客户端多线程输出存在乱序问题（`cout` 未加锁）
- 客户端输入密码时明文显示
- 好友/群组列表不会实时更新，需重新登录才能显示
- 客户端没有注销账号功能（仅支持断开连接）

## 📅 后续计划

- 修复控制台输出乱序问题，优化客户端交互体验
- 实现好友上下线实时通知
- 支持多设备同时在线（同一账号可多处登录）
- 添加消息已读回执功能
- 支持图片、文件等多媒体消息
- 实现心跳检测机制，主动检测后端节点健康状态
- 添加用户头像和个人资料功能
- 完善日志系统，支持分级和持久化

## 📄 许可证

本项目采用 MIT 许可证，详情请参见 [LICENSE](LICENSE) 文件。
