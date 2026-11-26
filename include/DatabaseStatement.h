#ifndef DATABASESTATEMENT_H
#define DATABASESTATEMENT_H

#include <mysql.h>

#include <iostream>
#include <limits>
#include <string>
#include <vector>

class DatabaseStatement {
public:
    DatabaseStatement(MYSQL* connection, const char* sql, std::string& error)
        : connection(connection), statement(nullptr), lengths(), parameters() {
        error.clear();
        if (connection == nullptr) {
            error = "Cannot prepare a statement without a database connection.";
            return;
        }

        statement = mysql_stmt_init(connection);
        if (statement == nullptr) {
            error = std::string("Could not initialize statement: ") +
                    mysql_error(connection);
            return;
        }
        if (mysql_stmt_prepare(
                statement, sql,
                static_cast<unsigned long>(std::char_traits<char>::length(sql))
            ) != 0) {
            error = std::string("Could not prepare statement: ") +
                    mysql_stmt_error(statement);
            close();
            return;
        }

        const unsigned long count = mysql_stmt_param_count(statement);
        parameters.resize(count);
        lengths.resize(count, 0);
    }

    ~DatabaseStatement() {
        close();
    }

    DatabaseStatement(const DatabaseStatement&) = delete;
    DatabaseStatement& operator=(const DatabaseStatement&) = delete;

    bool valid() const {
        return statement != nullptr;
    }

    MYSQL_STMT* get() const {
        return statement;
    }

    bool bindInt(unsigned int index, int& value, std::string& error) {
        if (!validIndex(index, error)) {
            return false;
        }
        parameters[index].buffer_type = MYSQL_TYPE_LONG;
        parameters[index].buffer = &value;
        return true;
    }

    bool bindLongLong(
        unsigned int index,
        long long& value,
        std::string& error
    ) {
        if (!validIndex(index, error)) {
            return false;
        }
        parameters[index].buffer_type = MYSQL_TYPE_LONGLONG;
        parameters[index].buffer = &value;
        return true;
    }

    bool bindUnsignedLongLong(
        unsigned int index,
        unsigned long long& value,
        std::string& error
    ) {
        if (!validIndex(index, error)) {
            return false;
        }
        parameters[index].buffer_type = MYSQL_TYPE_LONGLONG;
        parameters[index].is_unsigned = 1;
        parameters[index].buffer = &value;
        return true;
    }

    bool bindDouble(unsigned int index, double& value, std::string& error) {
        if (!validIndex(index, error)) {
            return false;
        }
        parameters[index].buffer_type = MYSQL_TYPE_DOUBLE;
        parameters[index].buffer = &value;
        return true;
    }

    bool bindText(
        unsigned int index,
        std::string& value,
        std::string& error
    ) {
        if (!validIndex(index, error)) {
            return false;
        }
        if (value.size() > std::numeric_limits<unsigned long>::max()) {
            error = "Text parameter is too large for MariaDB Connector/C.";
            return false;
        }
        lengths[index] = static_cast<unsigned long>(value.size());
        parameters[index].buffer_type = MYSQL_TYPE_STRING;
        parameters[index].buffer = value.data();
        parameters[index].buffer_length = lengths[index];
        parameters[index].length = &lengths[index];
        return true;
    }

    bool execute(std::string& error) {
        if (!valid()) {
            error = "Cannot execute an invalid prepared statement.";
            return false;
        }
        if (!parameters.empty() &&
            mysql_stmt_bind_param(statement, parameters.data()) != 0) {
            error = std::string("Could not bind statement parameters: ") +
                    mysql_stmt_error(statement);
            return false;
        }
        if (mysql_stmt_execute(statement) != 0) {
            error = std::string("Statement execution failed: ") +
                    mysql_stmt_error(statement);
            return false;
        }
        return true;
    }

    bool storeResult(std::string& error) {
        if (mysql_stmt_store_result(statement) != 0) {
            error = std::string("Could not store statement results: ") +
                    mysql_stmt_error(statement);
            return false;
        }
        return true;
    }

    my_ulonglong affectedRows() const {
        return mysql_stmt_affected_rows(statement);
    }

    my_ulonglong insertId() const {
        return mysql_stmt_insert_id(statement);
    }

    int fetch() {
        return mysql_stmt_fetch(statement);
    }

    const char* statementError() const {
        return mysql_stmt_error(statement);
    }

private:
    MYSQL* connection;
    MYSQL_STMT* statement;
    std::vector<unsigned long> lengths;
    std::vector<MYSQL_BIND> parameters;

    bool validIndex(unsigned int index, std::string& error) const {
        if (!valid() || index >= parameters.size()) {
            error = "Prepared-statement parameter index is invalid.";
            return false;
        }
        return true;
    }

    void close() {
        if (statement != nullptr) {
            if (mysql_stmt_close(statement) != 0) {
                std::cerr << "Could not close MariaDB statement: "
                          << mysql_error(connection) << '\n';
            }
            statement = nullptr;
        }
    }
};

inline void bindResultInt(
    MYSQL_BIND& binding,
    int& value,
    my_bool& isNull,
    my_bool& hasError
) {
    binding.buffer_type = MYSQL_TYPE_LONG;
    binding.buffer = &value;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

inline void bindResultLongLong(
    MYSQL_BIND& binding,
    unsigned long long& value,
    my_bool& isNull,
    my_bool& hasError
) {
    binding.buffer_type = MYSQL_TYPE_LONGLONG;
    binding.is_unsigned = 1;
    binding.buffer = &value;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

inline void bindResultDouble(
    MYSQL_BIND& binding,
    double& value,
    my_bool& isNull,
    my_bool& hasError
) {
    binding.buffer_type = MYSQL_TYPE_DOUBLE;
    binding.buffer = &value;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

inline void bindResultText(
    MYSQL_BIND& binding,
    char* buffer,
    unsigned long bufferSize,
    unsigned long& length,
    my_bool& isNull,
    my_bool& hasError
) {
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = buffer;
    binding.buffer_length = bufferSize;
    binding.length = &length;
    binding.is_null = &isNull;
    binding.error = &hasError;
}

#endif
