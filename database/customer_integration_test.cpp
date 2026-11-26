#include "CustomerRepository.h"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace {

constexpr int kCustomerIdA = 982651;
constexpr int kCustomerIdB = 982652;
constexpr int kRoomNumber = 982651;
constexpr int kReservationId = 982651;

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
            statement.get(), sql,
            static_cast<unsigned long>(std::strlen(sql))
        ) != 0) {
        error = std::string(operation) + ": could not prepare statement: " +
                mysql_stmt_error(statement.get());
        return {};
    }
    return statement;
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
        error = "Test value is too long for MariaDB Connector/C.";
        return false;
    }
    length = static_cast<unsigned long>(value.size());
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = value.data();
    binding.buffer_length = length;
    binding.length = &length;
    return true;
}

bool executeStatement(
    MYSQL* connection,
    const char* sql,
    MYSQL_BIND* parameters,
    unsigned long parameterCount,
    const char* operation,
    std::string& error
) {
    StatementPtr statement = prepareStatement(
        connection, sql, operation, error
    );
    if (!statement) {
        return false;
    }
    if (mysql_stmt_param_count(statement.get()) != parameterCount) {
        error = std::string(operation) + ": unexpected parameter count.";
        return false;
    }
    if (mysql_stmt_bind_param(statement.get(), parameters) != 0) {
        error = std::string(operation) + ": could not bind parameters: " +
                mysql_stmt_error(statement.get());
        return false;
    }
    if (mysql_stmt_execute(statement.get()) != 0) {
        error = std::string(operation) + ": execution failed: " +
                mysql_stmt_error(statement.get());
        return false;
    }
    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        error = std::string(operation) + ": expected exactly one affected row.";
        return false;
    }
    return true;
}

bool rowExists(
    MYSQL* connection,
    const char* sql,
    int id,
    bool& exists,
    std::string& error
) {
    StatementPtr statement = prepareStatement(
        connection, sql, "Test ID preflight", error
    );
    if (!statement) {
        return false;
    }
    MYSQL_BIND parameters[1]{};
    bindInteger(parameters[0], id);
    if (mysql_stmt_bind_param(statement.get(), parameters) != 0) {
        error = std::string("Test ID preflight could not bind: ") +
                mysql_stmt_error(statement.get());
        return false;
    }
    if (mysql_stmt_execute(statement.get()) != 0) {
        error = std::string("Test ID preflight failed: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    int resultId = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND results[1]{};
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = &resultId;
    results[0].is_null = &isNull;
    results[0].error = &hasError;
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Test ID preflight could not bind results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }
    if (mysql_stmt_store_result(statement.get()) != 0) {
        error = std::string("Test ID preflight could not store results: ") +
                mysql_stmt_error(statement.get());
        return false;
    }

    const int fetchStatus = mysql_stmt_fetch(statement.get());
    if (fetchStatus == MYSQL_NO_DATA) {
        exists = false;
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError) {
        error = std::string("Test ID preflight could not fetch result: ") +
                mysql_stmt_error(statement.get());
        return false;
    }
    exists = true;
    return true;
}

bool checkAbsent(
    MYSQL* connection,
    const char* sql,
    int id,
    std::string& error
) {
    bool exists = false;
    if (!rowExists(connection, sql, id, exists, error)) {
        return false;
    }
    if (exists) {
        error = "A reserved integration-test ID is already in use; "
                "no records were changed.";
        return false;
    }
    return true;
}

bool getCustomerCount(MYSQL* connection, std::size_t& count,
                      std::string& error) {
    if (mysql_query(connection, "SELECT COUNT(*) FROM customers") != 0) {
        error = std::string("Could not count customers: ") +
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
        error = std::string("Could not retrieve customer count: ") +
                mysql_error(connection);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr || row[0] == nullptr) {
        error = "Customer count query returned no value.";
        return false;
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(row[0], &end, 10);
    if (end == row[0] || *end != '\0') {
        error = "Customer count query returned an invalid value.";
        return false;
    }
    count = static_cast<std::size_t>(parsed);
    return true;
}

bool insertTestRoom(MYSQL* connection, std::string& error) {
    std::string category = "Single";
    std::string status = "Available";
    unsigned long categoryLength = 0;
    unsigned long statusLength = 0;
    int roomNumber = kRoomNumber;
    double price = 1.0;
    MYSQL_BIND parameters[4]{};
    bindInteger(parameters[0], roomNumber);
    if (!bindText(parameters[1], category, categoryLength, error)) {
        return false;
    }
    parameters[2].buffer_type = MYSQL_TYPE_DOUBLE;
    parameters[2].buffer = &price;
    if (!bindText(parameters[3], status, statusLength, error)) {
        return false;
    }
    return executeStatement(
        connection,
        "INSERT INTO rooms (room_number, category, price_per_night, status) "
        "VALUES (?, ?, ?, ?)",
        parameters,
        4,
        "Temporary test room insert",
        error
    );
}

bool insertTestReservation(MYSQL* connection, std::string& error) {
    int reservationId = kReservationId;
    int customerId = kCustomerIdB;
    int roomNumber = kRoomNumber;
    std::string checkIn = "2099-01-01";
    std::string checkOut = "2099-01-02";
    std::string status = "Confirmed";
    double totalCost = 1.0;
    unsigned long lengths[3]{};
    MYSQL_BIND parameters[7]{};
    bindInteger(parameters[0], reservationId);
    bindInteger(parameters[1], customerId);
    bindInteger(parameters[2], roomNumber);
    if (!bindText(parameters[3], checkIn, lengths[0], error) ||
        !bindText(parameters[4], checkOut, lengths[1], error)) {
        return false;
    }
    parameters[5].buffer_type = MYSQL_TYPE_DOUBLE;
    parameters[5].buffer = &totalCost;
    if (!bindText(parameters[6], status, lengths[2], error)) {
        return false;
    }
    return executeStatement(
        connection,
        "INSERT INTO reservations "
        "(reservation_id, customer_id, room_number, check_in_date, "
        "check_out_date, total_cost, status) VALUES (?, ?, ?, ?, ?, ?, ?)",
        parameters,
        7,
        "Temporary test reservation insert",
        error
    );
}

bool verifyTestRowsAbsent(MYSQL* connection, std::string& error) {
    return checkAbsent(
               connection,
               "SELECT customer_id FROM customers WHERE customer_id = ?",
               kCustomerIdA,
               error
           ) &&
           checkAbsent(
               connection,
               "SELECT customer_id FROM customers WHERE customer_id = ?",
               kCustomerIdB,
               error
           ) &&
           checkAbsent(
               connection,
               "SELECT room_number FROM rooms WHERE room_number = ?",
               kRoomNumber,
               error
           ) &&
           checkAbsent(
               connection,
               "SELECT reservation_id FROM reservations "
               "WHERE reservation_id = ?",
               kReservationId,
               error
           );
}

bool runIntegrationTest() {
    Database database;
    if (!database.connect()) {
        std::cerr << "Could not connect to MariaDB for integration test.\n";
        return false;
    }
    MYSQL* connection = database.getConnection();
    if (connection == nullptr) {
        std::cerr << "Database connection handle is unavailable.\n";
        return false;
    }

    CustomerLinkedList customers;
    CustomerRepository repository(database, customers);
    std::string error;
    if (!repository.loadCustomers(error)) {
        std::cerr << "Customer startup load failed: " << error << '\n';
        return false;
    }

    std::size_t initialCustomerCount = 0;
    if (!getCustomerCount(connection, initialCustomerCount, error)) {
        std::cerr << error << '\n';
        return false;
    }
    if (customers.size() != initialCustomerCount) {
        std::cerr << "Startup load count does not match the database count.\n";
        return false;
    }
    std::cout << "Startup loaded " << customers.size()
              << " existing customer(s).\n";
    if (initialCustomerCount == 0) {
        std::cout << "Empty customer table loaded successfully.\n";
    }

    if (!verifyTestRowsAbsent(connection, error)) {
        std::cerr << error << '\n';
        return false;
    }
    if (!checkAbsent(
            connection,
            "SELECT reservation_id FROM reservations WHERE customer_id = ?",
            kCustomerIdA,
            error
        ) ||
        !checkAbsent(
            connection,
            "SELECT reservation_id FROM reservations WHERE customer_id = ?",
            kCustomerIdB,
            error
        ) ||
        !checkAbsent(
            connection,
            "SELECT reservation_id FROM reservations WHERE room_number = ?",
            kRoomNumber,
            error
        ) ||
        !checkAbsent(
            connection,
            "SELECT payment_id FROM payments WHERE reservation_id = ?",
            kReservationId,
            error
        )) {
        std::cerr << error << '\n';
        return false;
    }
    std::cout << "All temporary IDs were absent before the test.\n";

    if (mysql_query(connection, "START TRANSACTION") != 0) {
        std::cerr << "Could not start integration-test transaction: "
                  << mysql_error(connection) << '\n';
        return false;
    }
    bool transactionActive = true;
    bool success = true;
    const auto require = [&success](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            success = false;
        }
        return success;
    };

    const Customer customerA(
        kCustomerIdA,
        "Integration Test Customer A",
        "555-82651",
        "customer-a-982651@example.test",
        "TEST-ID-982651"
    );
    const Customer customerB(
        kCustomerIdB,
        "Integration Test Customer B",
        "555-82652",
        "customer-b-982652@example.test",
        "TEST-ID-982652"
    );

    if (success) {
        require(repository.registerCustomer(customerA, error),
                "Customer insertion failed.");
        if (!success) {
            std::cerr << error << '\n';
        } else {
            std::cout << "Customer insertion passed.\n";
        }
    }
    if (success) {
        require(repository.registerCustomer(customerB, error),
                "Second customer insertion failed.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        CustomerLinkedList reloadedCustomers;
        CustomerRepository reloader(database, reloadedCustomers);
        const bool loaded = reloader.loadCustomers(error);
        const Customer* loadedA = reloadedCustomers.searchById(kCustomerIdA);
        const Customer* loadedB = reloadedCustomers.searchById(kCustomerIdB);
        require(loaded &&
                    reloadedCustomers.size() == initialCustomerCount + 2 &&
                    loadedA != nullptr && loadedB != nullptr &&
                    loadedA->getEmail() == customerA.getEmail() &&
                    loadedB->getEmail() == customerB.getEmail(),
                "Loading existing customer rows did not populate the list.");
        if (success) {
            std::cout << "Loading existing customer rows passed.\n";
            const std::size_t loadedCount = reloadedCustomers.size();
            const bool loadedAgain = reloader.loadCustomers(error);
            require(!loadedAgain &&
                        reloadedCustomers.size() == loadedCount &&
                        error.find("already contains") != std::string::npos,
                    "Repeated loading created or failed to reject duplicates.");
            if (success) {
                std::cout << "Repeated-load duplicate prevention passed.\n";
            } else {
                std::cerr << error << '\n';
            }
        } else {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        CustomerLinkedList duplicateCache;
        CustomerRepository duplicateRepository(database, duplicateCache);
        const bool duplicateInserted =
            duplicateRepository.registerCustomer(customerA, error);
        require(!duplicateInserted && duplicateCache.size() == 0 &&
                    error.find("already exists") != std::string::npos,
                "Duplicate customer ID was not rejected safely.");
        if (success) {
            std::cout << "Duplicate customer ID rejected without changing memory.\n";
        } else {
            std::cerr << error << '\n';
        }
    }

    if (success) {
        Customer foundCustomer;
        const CustomerLookupStatus status =
            repository.findCustomerById(kCustomerIdA, foundCustomer, error);
        require(status == CustomerLookupStatus::found &&
                    foundCustomer.getFullName() == customerA.getFullName() &&
                    foundCustomer.getPhoneNumber() == customerA.getPhoneNumber() &&
                    foundCustomer.getEmail() == customerA.getEmail() &&
                    foundCustomer.getIdentityDocument() ==
                        customerA.getIdentityDocument(),
                "Customer retrieval returned incorrect data.");
        if (success) {
            std::cout << "Customer retrieval passed.\n";
        } else {
            std::cerr << error << '\n';
        }
    }

    const Customer updatedCustomer(
        kCustomerIdA,
        "Integration Test Customer A Updated",
        "555-90001",
        "customer-a-updated-982651@example.test",
        "TEST-ID-982651-UPDATED"
    );
    if (success) {
        const CustomerUpdateStatus status =
            repository.updateCustomer(updatedCustomer, error);
        require(status == CustomerUpdateStatus::updated,
                "Customer update failed.");
        if (success) {
            Customer foundCustomer;
            const CustomerLookupStatus lookupStatus =
                repository.findCustomerById(
                    kCustomerIdA, foundCustomer, error
                );
            require(lookupStatus == CustomerLookupStatus::found &&
                        foundCustomer.getFullName() ==
                            updatedCustomer.getFullName() &&
                        foundCustomer.getPhoneNumber() ==
                            updatedCustomer.getPhoneNumber() &&
                        foundCustomer.getEmail() == updatedCustomer.getEmail() &&
                        foundCustomer.getIdentityDocument() ==
                            updatedCustomer.getIdentityDocument(),
                    "Updated customer could not be verified.");
        }
        if (success) {
            std::cout << "Customer update and verification passed.\n";
        } else {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        const CustomerUpdateStatus status =
            repository.updateCustomer(updatedCustomer, error);
        require(status == CustomerUpdateStatus::unchanged,
                "Unchanged customer update was not reported correctly.");
        if (success) {
            std::cout << "Unchanged customer update was reported correctly.\n";
        } else {
            std::cerr << error << '\n';
        }
    }

    if (success) {
        require(insertTestRoom(connection, error),
                "Could not create the temporary room.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        require(insertTestReservation(connection, error),
                "Could not create the temporary reservation.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        const CustomerDeleteStatus status =
            repository.deleteCustomer(kCustomerIdB, error);
        require(status == CustomerDeleteStatus::hasReservations,
                "Customer with a reservation was not protected from deletion.");
        if (success) {
            std::cout << "Deletion with an existing reservation was restricted.\n";
        } else {
            std::cerr << error << '\n';
        }
    }

    if (success) {
        MYSQL_BIND reservationParameter[1]{};
        int reservationId = kReservationId;
        bindInteger(reservationParameter[0], reservationId);
        require(executeStatement(
                    connection,
                    "DELETE FROM reservations WHERE reservation_id = ?",
                    reservationParameter,
                    1,
                    "Temporary test reservation cleanup",
                    error
                ),
                "Could not remove the temporary reservation.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        MYSQL_BIND roomParameter[1]{};
        int roomNumber = kRoomNumber;
        bindInteger(roomParameter[0], roomNumber);
        require(executeStatement(
                    connection,
                    "DELETE FROM rooms WHERE room_number = ?",
                    roomParameter,
                    1,
                    "Temporary test room cleanup",
                    error
                ),
                "Could not remove the temporary room.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        const CustomerDeleteStatus status =
            repository.deleteCustomer(kCustomerIdA, error);
        require(status == CustomerDeleteStatus::deleted,
                "Unreserved customer deletion failed.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success) {
        const CustomerDeleteStatus status =
            repository.deleteCustomer(kCustomerIdB, error);
        require(status == CustomerDeleteStatus::deleted,
                "Customer deletion after reservation cleanup failed.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }

    if (success) {
        require(verifyTestRowsAbsent(connection, error),
                "Temporary records remain before commit.");
        if (!success) {
            std::cerr << error << '\n';
        }
    }
    if (success &&
        (customers.searchById(kCustomerIdA) != nullptr ||
         customers.searchById(kCustomerIdB) != nullptr ||
         customers.size() != initialCustomerCount)) {
        require(false, "In-memory customers were not cleaned up.");
    }

    if (success) {
        if (mysql_commit(connection) != 0) {
            std::cerr << "Integration-test commit failed: "
                      << mysql_error(connection) << '\n';
            success = false;
        } else {
            transactionActive = false;
        }
    }

    if (!success && transactionActive) {
        if (mysql_rollback(connection) != 0) {
            std::cerr << "Integration-test rollback failed: "
                      << mysql_error(connection) << '\n';
        } else {
            transactionActive = false;
            std::cout << "Test transaction rolled back.\n";
        }
    }

    std::string cleanupError;
    const bool recordsAbsent = verifyTestRowsAbsent(connection, cleanupError);
    if (!recordsAbsent) {
        std::cerr << "Could not verify temporary-record cleanup: "
                  << cleanupError << '\n';
        success = false;
    } else {
        std::cout << "Temporary customer, room, and reservation IDs are absent.\n";
    }
    if (transactionActive) {
        std::cerr << "Test transaction is still active; rolling it back.\n";
        if (mysql_rollback(connection) != 0) {
            std::cerr << "Final rollback failed: " << mysql_error(connection) << '\n';
        }
        success = false;
    }

    database.disconnect();
    if (database.isConnected()) {
        std::cerr << "Database disconnection check failed.\n";
        success = false;
    } else {
        std::cout << "Database disconnection passed.\n";
    }

    if (success) {
        std::cout << "Customer repository integration tests passed.\n";
    }
    return success;
}

}  // namespace

int main() {
    return runIntegrationTest() ? 0 : 1;
}
