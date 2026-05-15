#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

class User {
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
        : id(id), name(name), password(pwd), state(state) {}

    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setPwd(string password) { this->password = password; }
    void setState(string state) { this->state = state; }

    int getId() const { return id; }
    string getName() const { return name; }
    string getPwd() const { return password; }
    string getState() const { return state; }

private:
    int id;
    string name;
    string password;
    string state;
};
#endif