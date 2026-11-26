#include "Database.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr int kTestCustomerId = 9001;

struct StatementCloser {
    void operator()(MYSQL_STMT* statement) const {
        if (statement != nullptr) {
            mysql_stmt_close(statement);
        }
    }
};

using StatementPtr = std::unique_ptr<MYSQL_STMT, StatementCloser>;

StatementPtr prepareStatement(MYSQL* connection, const char* sql,
                              const char* operation) {
    StatementPtr statement(mysql_stmt_init(connection));
    if (!statement) {
        std::cerr << operation << ": could not initialize statement: "
                  << mysql_error(connection) << '\n';
        return {};
    }

    if (mysql_stmt_prepare(
            statement.get(), sql,
            static_cast<unsigned long>(std::strlen(sql))) != 0) {
        std::cerr << operation << ": could not prepare statement: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return {};
    }

    return statement;
}

bool bindAndExecute(MYSQL_STMT* statement, MYSQL_BIND* parameters,
                    const char* operation) {
    if (mysql_stmt_bind_param(statement, parameters) != 0) {
        std::cerr << operation << ": could not bind parameters: "
                  << mysql_stmt_error(statement) << '\n';
        return false;
    }

    if (mysql_stmt_execute(statement) != 0) {
        std::cerr << operation << ": execution failed: "
                  << mysql_stmt_error(statement) << '\n';
        return false;
    }

    return true;
}

void bindInteger(MYSQL_BIND& binding, int& value) {
    binding.buffer_type = MYSQL_TYPE_LONG;
    binding.buffer = &value;
}

void bindText(MYSQL_BIND& binding, char* value, unsigned long& length) {
    length = static_cast<unsigned long>(std::strlen(value));
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = value;
    binding.buffer_length = length;
    binding.length = &length;
}

void bindResultText(MYSQL_BIND& binding, char* buffer,
                    unsigned long bufferLength, unsigned long& valueLength,
                    my_bool& isNull, my_bool& hasError) {
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = buffer;
    binding.buffer_length = bufferLength;
    binding.length = &valueLength;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

bool queryCustomerExists(MYSQL* connection, bool& exists) {
    StatementPtr statement = prepareStatement(
        connection,
        "SELECT customer_id FROM customers WHERE customer_id = ?",
        "Customer existence check");
    if (!statement) {
        return false;
    }

    int customerId = kTestCustomerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], customerId);
    if (!bindAndExecute(statement.get(), parameters,
                        "Customer existence check")) {
        return false;
    }

    int resultId = 0;
    my_bool isNull = 0;
    MYSQL_BIND results[1]{};
    bindInteger(results[0], resultId);
    results[0].is_null = &isNull;

    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        std::cerr << "Customer existence check: could not bind result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    if (mysql_stmt_store_result(statement.get()) != 0) {
        std::cerr << "Customer existence check: could not store result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        exists = false;
        return true;
    }
    if (fetchStatus != 0) {
        std::cerr << "Customer existence check: could not fetch result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    if (isNull || resultId != kTestCustomerId) {
        std::cerr << "Customer existence check returned an unexpected value.\n";
        return false;
    }

    exists = true;
    return true;
}

bool selectAndDisplayCustomer(MYSQL* connection,
                              const char* expectedPhoneNumber) {
    StatementPtr statement = prepareStatement(
        connection,
        "SELECT customer_id, full_name, phone_number, email, "
        "identity_document FROM customers WHERE customer_id = ?",
        "Customer select");
    if (!statement) {
        return false;
    }

    int requestedId = kTestCustomerId;
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], requestedId);
    if (!bindAndExecute(statement.get(), parameters, "Customer select")) {
        return false;
    }

    int customerId = 0;
    char fullName[151]{};
    char phoneNumber[31]{};
    char email[255]{};
    char identityDocument[51]{};
    unsigned long lengths[5]{};
    my_bool nulls[5]{};
    my_bool errors[5]{};

    MYSQL_BIND results[5]{};
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = &customerId;
    results[0].is_null = &nulls[0];
    results[0].error = &errors[0];
    bindResultText(results[1], fullName, sizeof(fullName), lengths[1],
                   nulls[1], errors[1]);
    bindResultText(results[2], phoneNumber, sizeof(phoneNumber), lengths[2],
                   nulls[2], errors[2]);
    bindResultText(results[3], email, sizeof(email), lengths[3],
                   nulls[3], errors[3]);
    bindResultText(results[4], identityDocument, sizeof(identityDocument),
                   lengths[4], nulls[4], errors[4]);

    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        std::cerr << "Customer select: could not bind result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    if (mysql_stmt_store_result(statement.get()) != 0) {
        std::cerr << "Customer select: could not store result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        std::cerr << "Customer select: customer ID " << kTestCustomerId
                  << " was not found.\n";
        return false;
    }
    if (fetchStatus != 0) {
        std::cerr << "Customer select: could not fetch result: "
                  << mysql_stmt_error(statement.get()) << '\n';
        return false;
    }

    for (my_bool isNull : nulls) {
        if (isNull) {
            std::cerr << "Customer select returned an unexpected NULL field.\n";
            return false;
        }
    }
    for (my_bool hasError : errors) {
        if (hasError) {
            std::cerr << "Customer select result exceeded its output buffer.\n";
            return false;
        }
    }

    if (customerId != kTestCustomerId) {
        std::cerr << "Customer select returned an unexpected customer ID.\n";
        return false;
    }

    const std::string retrievedPhoneNumber(phoneNumber, lengths[2]);
    if (retrievedPhoneNumber != expectedPhoneNumber) {
        std::cerr << "Customer select returned an unexpected phone number.\n";
        return false;
    }

    std::cout << "customer_id: " << customerId << '\n'
              << "full_name: " << std::string(fullName, lengths[1]) << '\n'
              << "phone_number: " << retrievedPhoneNumber << '\n'
              << "email: " << std::string(email, lengths[3]) << '\n'
              << "identity_document: "
              << std::string(identityDocument, lengths[4]) << '\n';
    return true;
}

bool executeCustomerCrud(MYSQL* connection) {
    if (mysql_query(connection, "START TRANSACTION") != 0) {
        std::cerr << "Could not start CRUD transaction: "
                  << mysql_error(connection) << '\n';
        return false;
    }

    const auto rollbackAndFail = [connection](const char* message) {
        std::cerr << message << '\n';
        if (mysql_rollback(connection) != 0) {
            std::cerr << "Transaction rollback failed: "
                      << mysql_error(connection) << '\n';
        }
        return false;
    };

    bool customerAlreadyExists = false;
    if (!queryCustomerExists(connection, customerAlreadyExists)) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }
    if (customerAlreadyExists) {
        return rollbackAndFail(
            "Customer ID 9001 already exists; refusing to modify it.");
    }

    char fullName[] = "CRUD Test Customer 9001";
    char phoneNumber[] = "555-9001";
    char email[] = "crud-test-9001@example.test";
    char identityDocument[] = "TEST-ID-9001";
    unsigned long fullNameLength = 0;
    unsigned long phoneNumberLength = 0;
    unsigned long emailLength = 0;
    unsigned long identityDocumentLength = 0;

    StatementPtr insertStatement = prepareStatement(
        connection,
        "INSERT INTO customers "
        "(customer_id, full_name, phone_number, email, identity_document) "
        "VALUES (?, ?, ?, ?, ?)",
        "Customer insert");
    if (!insertStatement) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }

    int customerId = kTestCustomerId;
    MYSQL_BIND insertParameters[5]{};
    bindInteger(insertParameters[0], customerId);
    bindText(insertParameters[1], fullName, fullNameLength);
    bindText(insertParameters[2], phoneNumber, phoneNumberLength);
    bindText(insertParameters[3], email, emailLength);
    bindText(insertParameters[4], identityDocument, identityDocumentLength);
    if (!bindAndExecute(insertStatement.get(), insertParameters,
                        "Customer insert")) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }
    if (mysql_stmt_affected_rows(insertStatement.get()) != 1) {
        return rollbackAndFail("Customer insert did not insert exactly one row.");
    }
    insertStatement.reset();

    std::cout << "After INSERT:\n";
    if (!selectAndDisplayCustomer(connection, phoneNumber)) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }

    char updatedPhoneNumber[] = "555-9002";
    unsigned long updatedPhoneNumberLength = 0;
    StatementPtr updateStatement = prepareStatement(
        connection,
        "UPDATE customers SET phone_number = ? WHERE customer_id = ?",
        "Customer update");
    if (!updateStatement) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }

    MYSQL_BIND updateParameters[2]{};
    bindText(updateParameters[0], updatedPhoneNumber,
             updatedPhoneNumberLength);
    bindInteger(updateParameters[1], customerId);
    if (!bindAndExecute(updateStatement.get(), updateParameters,
                        "Customer update")) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }
    if (mysql_stmt_affected_rows(updateStatement.get()) != 1) {
        return rollbackAndFail("Customer update did not update exactly one row.");
    }
    updateStatement.reset();

    std::cout << "\nAfter UPDATE:\n";
    if (!selectAndDisplayCustomer(connection, updatedPhoneNumber)) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }

    StatementPtr deleteStatement = prepareStatement(
        connection,
        "DELETE FROM customers WHERE customer_id = ?",
        "Customer delete");
    if (!deleteStatement) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }

    MYSQL_BIND deleteParameters[1]{};
    bindInteger(deleteParameters[0], customerId);
    if (!bindAndExecute(deleteStatement.get(), deleteParameters,
                        "Customer delete")) {
        return rollbackAndFail("CRUD test aborted; rolling back.");
    }
    if (mysql_stmt_affected_rows(deleteStatement.get()) != 1) {
        return rollbackAndFail("Customer delete did not delete exactly one row.");
    }
    deleteStatement.reset();

    bool customerStillExists = false;
    if (!queryCustomerExists(connection, customerStillExists)) {
        return rollbackAndFail("Final customer verification failed; rolling back.");
    }
    if (customerStillExists) {
        return rollbackAndFail(
            "Customer ID 9001 still exists after DELETE; rolling back.");
    }

    if (mysql_commit(connection) != 0) {
        std::cerr << "CRUD transaction commit failed: "
                  << mysql_error(connection) << '\n';
        if (mysql_rollback(connection) != 0) {
            std::cerr << "Transaction rollback failed: "
                      << mysql_error(connection) << '\n';
        }
        return false;
    }

    std::cout << "\nVerified customer ID 9001 no longer exists.\n"
              << "Customer CRUD transaction committed.\n";
    return true;
}

}  // namespace

int main() {
    Database database;
    if (!database.connect()) {
        database.disconnect();
        return 1;
    }

    MYSQL* connection = database.getConnection();
    if (connection == nullptr) {
        std::cerr << "Database connection handle is unavailable.\n";
        database.disconnect();
        return 1;
    }

    const bool success = executeCustomerCrud(connection);
    database.disconnect();
    return success ? 0 : 1;
}
