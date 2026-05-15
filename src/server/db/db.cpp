#include "db.h"
#include <string>
#include <muduo/base/Logging.h>


// 初始化数据库连接
MySQL::MySQL() : _conn(nullptr), _isFromPool(false) {
    _conn = ConnectionPool::instance()->getConnection();
    if (_conn) _isFromPool = true;
}


// 释放数据库连接资源
MySQL::~MySQL()
{
    close();
}
// 连接数据库
bool MySQL::connect() {
    // 如果已经持有连接，直接返回 true
    if (_conn) return true;
    _conn = ConnectionPool::instance()->getConnection();
    if (_conn) {
        _isFromPool = true;
        return true;
    }
    return false;
}
// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "更新失败!";
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

MYSQL *MySQL::getConnection()
{
    return _conn;
}


void MySQL::close() {
    if (_conn && _isFromPool) {
        ConnectionPool::instance()->releaseConnection(_conn);
        _conn = nullptr;
        _isFromPool = false;
    }
}