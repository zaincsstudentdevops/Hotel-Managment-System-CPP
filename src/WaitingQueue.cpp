
#include "WaitingQueue.h"
#include <iostream>

using namespace std;

WaitingQueue::WaitingQueue() {
    front = nullptr;
    rear = nullptr;
}

void WaitingQueue::enqueue(string customerName) {
    Node* newNode = new Node(customerName);

    if (rear == nullptr) {
        front = newNode;
        rear = newNode;
        return;
    }

    rear->next = newNode;
    rear = newNode;
}

string WaitingQueue::dequeue() {
    if (isEmpty()) {
        return "Queue is empty.";
    }

    Node* temp = front;
    string name = temp->customerName;

    front = front->next;

    if (front == nullptr) {
        rear = nullptr;
    }

    delete temp;

    return name;
}

bool WaitingQueue::isEmpty() {
    return front == nullptr;
}

void WaitingQueue::display() {
    Node* current = front;

    if (isEmpty()) {
        cout << "Queue is empty.\n";
        return;
    }

    while (current != nullptr) {
        cout << current->customerName << endl;
        current = current->next;
    }
}

WaitingQueue::~WaitingQueue() {
    while (!isEmpty()) {
        dequeue();
    }
}
