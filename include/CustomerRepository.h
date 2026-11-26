#ifndef CUSTOMERREPOSITORY_H
#define CUSTOMERREPOSITORY_H

#include "CustomerLinkedList.h"
#include "Database.h"

#include <string>

enum class CustomerLookupStatus {
    found,
    notFound,
    error
};

enum class CustomerUpdateStatus {
    updated,
    unchanged,
    notFound,
    error
};

enum class CustomerDeleteStatus {
    deleted,
    notFound,
    hasReservations,
    error
};

class CustomerRepository {
public:
    CustomerRepository(Database& database, CustomerLinkedList& customers);

    bool loadCustomers(std::string& error);
    bool registerCustomer(
        const Customer& customer,
        std::string& error,
        bool synchronizeMemory = true
    );
    CustomerLookupStatus findCustomerById(
        int customerId,
        Customer& customer,
        std::string& error
    );
    CustomerUpdateStatus updateCustomer(
        const Customer& customer,
        std::string& error,
        bool synchronizeMemory = true
    );
    CustomerDeleteStatus deleteCustomer(
        int customerId,
        std::string& error,
        bool synchronizeMemory = true
    );
    bool synchronizeCustomer(const Customer& customer, std::string& error);

private:
    Database& database;
    CustomerLinkedList& customers;

    bool getConnection(MYSQL*& connection, std::string& error) const;
    bool customerExists(
        MYSQL* connection,
        int customerId,
        bool& exists,
        std::string& error
    ) const;
    bool customerHasReservations(
        MYSQL* connection,
        int customerId,
        bool& hasReservations,
        std::string& error
    ) const;
};

#endif
