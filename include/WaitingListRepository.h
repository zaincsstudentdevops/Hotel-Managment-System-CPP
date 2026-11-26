#ifndef WAITINGLISTREPOSITORY_H
#define WAITINGLISTREPOSITORY_H

#include "Database.h"

#include <string>
#include <vector>

struct WaitingListEntrySnapshot {
    unsigned long long entryId;
    std::string customerName;
};

class WaitingListRepository {
public:
    explicit WaitingListRepository(Database& database);

    bool loadEntries(std::vector<std::string>& names, std::string& error) const;
    bool loadEntrySnapshots(
        std::vector<WaitingListEntrySnapshot>& entries,
        std::string& error
    ) const;
    bool enqueue(
        const std::string& name,
        unsigned long long& entryId,
        std::string& error
    ) const;
    bool dequeueOldest(
        std::string& name,
        unsigned long long& entryId,
        bool& found,
        std::string& error
    ) const;
    bool deleteEntry(unsigned long long entryId, std::string& error) const;

private:
    Database& database;
};

#endif
