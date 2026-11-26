#ifndef DATABASETRANSACTION_H
#define DATABASETRANSACTION_H

#include "Database.h"

#include <string>

class DatabaseTransaction {
public:
    explicit DatabaseTransaction(Database& database);
    ~DatabaseTransaction();

    DatabaseTransaction(const DatabaseTransaction&) = delete;
    DatabaseTransaction& operator=(const DatabaseTransaction&) = delete;

    bool begin(std::string& error);
    bool commit(std::string& error);
    bool rollback(std::string& error);
    bool isActive() const;

private:
    MYSQL* connection;
    bool active;
};

#endif
