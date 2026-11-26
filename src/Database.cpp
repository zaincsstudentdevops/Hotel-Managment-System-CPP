#include "Database.h"

#include <iostream>

Database::Database() : connection(nullptr) {}

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    if (isConnected()) {
        return true;
    }

    connection = mysql_init(nullptr);
    if (connection == nullptr) {
        std::cerr << "Database initialization failed: mysql_init returned null.\n";
        return false;
    }

    my_bool ssl_enforce = 0;
    if (mysql_options(connection, MYSQL_OPT_SSL_ENFORCE, &ssl_enforce) != 0) {
        std::cerr << "Database SSL configuration failed: "
                  << mysql_error(connection) << '\n';
        disconnect();
        return false;
    }

    my_bool verify_server_cert = 0;
    if (mysql_optionsv(connection, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                       &verify_server_cert) != 0) {
        std::cerr << "Database SSL verification configuration failed: "
                  << mysql_error(connection) << '\n';
        disconnect();
        return false;
    }

    if (mysql_real_connect(
            connection,
            "127.0.0.1",
            "root",
            "",
            "hotel_management",
            3306,
            nullptr,
            0) == nullptr) {
        std::cerr << "Database connection failed: "
                  << mysql_error(connection) << '\n';
        disconnect();
        return false;
    }

    return true;
}

void Database::disconnect() {
    if (connection != nullptr) {
        mysql_close(connection);
        connection = nullptr;
    }
}

bool Database::isConnected() const {
    return connection != nullptr;
}

MYSQL* Database::getConnection() const {
    return connection;
}