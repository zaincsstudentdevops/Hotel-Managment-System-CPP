#include "Customer.h"
#include <iostream>

Customer::Customer() : customerId(0), fullName(""), phoneNumber(""), email(""), identityDocument("") {}

Customer::Customer(int id, string name, string phone, string mail, string cnic)
    : customerId(id), fullName(name), phoneNumber(phone), email(mail), identityDocument(cnic) {}

int Customer::getCustomerId() const { return customerId; }
string Customer::getFullName() const { return fullName; }
string Customer::getPhoneNumber() const { return phoneNumber; }
string Customer::getEmail() const { return email; }
string Customer::getIdentityDocument() const { return identityDocument; }

void Customer::setFullName(string name) { fullName = name; }
void Customer::setPhoneNumber(string phone) { phoneNumber = phone; }
void Customer::setEmail(string mail) { email = mail; }
void Customer::setIdentityDocument(string document) {
    identityDocument = document;
}

void Customer::displayCustomer() const {
    cout << "Customer ID: " << customerId
         << " | Name: " << fullName
         << " | Phone: " << phoneNumber
         << " | CNIC: " << identityDocument << endl;
}
