
#ifndef CUSTOMERLINKEDLIST_H
#define CUSTOMERLINKEDLIST_H

#include "Customer.h"
#include <string>

using namespace std;

class CustomerLinkedList {
private:
    struct Node {
        Customer data;
        Node* next;

        Node(Customer customer) {
            data = customer;
            next = nullptr;
        }
    };

    Node* head;

public:
    CustomerLinkedList();
    ~CustomerLinkedList();

    void insertCustomer(Customer customer);
    Customer* searchById(int id);
    Customer* searchByName(string name);
    bool deleteCustomer(int id);
    void displayAllCustomers();
};

#endif
