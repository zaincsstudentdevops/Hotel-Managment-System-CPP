
#include "CustomerLinkedList.h"
#include <iostream>

using namespace std;

CustomerLinkedList::CustomerLinkedList() {
    head = nullptr;
}

bool CustomerLinkedList::insertCustomer(Customer customer) {
    if (searchById(customer.getCustomerId()) != nullptr) {
        return false;
    }

    Node* newNode = new Node(customer);

    if (head == nullptr) {
        head = newNode;
        return true;
    }

    Node* current = head;

    while (current->next != nullptr) {
        current = current->next;
    }

    current->next = newNode;
    return true;
}

Customer* CustomerLinkedList::searchById(int id) {
    Node* current = head;

    while (current != nullptr) {
        if (current->data.getCustomerId() == id) {
            return &(current->data);
        }

        current = current->next;
    }

    return nullptr;
}

Customer* CustomerLinkedList::searchByName(string name) {
    Node* current = head;

    while (current != nullptr) {
        if (current->data.getFullName() == name) {
            return &(current->data);
        }

        current = current->next;
    }

    return nullptr;
}

bool CustomerLinkedList::deleteCustomer(int id) {
    if (head == nullptr) {
        return false;
    }

    if (head->data.getCustomerId() == id) {
        Node* temp = head;
        head = head->next;
        delete temp;
        return true;
    }

    Node* current = head;

    while (current->next != nullptr) {
        if (current->next->data.getCustomerId() == id) {
            Node* temp = current->next;
            current->next = temp->next;
            delete temp;
            return true;
        }

        current = current->next;
    }

    return false;
}

void CustomerLinkedList::displayAllCustomers() {
    Node* current = head;

    if (current == nullptr) {
        cout << "No customers found.\n";
        return;
    }

    while (current != nullptr) {
        current->data.displayCustomer();
        current = current->next;
    }
}

CustomerLinkedList::~CustomerLinkedList() {
    Node* current = head;

    while (current != nullptr) {
        Node* temp = current;
        current = current->next;
        delete temp;
    }
}
