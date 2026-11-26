#ifndef ACTIONHISTORYREPOSITORY_H
#define ACTIONHISTORYREPOSITORY_H

#include "Database.h"

#include <string>
#include <vector>

class ActionHistoryRepository {
public:
    explicit ActionHistoryRepository(Database& database);

    bool loadActions(std::vector<std::string>& actions, std::string& error) const;
    bool recordAction(
        const std::string& action,
        unsigned long long& actionId,
        std::string& error
    ) const;
    bool deleteAction(unsigned long long actionId, std::string& error) const;

private:
    Database& database;
};

#endif
