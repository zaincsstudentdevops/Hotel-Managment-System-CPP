#include "CustomerRepository.h"

#include "DatabaseFieldValidation.h"

#include <charconv>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <mysqld_error.h>

namespace {

struct StatementCloser {
    MYSQL* connection;
    const char* operation;

    void operator()(MYSQL_STMT* statement) const noexcept {
        if (statement != nullptr && mysql_stmt_close(statement) != 0) {
            std::cerr << operation << ": could not close statement: "
                      << mysql_error(connection) << '\n';
        }
    }
};

using StatementPtr = std::unique_ptr<MYSQL_STMT, StatementCloser>;

StatementPtr prepareStatement(
    MYSQL* connection,
    const char* sql,
    const char* operation,
    std::string& error
) {
    StatementPtr statement(
        mysql_stmt_init(connection),
        StatementCloser{connection, operation}
    );
    if (!statement) {
        error = std::string(operation) + ": could not initialize statement: " +
                mysql_error(connection);
        return {};
    }

    if (mysql_stmt_prepare(
            statement.get(),
            sql,
            static_cast<unsigned long>(std::char_traits<char>::length(sql))
        ) != 0) {
        error = std::string(operation) + ": could not prepare statement: " +
                mysql_stmt_error(statement.get());
        return {};
    }

    return statement;
}

bool bindAndExecute(
    MYSQL_STMT* statement,
    MYSQL_BIND* parameters,
    const char* operation,
    std::string& error
) {
    if (mysql_stmt_bind_param(statement, parameters) != 0) {
        error = std::string(operation) + ": could not bind parameters: " +
                mysql_stmt_error(statement);
        return false;
    }

    if (mysql_stmt_execute(statement) != 0) {
        error = std::string(operation) + ": execution failed: " +
                mysql_stmt_error(statement);
        return false;
    }

    return true;
}

void bindInteger(MYSQL_BIND& binding, int& value) {
    binding.buffer_type = MYSQL_TYPE_LONG;
    binding.buffer = &value;
}

bool bindText(
    MYSQL_BIND& binding,
    std::string& value,
    unsigned long& length,
    std::string& error
) {
    if (value.size() > std::numeric_limits<unsigned long>::max()) {
        error = "Customer field is too long for MariaDB Connector/C.";
        return false;
    }

    length = static_cast<unsigned long>(value.size());
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = value.data();
    binding.buffer_length = length;
    binding.length = &length;
    return true;
}

bool hasText(const std::string& value) {
    return value.find_first_not_of(" \t\r\n") != std::string::npos;
}

bool isValidCustomer(const Customer& customer, std::string& error) {
    if (customer.getCustomerId() <= 0) {
        error = "Customer ID must be greater than zero.";
        return false;
    }
    if (!hasText(customer.getFullName()) ||
        !hasText(customer.getPhoneNumber()) ||
        !hasText(customer.getEmail()) ||
        !hasText(customer.getIdentityDocument())) {
        error = "Customer name, phone, email, and identity document are required.";
        return false;
    }
    return true;
}

bool customerFieldsFitDatabase(
    MYSQL* connection,
    const Customer& customer,
    std::string& error
) {
    return database_field_validation::withinCharacterLimit(
               connection, customer.getFullName(), 150,
               "Customer full name", error
           ) &&
           database_field_validation::withinCharacterLimit(
               connection, customer.getPhoneNumber(), 30,
               "Customer phone number", error
           ) &&
           database_field_validation::withinCharacterLimit(
               connection, customer.getEmail(), 254,
               "Customer email", error
           ) &&
           database_field_validation::withinCharacterLimit(
               connection, customer.getIdentityDocument(), 50,
               "Customer identity document", error
           );
}

void bindResultInteger(MYSQL_BIND& binding, int& value, my_bool& isNull,
                       my_bool& hasError) {
    binding.buffer_type = MYSQL_TYPE_LONG;
    binding.buffer = &value;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

void bindCustomerResultText(
    MYSQL_BIND& binding,
    char* buffer,
    unsigned long bufferLength,
    unsigned long& valueLength,
    my_bool& isNull,
    my_bool& hasError
) {
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = buffer;
    binding.buffer_length = bufferLength;
    binding.length = &valueLength;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

bool sameCustomer(const Customer& left, const Customer& right) {
    return left.getCustomerId() == right.getCustomerId() &&
           left.getFullName() == right.getFullName() &&
           left.getPhoneNumber() == right.getPhoneNumber() &&
           left.getEmail() == right.getEmail() &&
           left.getIdentityDocument() == right.getIdentityDocument();
}

}  // namespace

CustomerRepository::CustomerRepository(
    Database& database,
    CustomerLinkedList& customers
) : database(database), customers(customers) {}

bool CustomerRepository::getConnection(
    MYSQL*& connection,
    std::string& error
) const {
    connection = database.getConnection();
    if (!database.isConnected() || connection == nullptr) {
        error = "MariaDB connection is not available.";
        return false;
    }
    return true;
}

bool CustomerRepository::loadCustomers(std::string& error) {
    error.clear();
    MYSQL* connection = nullptr;
    if (!getConnection(connection, error)) {
        return false;
    }

    const char* sql =
        "SELECT customer_id, full_name, phone_number, email, identity_document "
        "FROM customers ORDER BY customer_id";
    if (mysql_query(connection, sql) != 0) {
        error = std::string("Could not load customers: ") +
                mysql_error(connection);
        return false;
    }

    struct ResultCloser {
        void operator()(MYSQL_RES* result) const noexcept {
            if (result != nullptr) {
                mysql_free_result(result);
            }
        }
    };
    std::unique_ptr<MYSQL_RES, ResultCloser> result(
        mysql_store_result(connection)
    );
    if (!result) {
        if (mysql_field_count(connection) == 0) {
            error = "Customer query did not return a result set.";
        } else {
            error = std::string("Could not retrieve customers: ") +
                    mysql_error(connection);
        }
        return false;
    }

    if (mysql_num_fields(result.get()) != 5) {
        error = "Customer query returned an unexpected number of columns.";
        return false;
    }

    std::vector<Customer> loadedCustomers;
    std::set<int> loadedIds;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result.get());
        if (lengths == nullptr) {
            error = std::string("Could not read customer field lengths: ") +
                    mysql_error(connection);
            return false;
        }
        for (unsigned int column = 0; column < 5; ++column) {
            if (row[column] == nullptr) {
                error = "A customer row contains a NULL field.";
                return false;
            }
        }

        int customerId = 0;
        const char* idEnd = row[0] + lengths[0];
        const auto parsedId = std::from_chars(row[0], idEnd, customerId);
        if (parsedId.ec != std::errc{} || parsedId.ptr != idEnd ||
            customerId <= 0) {
            error = "A customer row contains an invalid customer ID.";
            return false;
        }
        if (!loadedIds.insert(customerId).second) {
            error = "Duplicate customer ID returned while loading customers.";
            return false;
        }

        loadedCustomers.emplace_back(
            customerId,
            std::string(row[1], lengths[1]),
            std::string(row[2], lengths[2]),
            std::string(row[3], lengths[3]),
            std::string(row[4], lengths[4])
        );
    }

    if (mysql_errno(connection) != 0) {
        error = std::string("Could not finish loading customers: ") +
                mysql_error(connection);
        return false;
    }

    for (const Customer& customer : loadedCustomers) {
        if (customers.searchById(customer.getCustomerId()) != nullptr) {
            error = "Customer list already contains ID " +
                    std::to_string(customer.getCustomerId()) +
                    "; refusing to load duplicates.";
            return false;
        }
    }

    for (const Customer& customer : loadedCustomers) {
        if (!customers.insertCustomer(customer)) {
            error = "Could not add customer ID " +
                    std::to_string(customer.getCustomerId()) +
                    " to the in-memory list.";
            return false;
        }
    }

    return true;
}

bool CustomerRepository::registerCustomer(
    const Customer& customer,
    std::string& error,
    bool synchronizeMemory
) {
    error.clear();
    if (!isValidCustomer(customer, error)) {
        return false;
    }

    if (customers.searchById(customer.getCustomerId()) != nullptr) {
        error = "Customer ID already exists.";
        return false;
    }

    MYSQL* connection = nullptr;
    if (!getConnection(connection, error)) {
        return false;
    }
    if (!customerFieldsFitDatabase(connection, customer, error)) {
        return false;
    }

    StatementPtr statement = prepareStatement(
        connection,
        "INSERT INTO customers "
        "(customer_id, full_name, phone_number, email, identity_document) "
        "VALUES (?, ?, ?, ?, ?)",
        "Customer insert",
        error
    );
    if (!statement) {
        return false;
    }

    int customerId = customer.getCustomerId();
    std::string fullName = customer.getFullName();
    std::string phoneNumber = customer.getPhoneNumber();
    std::string email = customer.getEmail();
    std::string identityDocument = customer.getIdentityDocument();
    unsigned long lengths[4]{};
    MYSQL_BIND parameters[5]{};
    bindInteger(parameters[0], customerId);
    if (!bindText(parameters[1], fullName, lengths[0], error) ||
        !bindText(parameters[2], phoneNumber, lengths[1], error) ||
        !bindText(parameters[3], email, lengths[2], error) ||
        !bindText(parameters[4], identityDocument, lengths[3], error)) {
        return false;
    }
    if (!bindAndExecute(statement.get(), parameters, "Customer insert", error)) {
        if (mysql_stmt_errno(statement.get()) == ER_DUP_ENTRY) {
            error = "Customer ID already exists in MariaDB.";
        }
        return false;
    }
    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        error = "MariaDB did not insert exactly one customer.";
        return false;
    }

    if (synchronizeMemory && !synchronizeCustomer(customer, error)) {
        error = "Customer was inserted in MariaDB, but the in-memory list "
                "could not be synchronized: " + error;
        return false;
    }
    return true;
}

CustomerLookupStatus CustomerRepository::findCustomerById(
    int customerId,
    Customer& customer,
    std::string& error
) {
    error.clear();
    if (customerId <= 0) {
        error = "Customer ID must be greater than zero.";
        return CustomerLookupStatus::error;
    }

    MYSQL* connection = nullptr;
    if (!getConnection(connection, error)) {
        return CustomerLookupStatus::error;
    }

    StatementPtr statement = prepareStatement(
        connection,
        "SELECT customer_id, full_name, phone_number, email, "
        "identity_document FROM customers WHERE customer_id = ?",
        "Customer search",
        error
    );
    if (!statement) {
        return CustomerLookupStatus::error;
    }

    int requestedId = customerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], requestedId);
    if (!bindAndExecute(statement.get(), parameters, "Customer search", error)) {
        return CustomerLookupStatus::error;
    }

    if (mysql_stmt_store_result(statement.get()) != 0) {
        error = std::string("Customer search could not store results: ") +
                mysql_stmt_error(statement.get());
        return CustomerLookupStatus::error;
    }

    int resultId = 0;
    char fullName[601]{};
    char phoneNumber[121]{};
    char email[1017]{};
    char identityDocument[201]{};
    unsigned long lengths[5]{};
    my_bool nulls[5]{};
    my_bool errors[5]{};
    MYSQL_BIND results[5]{};
    bindResultInteger(results[0], resultId, nulls[0], errors[0]);
    bindCustomerResultText(results[1], fullName, sizeof(fullName), lengths[1],
                           nulls[1], errors[1]);
    bindCustomerResultText(results[2], phoneNumber, sizeof(phoneNumber),
                           lengths[2], nulls[2], errors[2]);
    bindCustomerResultText(results[3], email, sizeof(email), lengths[3],
                           nulls[3], errors[3]);
    bindCustomerResultText(results[4], identityDocument,
                           sizeof(identityDocument), lengths[4],
                           nulls[4], errors[4]);

    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Customer search could not bind results: ") +
                mysql_stmt_error(statement.get());
        return CustomerLookupStatus::error;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        return CustomerLookupStatus::notFound;
    }
    if (fetchStatus != 0 && fetchStatus != MYSQL_DATA_TRUNCATED) {
        error = std::string("Customer search could not fetch the result: ") +
                mysql_stmt_error(statement.get());
        return CustomerLookupStatus::error;
    }

    for (unsigned int column = 0; column < 5; ++column) {
        if (nulls[column]) {
            error = "Customer search returned an unexpected NULL field.";
            return CustomerLookupStatus::error;
        }
        if (errors[column]) {
            error = "Customer data exceeded the result buffer size.";
            return CustomerLookupStatus::error;
        }
    }
    if (resultId != customerId || lengths[1] >= sizeof(fullName) ||
        lengths[2] >= sizeof(phoneNumber) || lengths[3] >= sizeof(email) ||
        lengths[4] >= sizeof(identityDocument)) {
        error = "Customer search returned invalid or truncated customer data.";
        return CustomerLookupStatus::error;
    }

    Customer foundCustomer(
        resultId,
        std::string(fullName, lengths[1]),
        std::string(phoneNumber, lengths[2]),
        std::string(email, lengths[3]),
        std::string(identityDocument, lengths[4])
    );
    if (!synchronizeCustomer(foundCustomer, error)) {
        return CustomerLookupStatus::error;
    }

    customer = foundCustomer;
    return CustomerLookupStatus::found;
}

CustomerUpdateStatus CustomerRepository::updateCustomer(
    const Customer& customer,
    std::string& error,
    bool synchronizeMemory
) {
    error.clear();
    if (!isValidCustomer(customer, error)) {
        return CustomerUpdateStatus::error;
    }

    MYSQL* connection = nullptr;
    if (!getConnection(connection, error)) {
        return CustomerUpdateStatus::error;
    }
    if (!customerFieldsFitDatabase(connection, customer, error)) {
        return CustomerUpdateStatus::error;
    }

    StatementPtr statement = prepareStatement(
        connection,
        "UPDATE customers SET full_name = ?, phone_number = ?, email = ?, "
        "identity_document = ? WHERE customer_id = ?",
        "Customer update",
        error
    );
    if (!statement) {
        return CustomerUpdateStatus::error;
    }

    std::string fullName = customer.getFullName();
    std::string phoneNumber = customer.getPhoneNumber();
    std::string email = customer.getEmail();
    std::string identityDocument = customer.getIdentityDocument();
    int customerId = customer.getCustomerId();
    unsigned long lengths[4]{};
    MYSQL_BIND parameters[5]{};
    if (!bindText(parameters[0], fullName, lengths[0], error) ||
        !bindText(parameters[1], phoneNumber, lengths[1], error) ||
        !bindText(parameters[2], email, lengths[2], error) ||
        !bindText(parameters[3], identityDocument, lengths[3], error)) {
        return CustomerUpdateStatus::error;
    }
    bindInteger(parameters[4], customerId);

    if (!bindAndExecute(statement.get(), parameters, "Customer update", error)) {
        return CustomerUpdateStatus::error;
    }

    const my_ulonglong affectedRows = mysql_stmt_affected_rows(statement.get());
    if (affectedRows == 1) {
        if (synchronizeMemory && !synchronizeCustomer(customer, error)) {
            return CustomerUpdateStatus::error;
        }
        return CustomerUpdateStatus::updated;
    }
    if (affectedRows != 0) {
        error = "Customer update changed an unexpected number of rows.";
        return CustomerUpdateStatus::error;
    }

    if (!synchronizeMemory) {
        bool exists = false;
        if (!customerExists(connection, customerId, exists, error)) {
            return CustomerUpdateStatus::error;
        }
        return exists ? CustomerUpdateStatus::unchanged
                      : CustomerUpdateStatus::notFound;
    }

    Customer storedCustomer;
    const CustomerLookupStatus lookupStatus =
        findCustomerById(customerId, storedCustomer, error);
    if (lookupStatus == CustomerLookupStatus::notFound) {
        return CustomerUpdateStatus::notFound;
    }
    if (lookupStatus == CustomerLookupStatus::error) {
        return CustomerUpdateStatus::error;
    }
    if (sameCustomer(storedCustomer, customer)) {
        return CustomerUpdateStatus::unchanged;
    }

    error = "MariaDB did not change the customer record.";
    return CustomerUpdateStatus::error;
}

bool CustomerRepository::customerHasReservations(
    MYSQL* connection,
    int customerId,
    bool& hasReservations,
    std::string& error
) const {
    StatementPtr statement = prepareStatement(
        connection,
        "SELECT reservation_id FROM reservations "
        "WHERE customer_id = ? LIMIT 1",
        "Customer reservation check",
        error
    );
    if (!statement) {
        return false;
    }

    int requestedId = customerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], requestedId);
    if (!bindAndExecute(
            statement.get(), parameters, "Customer reservation check", error
        )) {
        return false;
    }
    if (mysql_stmt_store_result(statement.get()) != 0) {
        error = std::string("Customer reservation check could not store results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    int reservationId = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND results[1]{};
    bindResultInteger(results[0], reservationId, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Customer reservation check could not bind results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        hasReservations = false;
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError) {
        error = std::string("Customer reservation check could not fetch a result: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    hasReservations = true;
    return true;
}

bool CustomerRepository::customerExists(
    MYSQL* connection,
    int customerId,
    bool& exists,
    std::string& error
) const {
    StatementPtr statement = prepareStatement(
        connection,
        "SELECT customer_id FROM customers WHERE customer_id = ? LIMIT 1",
        "Customer existence check",
        error
    );
    if (!statement) {
        return false;
    }

    int requestedId = customerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], requestedId);
    if (!bindAndExecute(
            statement.get(), parameters, "Customer existence check", error
        )) {
        return false;
    }

    int resultId = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND results[1]{};
    bindResultInteger(results[0], resultId, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Customer existence check could not bind results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }
    if (mysql_stmt_store_result(statement.get()) != 0) {
        error = std::string("Customer existence check could not store results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        exists = false;
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError || resultId != customerId) {
        error = std::string("Customer existence check could not fetch a valid result: ") +
                mysql_stmt_error(statement.get());
        return false;
    }
    exists = true;
    return true;
}

CustomerDeleteStatus CustomerRepository::deleteCustomer(
    int customerId,
    std::string& error,
    bool synchronizeMemory
) {
    error.clear();
    if (customerId <= 0) {
        error = "Customer ID must be greater than zero.";
        return CustomerDeleteStatus::error;
    }

    MYSQL* connection = nullptr;
    if (!getConnection(connection, error)) {
        return CustomerDeleteStatus::error;
    }

    bool exists = false;
    if (!customerExists(connection, customerId, exists, error)) {
        return CustomerDeleteStatus::error;
    }
    if (!exists) {
        return CustomerDeleteStatus::notFound;
    }

    bool hasReservations = false;
    if (!customerHasReservations(
            connection, customerId, hasReservations, error
        )) {
        return CustomerDeleteStatus::error;
    }
    if (hasReservations) {
        error = "Cannot delete this customer because reservations are linked to it.";
        return CustomerDeleteStatus::hasReservations;
    }

    StatementPtr statement = prepareStatement(
        connection,
        "DELETE FROM customers WHERE customer_id = ?",
        "Customer delete",
        error
    );
    if (!statement) {
        return CustomerDeleteStatus::error;
    }

    int requestedId = customerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], requestedId);
    if (!bindAndExecute(statement.get(), parameters, "Customer delete", error)) {
        if (mysql_stmt_errno(statement.get()) == ER_ROW_IS_REFERENCED_2) {
            error = "Cannot delete this customer because related records exist.";
            return CustomerDeleteStatus::hasReservations;
        }
        return CustomerDeleteStatus::error;
    }

    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        return CustomerDeleteStatus::notFound;
    }

    if (synchronizeMemory) {
        customers.deleteCustomer(customerId);
    }
    return CustomerDeleteStatus::deleted;
}

bool CustomerRepository::synchronizeCustomer(
    const Customer& customer,
    std::string& error
) {
    Customer* existing = customers.searchById(customer.getCustomerId());
    if (existing == nullptr) {
        if (!customers.insertCustomer(customer)) {
            error = "Customer already exists in the in-memory list.";
            return false;
        }
        return true;
    }

    existing->setFullName(customer.getFullName());
    existing->setPhoneNumber(customer.getPhoneNumber());
    existing->setEmail(customer.getEmail());
    existing->setIdentityDocument(customer.getIdentityDocument());
    return true;
}
