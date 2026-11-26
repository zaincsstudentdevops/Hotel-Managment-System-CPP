
#include <iostream>
#include <mysql.h>

int main()
{
    MYSQL* connection = mysql_init(nullptr);

    if (connection == nullptr)
    {
        std::cerr << "Error: Could not initialize MariaDB connection.\n";
        return 1;
    }
    my_bool ssl_enforce = 0;
    mysql_options(connection, MYSQL_OPT_SSL_ENFORCE, &ssl_enforce);

    my_bool verify_server_cert = 0;
    int verify_rc = mysql_optionsv(connection, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify_server_cert);
    std::cerr << "SSL_VERIFY_SERVER_CERT mysql_optionsv return: " << verify_rc << '\n';

    MYSQL* connected = mysql_real_connect(
        connection,
        "127.0.0.1",
        "root",
        "",
        "hotel_management",
        3306,
        nullptr,
        0
    );

    if (connected == nullptr)
    {
        std::cerr << "Connection failed: "
                  << mysql_error(connection) << '\n';

        mysql_close(connection);
        return 1;
    }

    std::cout << "Successfully connected to MariaDB!\n";
    std::cout << "Database: hotel_management\n";

    mysql_close(connection);

    return 0;
}
