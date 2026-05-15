#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

class ConnectionPool {
public:
    static ConnectionPool* instance();     // 单例

    // 初始化连接池（在服务器启动时调用一次）
    bool init(const std::string& host, int port,
              const std::string& user, const std::string& password,
              const std::string& dbname,
              int initSize = 4, int maxSize = 16);

    // 获取一个可用的连接
    MYSQL* getConnection();

    // 归还连接
    void releaseConnection(MYSQL* conn);

    // 析构时关闭所有连接
    ~ConnectionPool();

private:
    ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    MYSQL* createConnection();          // 创建新连接
    void closeConnection(MYSQL* conn);  // 关闭连接

    std::queue<MYSQL*> _connQueue;      // 空闲连接队列
    std::mutex _mtx;
    std::condition_variable _cv;

    int _initSize;
    int _maxSize;
    int _currentCount;                  // 当前连接总数（包含被借出的）

    std::string _host;
    int _port;
    std::string _user;
    std::string _password;
    std::string _dbname;
};

#endif