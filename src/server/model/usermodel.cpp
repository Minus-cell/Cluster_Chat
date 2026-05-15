#include "usermodel.hpp"
#include "db.h"
#include "bcrypt.h"                     // 1. 引入 bcrypt 头文件

bool UserModel::insert(User &user)
{
    // 2. 将用户明文密码生成 bcrypt 哈希
    std::string hashedPwd = bcrypt::generateHash(user.getPwd(), 12);

    MySQL mysql;
    if (!mysql.connect())
        return false;

    MYSQL *conn = mysql.getConnection();
    // 3. 转义用户输入的字段，防止 SQL 注入（哈希值只有安全字符，但最好也转义）
    char name_buf[128] = {0};
    char pwd_buf[256] = {0};
    char state_buf[32] = {0};

    mysql_real_escape_string(conn, name_buf, user.getName().c_str(), user.getName().size());
    mysql_real_escape_string(conn, pwd_buf, hashedPwd.c_str(), hashedPwd.size());
    mysql_real_escape_string(conn, state_buf, user.getState().c_str(), user.getState().size());

    // 4. 表名已修正为小写 'user'
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql),
             "INSERT INTO user(name, password, state) VALUES('%s', '%s', '%s')",
             name_buf, pwd_buf, state_buf);

    if (mysql.update(sql))
    {
        // 获取自增 ID
        user.setId(mysql_insert_id(conn));
        return true;
    }
    return false;
}

//根据用户号码查询信息
User UserModel::query(int id)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql),"select * from user where id = %d", id);

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1] ? row[1] : "");
                user.setPwd(row[2] ? row[2] : "");
                user.setState(row[3] ? row[3] : "offline");
                mysql_free_result(res);
                return user;
            }
            mysql_free_result(res);
        }
    }

    // 查询失败返回默认构造的User对象（id=-1）
    return User();
}
bool UserModel::updateState(User &user)
{
    char sql[1024] = {0};
    sprintf(sql, "update user set state = '%s' where id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }

    return false;
}

bool UserModel::resetAllStateToOffline() {
    const char* sql = "update user set state='offline' where state='online'";
    MySQL mysql;
    if (mysql.connect()) {
        return mysql.update(sql);
    }
    return false;
}