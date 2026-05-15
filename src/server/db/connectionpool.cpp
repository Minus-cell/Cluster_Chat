#include "connectionpool.h"
#include <iostream>
#include <chrono>

ConnectionPool* ConnectionPool::instance() {
    static ConnectionPool pool;
    return &pool;
}
ConnectionPool::ConnectionPool()
    : _initSize(0), _maxSize(0), _currentCount(0), _port(0) {
}
bool ConnectionPool::init(const std::string& host, int port,
                          const std::string& user, const std::string& password,
                          const std::string& dbname,
                          int initSize, int maxSize) {
    _host = host;
    _port = port;
    _user = user;
    _password = password;
    _dbname = dbname;
    _initSize = initSize;
    _maxSize = maxSize;
    _currentCount = 0;

    for (int i = 0; i < _initSize; ++i) {
        MYSQL* conn = createConnection();
        if (conn) {
            _connQueue.push(conn);
            _currentCount++;
        }
    }
    return !_connQueue.empty();
}

MYSQL* ConnectionPool::createConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) return nullptr;

    // 设置自动重连等选项（可选）
    // mysql_options(conn, MYSQL_OPT_RECONNECT, &...);

    if (!mysql_real_connect(conn, _host.c_str(), _user.c_str(),
                            _password.c_str(), _dbname.c_str(), _port, nullptr, 0)) {
        std::cerr << "mysql_real_connect error: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return nullptr;
    }
    return conn;
}

void ConnectionPool::closeConnection(MYSQL* conn) {
    if (conn) {
        mysql_close(conn);
    }
}

MYSQL* ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(_mtx);
    // 如果没有空闲连接且未达最大连接数，创建新连接
    if (_connQueue.empty() && _currentCount < _maxSize) {
        MYSQL* conn = createConnection();
        if (conn) {
            _currentCount++;
            return conn;
        }
    }
    // 如果有空闲连接，直接返回
    if (!_connQueue.empty()) {
        MYSQL* conn = _connQueue.front();
        _connQueue.pop();
        return conn;
    }
    // 如果连接池已满，等待别人归还
    _cv.wait(lock, [this]{ return !_connQueue.empty(); });
    MYSQL* conn = _connQueue.front();
    _connQueue.pop();
    return conn;
}

void ConnectionPool::releaseConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(_mtx);
    _connQueue.push(conn);
    _cv.notify_one();
}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(_mtx);
    while (!_connQueue.empty()) {
        MYSQL* conn = _connQueue.front();
        _connQueue.pop();
        closeConnection(conn);
    }
}