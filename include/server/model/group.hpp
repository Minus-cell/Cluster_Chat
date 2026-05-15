#ifndef GROUP_H
#define GROUP_H

#include <string>
using namespace std;

class Group
{
public:
    Group(int id = -1, string name = "", string desc = "")
        : id(id), name(name), desc(desc) {}

    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setDesc(string desc) { this->desc = desc; }

    int getId() const { return id; }
    string getName() const { return name; }
    string getDesc() const { return desc; }

private:
    int id;
    string name;
    string desc;
};

#endif