#include "ActionHistoryRepository.h"

#include "DatabaseStatement.h"

#include <set>
#include <utility>

namespace {

constexpr unsigned long kMaximumTextBytes = 65535;

}

ActionHistoryRepository::ActionHistoryRepository(Database& database)
    : database(database) {}

bool ActionHistoryRepository::loadActions(
    std::vector<std::string>& actions,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "SELECT action_id, action_text FROM action_history ORDER BY action_id",
        error
    );
    if (!statement.valid() || !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    unsigned long long actionId = 0;
    char actionText[4097]{};
    unsigned long textLength = 0;
    my_bool nulls[2]{};
    my_bool errors[2]{};
    MYSQL_BIND results[2]{};
    bindResultLongLong(results[0], actionId, nulls[0], errors[0]);
    bindResultText(results[1], actionText, sizeof(actionText), textLength,
                   nulls[1], errors[1]);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Could not bind action-history results: ") +
                statement.statementError();
        return false;
    }

    std::vector<std::string> loaded;
    while (true) {
        const int fetchStatus = statement.fetch();
        if (fetchStatus == MYSQL_NO_DATA) {
            break;
        }
        if (fetchStatus != 0 && fetchStatus != MYSQL_DATA_TRUNCATED) {
            error = std::string("Could not fetch action-history records: ") +
                    statement.statementError();
            return false;
        }
        if (nulls[0] || nulls[1] || errors[0] || actionId == 0 ||
            textLength == 0 || textLength > kMaximumTextBytes) {
            error = "The database contains an invalid or oversized action.";
            return false;
        }

        if (errors[1] || textLength > sizeof(actionText)) {
            std::string fullText(textLength, '\0');
            unsigned long fetchedLength = 0;
            my_bool fetchedNull = 0;
            my_bool fetchedError = 0;
            MYSQL_BIND column{};
            column.buffer_type = MYSQL_TYPE_STRING;
            column.buffer = fullText.data();
            column.buffer_length = textLength;
            column.length = &fetchedLength;
            column.is_null = &fetchedNull;
            column.error = &fetchedError;
            if (mysql_stmt_fetch_column(
                    statement.get(), &column, 1, 0
                ) != 0 ||
                fetchedNull || fetchedError ||
                fetchedLength != textLength) {
                error = std::string("Could not fetch full action-history text: ") +
                        statement.statementError();
                return false;
            }
            loaded.push_back(std::move(fullText));
        } else {
            loaded.emplace_back(actionText, textLength);
        }
    }
    actions.swap(loaded);
    error.clear();
    return true;
}

bool ActionHistoryRepository::recordAction(
    const std::string& action,
    unsigned long long& actionId,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || action.empty() ||
        action.size() > kMaximumTextBytes) {
        error = "Action-history entry is invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "INSERT INTO action_history (action_text) VALUES (?)",
        error
    );
    std::string actionText = action;
    if (!statement.valid() ||
        !statement.bindText(0, actionText, error) ||
        !statement.execute(error)) {
        return false;
    }
    actionId = statement.insertId();
    if (statement.affectedRows() != 1 || actionId == 0) {
        error = "MariaDB did not return a valid action-history ID.";
        return false;
    }
    return true;
}

bool ActionHistoryRepository::deleteAction(
    unsigned long long actionId,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || actionId == 0) {
        error = "Action-history ID is invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "DELETE FROM action_history WHERE action_id = ?",
        error
    );
    if (!statement.valid() ||
        !statement.bindUnsignedLongLong(0, actionId, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "Action-history record was not deleted.";
        return false;
    }
    return true;
}
