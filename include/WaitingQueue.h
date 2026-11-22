
#ifndef WAITINGQUEUE_H
#define WAITINGQUEUE_H

#include <string>
using namespace std;

class WaitingQueue {
private:
    struct Node {
        string customerName;
        Node* next;

        Node(string name) {
            customerName = name;
            next = nullptr;
        }
    };

    Node* front;
    Node* rear;

public:
    WaitingQueue();
    ~WaitingQueue();

    void enqueue(string customerName);
    string dequeue();
    bool isEmpty();
    void display();
};

#endif
