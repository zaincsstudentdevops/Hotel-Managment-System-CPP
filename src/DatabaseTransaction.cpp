#include "DatabaseTransaction.h"

#include <iostream>

DatabaseTransaction::DatabaseTransaction(Database& database)
    : connection(database.getConnection()), active(false) {}

DatabaseTransaction::~DatabaseTransaction() {
    if (active && mysql_rollback(connection) != 0) {
        std::cerr << "Database transaction rollback failed during cleanup: "
                  << mysql_error(connection) << '\n';
    }
}

bool DatabaseTransaction::begin(std::string& error) {
    error.clear();
    if (connection == nullptr) {
        error = "Cannot start a transaction without a database connection.";
        return false;
    }
    if (active) {
        error = "A database transaction is already active.";
        return false;
    }
    if (mysql_query(connection, "START TRANSACTION") != 0) {
        error = std::string("Could not start database transaction: ") +
                mysql_error(connection);
        return false;
    }
    active = true;
    return true;
}

bool DatabaseTransaction::commit(std::string& error) {
    error.clear();
    if (!active) {
        error = "Cannot commit because no database transaction is active.";
        return false;
    }
    if (mysql_commit(connection) != 0) {
        error = std::string("Database transaction commit failed: ") +
                mysql_error(connection);
        return false;
    }
    active = false;
    return true;
}

bool DatabaseTransaction::rollback(std::string& error) {
    error.clear();
    if (!active) {
        error.clear();
        return true;
    }
    if (mysql_rollback(connection) != 0) {
        error = std::string("Database transaction rollback failed: ") +
                mysql_error(connection);
        return false;
    }
    active = false;
    return true;
}

bool DatabaseTransaction::isActive() const {
    return active;
}
