
#include "ActionStack.h"
#include <iostream>

using namespace std;

ActionStack::ActionStack() {
    top = nullptr;
}

void ActionStack::push(string action) {
    Node* newNode = new Node(action);

    newNode->next = top;
    top = newNode;
}

string ActionStack::pop() {
    if (isEmpty()) {
        return "Stack is empty.";
    }

    Node* temp = top;
    string action = temp->action;

    top = top->next;
    delete temp;

    return action;
}

bool ActionStack::isEmpty() {
    return top == nullptr;
}

void ActionStack::display() {
    Node* current = top;

    if (isEmpty()) {
        cout << "Stack is empty.\n";
        return;
    }

    while (current != nullptr) {
        cout << current->action << endl;
        current = current->next;
    }
}

ActionStack::~ActionStack() {
    while (!isEmpty()) {
        pop();
    }
}
