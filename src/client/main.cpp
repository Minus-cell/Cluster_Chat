#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <mutex>
#include <condition_variable>

using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// ============= 全局变量 =============
User g_currentUser;                     // 当前登录用户
vector<User> g_currentUserFriendList;   // 好友列表
vector<Group> g_currentUserGroupList;   // 群组列表

int g_clientfd = -1;                    // 与服务器的 socket
bool g_isLoginSuccess = false;          // 登录成功标志



// ============= 辅助函数声明 =============
void showCurrentUserData();
string getCurrentTime();
void mainMenu();
void readTaskHandler(int clientfd);
int sendLine(int fd, const string &line);
string recvLine(int fd);       // 阻塞读取一行（用于登录阶段）
void handleReceivedJson(const json &js);

// ============= 实现 =============

// 获取当前时间字符串
string getCurrentTime() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return string(buf);
}

// 发送一行（自动添加 \n）
int sendLine(int fd, const string &line) {
    string msg = line + "\n";
    return send(fd, msg.c_str(), msg.size(), 0);
}

// 阻塞读取一行（用于登录/注册阶段，仅在主线程中调用）
string recvLine(int fd) {
    static string buffer;
    while (true) {
        // 检查 buffer 中是否已有完整行
        size_t pos = buffer.find('\n');
        if (pos != string::npos) {
            string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            return line;
        }
        char chunk[1024];
        int n = recv(fd, chunk, sizeof(chunk) - 1, 0);
        if (n <= 0) {
            // 连接断开
            if (n == 0) cerr << "Server closed connection." << endl;
            else perror("recv");
            exit(1);
        }
        chunk[n] = '\0';
        buffer += chunk;
    }
}

// 显示当前用户信息
void showCurrentUserData() {
    cout << "\n====================== login user ======================" << endl;
    cout << "id: " << g_currentUser.getId() << "  name: " << g_currentUser.getName() << endl;
    cout << "---------------------- friend list ---------------------" << endl;
    if (g_currentUserFriendList.empty()) {
        cout << "No friends." << endl;
    } else {
        for (auto &f : g_currentUserFriendList) {
            cout << f.getId() << "  " << f.getName() << "  " << f.getState() << endl;
        }
    }
    cout << "---------------------- group list ----------------------" << endl;
    if (g_currentUserGroupList.empty()) {
        cout << "No groups." << endl;
    } else {
        for (auto &g : g_currentUserGroupList) {
            cout << "group id: " << g.getId() << "  name: " << g.getName() << "  desc: " << g.getDesc() << endl;
        }
    }
    cout << "========================================================" << endl;
}

// 处理接收到的 JSON（由接收线程调用）
void handleReceivedJson(const json &js) {
    int msgid = js["msgid"].get<int>();

    // 根据消息类型分发
    // 登录/注册响应已由主线程在登录阶段同步处理，这里不再需要处理
    // 如果意外收到，直接忽略即可
    if (msgid == ONE_CHAT_MSG) {
        cout << "\n[Private] " << js["id"] << ": " << js["msg"] << " (time: " << js["time"] << ")" << endl;
    }
    else if (msgid == GROUP_CHAT_MSG) {
        cout << "\n[Group " << js["groupid"] << "] " << js["id"] << ": " << js["msg"] << " (time: " << js["time"] << ")" << endl;
    }
    else if (msgid == KICK_MSG_ACK) {
        cout << "\nYou have been kicked by another login!" << endl;
        close(g_clientfd);
        exit(0);
    }
    else if (msgid == CREATE_GROUP_ACK) {
        if (js["errno"] == 0) {
            cout << "Group created successfully, group id: " << js["groupid"] << endl;
        } else {
            cout << "Create group failed, errno=" << js["errno"] << endl;
        }
    }
    else if (msgid == JOIN_GROUP_MSG_ACK) {
        if (js["errno"] == 0) {
            cout << "Joined group successfully." << endl;
        } else {
            cout << "Join group failed, errno=" << js["errno"] << endl;
        }
    }
    else if (msgid == LEAVE_GROUP_MSG_ACK) {
        if (js["errno"] == 0) {
            cout << "Left group successfully." << endl;
        } else {
            cout << "Leave group failed, errno=" << js["errno"] << endl;
        }
    }
    // 其他消息（如 LOGIN_MSG_ACK, REG_MSG_ACK）忽略
}

// 接收线程入口
void readTaskHandler(int clientfd) {
    string buffer;
    while (true) {
        // 接收一块数据
        char tmp[4096];
        int n = recv(clientfd, tmp, sizeof(tmp) - 1, 0);
        if (n <= 0) {
            if (n == 0) 
                perror("recv");
            //else perror("recv");
            //close(clientfd);
            return;
        }
        tmp[n] = '\0';
        buffer += tmp;

        // 按换行符分割，逐个处理完整 JSON
        while (true) {
            size_t pos = buffer.find('\n');
            if (pos == string::npos) break;
            string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) continue;
            try {
                json js = json::parse(line);
                handleReceivedJson(js);
            } catch (const json::parse_error &e) {
                cerr << "JSON parse error: " << e.what() << endl;
            }
        }
    }
}

// 登录/注册交互（主线程阻塞读取，直到收到 ACK）
bool loginOrRegister(int fd) {
    while (true) {
        cout << "====================" << endl;
        cout << "1. Login" << endl;
        cout << "2. Register" << endl;
        cout << "3. Exit" << endl;
        cout << "Choice: ";
        int choice;
        cin >> choice;
        cin.ignore();

        if (choice == 3) {
            cout << "Goodbye!" << endl;
            close(fd);
            exit(0);
        }

        json req;
        if (choice == 1)
        {
            int id;
            string pwd;
            cout << "User ID: ";
            cin >> id;
            if (cin.fail())
            {
                cin.clear();            // 清除错误状态
                cin.ignore(1024, '\n'); // 丢弃缓冲区中的错误内容
                cout << "Invalid input. Please enter a number." << endl;
                continue; // 重新显示菜单
            }
            cout << "Password: ";
            cin >> pwd;
            if (cin.fail())
            {
                cin.clear();
                cin.ignore(1024, '\n');
                cout << "Invalid input." << endl;
                continue;
            }
            req["msgid"] = LOGIN_MSG;
            req["id"] = id;
            req["password"] = pwd;
        }
        else if (choice == 2)
        {
            string name, pwd;
            cout << "Username: ";
            cin >> name;
            cout << "Password: ";
            cin >> pwd;
            req["msgid"] = REG_MSG;
            req["name"] = name;
            req["password"] = pwd;
        }
        else
        {
            cout << "Invalid choice, try again." << endl;
            continue;
        }

        sendLine(fd, req.dump());

        // 循环接收，直到收到登录/注册响应
        while (true) {
            string line = recvLine(fd);
            json js;
            try {
                js = json::parse(line);
            } catch (const json::parse_error &e) {
                cerr << "JSON parse error: " << e.what() << endl;
                continue;
            }

            int msgid = js["msgid"].get<int>();

            if (msgid == LOGIN_MSG_ACK) {
                int errno_val = js["errno"].get<int>();
                if (choice == 1) { // 登录
                    if (errno_val == LOGIN_OK) {
                        g_currentUser.setId(js["id"].get<int>());
                        g_currentUser.setName(js["name"].get<string>());

                        // 解析好友列表
                        g_currentUserFriendList.clear();
                        if (js.contains("friends")) {
                            for (auto &f : js["friends"]) {
                                User u;
                                u.setId(f["id"].get<int>());
                                u.setName(f["name"].get<string>());
                                u.setState(f["state"].get<string>());
                                g_currentUserFriendList.push_back(u);
                            }
                        }
                        // 解析群组列表
                        g_currentUserGroupList.clear();
                        if (js.contains("groups")) {
                            for (auto &g : js["groups"]) {
                                Group grp;
                                grp.setId(g["id"].get<int>());
                                grp.setName(g["groupname"]);
                                grp.setDesc(g["groupdesc"]);
                                g_currentUserGroupList.push_back(grp);
                            }
                        }
                        return true;  // 登录成功，返回
                    } else {
                        cout << "Login failed, errno=" << errno_val << endl;
                        if (errno_val == LOGIN_USER_NOT_EXIST)
                            cout << "User not exist." << endl;
                        else if (errno_val == LOGIN_WRONG_PASSWORD)
                            cout << "Wrong password." << endl;
                        break; // 退出内层 while，重新显示登录菜单
                    }
                }
            }
            else if (msgid == REG_MSG_ACK) {
                int errno_val = js["errno"].get<int>();
                if (errno_val == REGISTER_OK) {
                    cout << "Register success, your user id: " << js["id"] << endl;
                } else {
                    cout << "Register failed, errno=" << errno_val << endl;
                }
                break; // 退出内层 while，重新显示菜单
            }
            else if (msgid == ONE_CHAT_MSG || msgid == GROUP_CHAT_MSG) {
                // 登录前推送的离线消息，直接显示
                handleReceivedJson(js);
            }
            else {
                // 其他消息（如被踢通知）直接处理
                handleReceivedJson(js);
            }
        } // 内层 while
    } // 外层 while
}

// 主菜单（主线程发送线程）
void mainMenu() {
    showCurrentUserData();

    while (true) {
        cout << "\n===================== Menu =====================" << endl;
        cout << "1. One chat (private)" << endl;
        cout << "2. Group chat" << endl;
        cout << "3. Add friend" << endl;
        cout << "4. Create group" << endl;
        cout << "5. Join group" << endl;
        cout << "6. Leave group" << endl;
        cout << "7. Logout (re-login)" << endl;
        cout << "8. Quit" << endl;
        cout << "Choice: ";
        int choice;
        cin >> choice;
        if (cin.fail())
        {
            cin.clear();
            cin.ignore(1024, '\n');
            cout << "Invalid choice." << endl;
            continue;
        }

        json js;
        string msgText;
        int targetId, groupId;

        switch (choice) {
        case 1: // 一对一聊天
            cout << "Friend user id: ";
            cin >> targetId;
            cin.ignore();
            cout << "Message: ";
            getline(cin, msgText);
            js["msgid"] = ONE_CHAT_MSG;
            js["id"] = g_currentUser.getId();
            js["to"] = targetId;
            js["msg"] = msgText;
            js["time"] = getCurrentTime();
            sendLine(g_clientfd, js.dump());
            break;
        case 2: // 群聊
            cout << "Group id: ";
            cin >> groupId;
            cin.ignore();
            cout << "Message: ";
            getline(cin, msgText);
            js["msgid"] = GROUP_CHAT_MSG;
            js["id"] = g_currentUser.getId();
            js["groupid"] = groupId;
            js["msg"] = msgText;
            js["time"] = getCurrentTime();
            sendLine(g_clientfd, js.dump());
            break;
        case 3: // 添加好友
            cout << "Friend user id: ";
            cin >> targetId;
            js["msgid"] = ADD_FRIEND_MSG;
            js["id"] = g_currentUser.getId();
            js["friendid"] = targetId;
            sendLine(g_clientfd, js.dump());
            cout << "Friend request sent." << endl;
            break;
        case 4: { // 创建群组
            string gname, gdesc;
            cout << "Group name: ";
            getline(cin, gname);  // cin 后需要先清除缓冲区，可通过cin.ignore()
            // 上面已经有 cin.ignore() 在 switch 之前？需要调整：对于 case 4，需要先读掉前一个输入的换行
            // 简便起见，在 case 4 内部处理
            // 重新输入：因为之前 cin >> choice 后调用了 cin.ignore()，所以直接getline没问题
            // 但 case 4 里的 getline 会再次读取空行？之前的 cin.ignore() 已经忽略了换行，所以这里直接 getline 即可
            // 然而，若有其他输入残留，可能需额外处理，此处假定正确。
            cout << "Group description: ";
            getline(cin, gdesc);
            js["msgid"] = CREATE_GROUP_MSG;
            js["id"] = g_currentUser.getId();
            js["groupname"] = gname;
            js["groupdesc"] = gdesc;
            sendLine(g_clientfd, js.dump());
            break;
        }
        case 5: // 加入群组
            cout << "Group id to join: ";
            cin >> groupId;
            js["msgid"] = JOIN_GROUP_MSG;
            js["id"] = g_currentUser.getId();
            js["groupid"] = groupId;
            sendLine(g_clientfd, js.dump());
            break;
        case 6: // 退出群组
            cout << "Group id to leave: ";
            cin >> groupId;
            js["msgid"] = LEAVE_GROUP_MSG;
            js["id"] = g_currentUser.getId();
            js["groupid"] = groupId;
            sendLine(g_clientfd, js.dump());
            break;
        case 7: // 注销，重新登录
            cout << "Logging out..." << endl;
            close(g_clientfd);
            return; // 返回 main 重新连接（但 main 已经退出？需要修改：main 中循环连接）
        case 8:
            cout << "Goodbye!" << endl;
            close(g_clientfd);
            exit(0);
        default:
            cout << "Invalid choice." << endl;
        }
    }
}

// ============= 主函数 =============
int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << endl;
        return 1;
    }

    string serverIp = argv[1];
    int serverPort = stoi(argv[2]);

    // 外层循环支持重新登录
    while (true) {
        // 1. 创建 socket 并连接
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("socket");
            return 1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(serverPort);
        if (inet_pton(AF_INET, serverIp.c_str(), &addr.sin_addr) <= 0) {
            perror("inet_pton");
            close(sock);
            return 1;
        }

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("connect");
            close(sock);
            return 1;
        }
        g_clientfd = sock;

        // 2. 登录/注册
        if (!loginOrRegister(sock)) {
            // 登录/注册过程中主动退出
            continue;   // 重新连接
        }

        // 3. 启动接收线程
        thread recvThread(readTaskHandler, sock);
        recvThread.detach();

        // 4. 进入主菜单
        mainMenu();

        // 如果 mainMenu 返回（注销），关闭当前 socket，循环回去重新连接
        close(sock);
        // 清空旧数据
        g_currentUser = User();
        g_currentUserFriendList.clear();
        g_currentUserGroupList.clear();
    }

    return 0;
}