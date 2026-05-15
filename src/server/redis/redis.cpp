#include "redis.hpp"
#include <iostream>
#include <chrono>

using namespace std;

Redis::Redis()
    : _publish_context(nullptr), _subscribe_context(nullptr), _stop(false)
{
}

Redis::~Redis()
{
    // 设置停止标志
    _stop = true;

    // 释放订阅上下文会中断阻塞的 redisGetReply
    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
        _subscribe_context = nullptr;
    }

    // 等待监听线程结束
    if (_sub_thread.joinable())
    {
        _sub_thread.detach();
    }

    // 释放发布上下文
    if (_publish_context != nullptr)
    {
        lock_guard<mutex> lock(_publish_mutex);
        redisFree(_publish_context);
        _publish_context = nullptr;
    }
}

bool Redis::connect()
{
    // 防止重复连接
    if (_publish_context != nullptr)
    {
        return true;
    }

    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _publish_context)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subscribe_context)
    {
        cerr << "connect redis failed!" << endl;
        redisFree(_publish_context);
        _publish_context = nullptr;
        return false;
    }

    // 启动监听线程（不再detach，由析构join管理）
    _stop = false;
    _sub_thread = std::thread([this]() {
        observer_channel_message();
    });

    cout << "connect redis-server success!" << endl;
    return true;
}

// 向redis指定的通道channel发布消息（线程安全）
bool Redis::publish(int channel, string message)
{
    lock_guard<mutex> lock(_publish_mutex);
    if (_publish_context == nullptr)
    {
        cerr << "publish context is null!" << endl;
        return false;
    }

    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    if (_subscribe_context == nullptr)
    {
        return false;
    }

    // 记录频道以便重连后重新订阅
    {
        lock_guard<mutex> lock(_channels_mutex);
        _channels.push_back(channel);
    }

    // 使用异步命令，不阻塞
    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            cerr << "subscribe buffer write failed!" << endl;
            return false;
        }
    }
    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if (_subscribe_context == nullptr)
    {
        return false;
    }

    // 从记录中移除（简单处理：遍历删除）
    {
        lock_guard<mutex> lock(_channels_mutex);
        auto it = find(_channels.begin(), _channels.end(), channel);
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
    }

    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            cerr << "unsubscribe buffer write failed!" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息（带重连和停止检查）
void Redis::observer_channel_message()
{
    while (!_stop)
    {
        redisReply *reply = nullptr;
        int ret = redisGetReply(_subscribe_context, (void **)&reply);

        if (ret == REDIS_OK && reply != nullptr)
        {
            // 订阅消息是一个三元素数组：message, channel, content

            // 安全校验：必须是数组、至少3个元素、且str非空
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3)
            {
                if (reply->element[1] != nullptr && reply->element[2] != nullptr &&
                    reply->element[1]->str != nullptr && reply->element[2]->str != nullptr)
                {
                    int channel = atoi(reply->element[1]->str);
                    std::string msg = reply->element[2]->str;
                    if (_notify_message_handler)
                    {
                        _notify_message_handler(channel, msg);
                    }
                }
            }

            freeReplyObject(reply);
        }
        else
        {
            // 连接断开或出错
            if (reply)
            {
                freeReplyObject(reply);
            }

            if (_stop)
            {
                break;
            }

            cerr << "redis subscribe connection lost, trying to reconnect..." << endl;

            // 释放旧上下文
            if (_subscribe_context)
            {
                redisFree(_subscribe_context);
                _subscribe_context = nullptr;
            }

            // 重连循环
            while (!_stop)
            {
                _subscribe_context = redisConnect("127.0.0.1", 6379);
                if (_subscribe_context != nullptr)
                {
                    break;
                }
                cerr << "reconnect failed, retry in 1 second..." << endl;
                this_thread::sleep_for(chrono::seconds(1));
            }

            if (_stop)
            {
                break;
            }

            // 重新订阅之前的所有频道
            {
                lock_guard<mutex> lock(_channels_mutex);
                for (int ch : _channels)
                {
                    // 使用异步命令重新订阅
                    redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", ch);
                }
            }

            // 发送缓冲区
            int done = 0;
            while (!done)
            {
                if (redisBufferWrite(_subscribe_context, &done) == REDIS_ERR)
                {
                    cerr << "resubscribe buffer write failed" << endl;
                    break;
                }
            }
        }
    }

    cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << endl;
}

void Redis::init_notify_handler(function<void(int, string)> fn)
{
    _notify_message_handler = fn;
}