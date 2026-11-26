
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

bool WaitingQueue::peek(string& customerName) const {
    if (front == nullptr) {
        return false;
    }
    customerName = front->customerName;
    return true;
}

bool WaitingQueue::isEmpty() {
    return front == nullptr;
}

std::size_t WaitingQueue::size() const {
    std::size_t count = 0;
    Node* current = front;
    while (current != nullptr) {
        ++count;
        current = current->next;
    }
    return count;
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
