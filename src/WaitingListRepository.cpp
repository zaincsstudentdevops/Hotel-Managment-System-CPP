#include "WaitingListRepository.h"

#include "DatabaseFieldValidation.h"
#include "DatabaseStatement.h"

#include <set>

WaitingListRepository::WaitingListRepository(Database& database)
    : database(database) {}

bool WaitingListRepository::loadEntries(
    std::vector<std::string>& names,
    std::string& error
) const {
    std::vector<WaitingListEntrySnapshot> entries;
    if (!loadEntrySnapshots(entries, error)) {
        return false;
    }
    std::vector<std::string> loadedNames;
    loadedNames.reserve(entries.size());
    for (const WaitingListEntrySnapshot& entry : entries) {
        loadedNames.push_back(entry.customerName);
    }
    names.swap(loadedNames);
    return true;
}

bool WaitingListRepository::loadEntrySnapshots(
    std::vector<WaitingListEntrySnapshot>& entries,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "SELECT waiting_entry_id, customer_name "
        "FROM waiting_list ORDER BY waiting_entry_id",
        error
    );
    if (!statement.valid() || !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    unsigned long long entryId = 0;
    char name[601]{};
    unsigned long nameLength = 0;
    my_bool nulls[2]{};
    my_bool errors[2]{};
    MYSQL_BIND results[2]{};
    bindResultLongLong(results[0], entryId, nulls[0], errors[0]);
    bindResultText(results[1], name, sizeof(name), nameLength,
                   nulls[1], errors[1]);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Could not bind waiting-list results: ") +
                statement.statementError();
        return false;
    }

    std::vector<WaitingListEntrySnapshot> loaded;
    while (true) {
        const int fetchStatus = statement.fetch();
        if (fetchStatus == MYSQL_NO_DATA) {
            break;
        }
        if (fetchStatus != 0) {
            error = std::string("Could not fetch waiting-list records: ") +
                    statement.statementError();
            return false;
        }
        if (nulls[0] || nulls[1] || errors[0] || errors[1] ||
            entryId == 0 || nameLength == 0 || nameLength >= sizeof(name)) {
            error = "The database contains an invalid waiting-list record.";
            return false;
        }
        loaded.push_back({
            entryId,
            std::string(name, nameLength)
        });
    }
    entries.swap(loaded);
    error.clear();
    return true;
}

bool WaitingListRepository::enqueue(
    const std::string& name,
    unsigned long long& entryId,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || name.empty()) {
        error = "Waiting-list name is invalid.";
        return false;
    }
    if (!database_field_validation::withinCharacterLimit(
            database.getConnection(), name, 150,
            "Waiting-list name", error
        )) {
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "INSERT INTO waiting_list (customer_name) VALUES (?)",
        error
    );
    std::string customerName = name;
    if (!statement.valid() ||
        !statement.bindText(0, customerName, error) ||
        !statement.execute(error)) {
        return false;
    }
    entryId = statement.insertId();
    if (statement.affectedRows() != 1 || entryId == 0) {
        error = "MariaDB did not return a valid waiting-list entry ID.";
        return false;
    }
    return true;
}

bool WaitingListRepository::dequeueOldest(
    std::string& name,
    unsigned long long& entryId,
    bool& found,
    std::string& error
) const {
    error.clear();
    found = false;
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    DatabaseStatement select(
        database.getConnection(),
        "SELECT waiting_entry_id, customer_name FROM waiting_list "
        "ORDER BY waiting_entry_id LIMIT 1 FOR UPDATE",
        error
    );
    if (!select.valid() || !select.execute(error) ||
        !select.storeResult(error)) {
        return false;
    }

    unsigned long long selectedId = 0;
    char selectedName[601]{};
    unsigned long nameLength = 0;
    my_bool nulls[2]{};
    my_bool errors[2]{};
    MYSQL_BIND results[2]{};
    bindResultLongLong(results[0], selectedId, nulls[0], errors[0]);
    bindResultText(results[1], selectedName, sizeof(selectedName), nameLength,
                   nulls[1], errors[1]);
    if (mysql_stmt_bind_result(select.get(), results) != 0) {
        error = std::string("Could not bind oldest waiting-list entry: ") +
                select.statementError();
        return false;
    }
    const int fetchStatus = select.fetch();
    if (fetchStatus == MYSQL_NO_DATA) {
        error.clear();
        return true;
    }
    if (fetchStatus != 0 || nulls[0] || nulls[1] || errors[0] || errors[1] ||
        selectedId == 0 || nameLength >= sizeof(selectedName)) {
        error = std::string("Could not read oldest waiting-list entry: ") +
                select.statementError();
        return false;
    }
    name.assign(selectedName, nameLength);
    entryId = selectedId;
    found = true;
    return deleteEntry(entryId, error);
}

bool WaitingListRepository::deleteEntry(
    unsigned long long entryId,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || entryId == 0) {
        error = "Waiting-list entry ID is invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "DELETE FROM waiting_list WHERE waiting_entry_id = ?",
        error
    );
    if (!statement.valid() ||
        !statement.bindUnsignedLongLong(0, entryId, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "Waiting-list entry was not deleted.";
        return false;
    }
    return true;
}
