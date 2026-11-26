#ifndef DATABASE_H
#define DATABASE_H

#include <mysql.h>

class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool connect();
    void disconnect();
    bool isConnected() const;
    MYSQL* getConnection() const;

private:
    MYSQL* connection;
};

#endif