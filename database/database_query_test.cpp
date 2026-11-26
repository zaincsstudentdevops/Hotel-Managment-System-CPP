#include "Database.h"

#include <iostream>

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

    if (mysql_query(connection, "SELECT 1") != 0) {
        std::cerr << "Query failed: " << mysql_error(connection) << '\n';
        database.disconnect();
        return 1;
    }

    MYSQL_RES* result = mysql_store_result(connection);
    if (result == nullptr) {
        if (mysql_field_count(connection) == 0) {
            std::cerr << "SELECT 1 did not return a result set.\n";
        } else {
            std::cerr << "Failed to retrieve query result: "
                      << mysql_error(connection) << '\n';
        }
        database.disconnect();
        return 1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        if (mysql_errno(connection) != 0) {
            std::cerr << "Failed to fetch query result: "
                      << mysql_error(connection) << '\n';
        } else {
            std::cerr << "SELECT 1 returned no rows.\n";
        }
        mysql_free_result(result);
        database.disconnect();
        return 1;
    }

    if (row[0] == nullptr) {
        std::cerr << "SELECT 1 returned a NULL value.\n";
        mysql_free_result(result);
        database.disconnect();
        return 1;
    }

    std::cout << "SELECT 1 returned: " << row[0] << '\n';

    mysql_free_result(result);
    database.disconnect();
    return 0;
}
