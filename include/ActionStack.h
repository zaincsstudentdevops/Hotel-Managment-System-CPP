
#ifndef ACTIONSTACK_H
#define ACTIONSTACK_H

#include <string>
using namespace std;

class ActionStack {
private:
    struct Node {
        string action;
        Node* next;

        Node(string value) {
            action = value;
            next = nullptr;
        }
    };

    Node* top;

public:
    ActionStack();
    ~ActionStack();

    ActionStack(const ActionStack&) = delete;
    ActionStack& operator=(const ActionStack&) = delete;

    void push(string action);
    string pop();
    bool peek(string& action) const;
    bool isEmpty();
    void display();
};

#endif
