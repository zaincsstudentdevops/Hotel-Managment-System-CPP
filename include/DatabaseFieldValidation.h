#ifndef DATABASEFIELDVALIDATION_H
#define DATABASEFIELDVALIDATION_H

#include "DatabaseStatement.h"

#include <cstddef>
#include <string>

namespace database_field_validation {

inline bool withinCharacterLimit(
    MYSQL* connection,
    const std::string& value,
    std::size_t maximumCharacters,
    const std::string& fieldName,
    std::string& error
) {
    if (connection == nullptr) {
        error = "Cannot validate " + fieldName +
                " without a database connection.";
        return false;
    }

    DatabaseStatement statement(
        connection, "SELECT CHAR_LENGTH(?)", error
    );
    std::string parameter = value;
    if (!statement.valid() ||
        !statement.bindText(0, parameter, error) ||
        !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    unsigned long long characterCount = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND result[1]{};
    bindResultLongLong(result[0], characterCount, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), result) != 0) {
        error = std::string("Could not read the ") + fieldName +
                " character count: " + statement.statementError();
        return false;
    }

    const int fetchStatus = statement.fetch();
    if (fetchStatus != 0 || isNull || hasError) {
        error = std::string("Could not validate the ") + fieldName +
                " length: " + statement.statementError();
        return false;
    }
    if (characterCount > maximumCharacters) {
        error = fieldName + " exceeds the database limit of " +
                std::to_string(maximumCharacters) + " characters.";
        return false;
    }
    return true;
}

}

#endif
