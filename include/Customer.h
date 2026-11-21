#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <string>
using namespace std;

class Customer {
private:
    int customerId;
    string fullName;
    string phoneNumber;
    string email;
    string identityDocument; // CNIC or Passport

public:
    Customer();
    Customer(int id, string name, string phone, string mail, string cnic);

    int getCustomerId() const;
    string getFullName() const;
    string getPhoneNumber() const;
    string getEmail() const;
    string getIdentityDocument() const;

    void setPhoneNumber(string phone);
    void setEmail(string mail);
    void displayCustomer() const;
};

#endif
