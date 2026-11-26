#include "PaymentRepository.h"

#include "DatabaseStatement.h"
#include "Money.h"

#include <cmath>
#include <limits>
#include <set>

namespace {

bool validMethod(const std::string& method) {
    return method == "Cash" || method == "Card";
}

bool validDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }
    int values[3]{};
    int component = 0;
    for (std::size_t index = 0; index < date.size(); ++index) {
        if (index == 4 || index == 7) {
            ++component;
            continue;
        }
        if (date[index] < '0' || date[index] > '9') {
            return false;
        }
        values[component] = values[component] * 10 + date[index] - '0';
    }
    const int year = values[0];
    const int month = values[1];
    const int day = values[2];
    if (year == 0 || month < 1 || month > 12) {
        return false;
    }
    const bool leapYear =
        (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int daysInMonth[] = {
        31, leapYear ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    return day >= 1 && day <= daysInMonth[month - 1];
}

}

PaymentRepository::PaymentRepository(Database& database) : database(database) {}

bool PaymentRepository::loadPayments(
    std::vector<Payment>& payments,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "SELECT payment_id, reservation_id, amount, payment_method, "
        "payment_date FROM payments ORDER BY payment_id",
        error
    );
    if (!statement.valid() || !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    int paymentId = 0;
    int reservationId = 0;
    double amount = 0.0;
    char method[16]{};
    char date[16]{};
    unsigned long methodLength = 0;
    unsigned long dateLength = 0;
    my_bool nulls[5]{};
    my_bool errors[5]{};
    MYSQL_BIND results[5]{};
    bindResultInt(results[0], paymentId, nulls[0], errors[0]);
    bindResultInt(results[1], reservationId, nulls[1], errors[1]);
    bindResultDouble(results[2], amount, nulls[2], errors[2]);
    bindResultText(results[3], method, sizeof(method), methodLength,
                   nulls[3], errors[3]);
    bindResultText(results[4], date, sizeof(date), dateLength,
                   nulls[4], errors[4]);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Could not bind payment results: ") +
                statement.statementError();
        return false;
    }

    std::vector<Payment> loaded;
    std::set<int> reservationIds;
    while (true) {
        const int fetchStatus = statement.fetch();
        if (fetchStatus == MYSQL_NO_DATA) {
            break;
        }
        if (fetchStatus != 0) {
            error = std::string("Could not fetch payment records: ") +
                    statement.statementError();
            return false;
        }
        if (nulls[0] || nulls[1] || nulls[2] || nulls[3] || nulls[4] ||
            errors[0] || errors[1] || errors[2] || errors[3] || errors[4] ||
            methodLength >= sizeof(method) || dateLength != 10 ||
            paymentId <= 0 || reservationId <= 0 ||
            !std::isfinite(amount) || amount <= 0.0) {
            error = "The database contains an invalid payment record.";
            return false;
        }
        const std::string methodValue(method, methodLength);
        const std::string dateValue(date, dateLength);
        if (!validMethod(methodValue) || !validDate(dateValue) ||
            !reservationIds.insert(reservationId).second) {
            error = "The database contains an invalid or duplicate payment.";
            return false;
        }
        loaded.emplace_back(
            paymentId, reservationId, amount, methodValue, dateValue
        );
    }
    payments.swap(loaded);
    error.clear();
    return true;
}

bool PaymentRepository::insertPayment(
    int reservationId,
    double amount,
    const std::string& method,
    const std::string& date,
    Payment& payment,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || reservationId <= 0 ||
        !std::isfinite(amount) || amount <= 0.0 ||
        !validMethod(method) || !validDate(date)) {
        error = "Payment information is invalid.";
        return false;
    }
    double normalizedAmount = 0.0;
    if (!hotel_money::normalize(
            amount,
            hotel_money::kDecimal12_2MaxCents,
            normalizedAmount
        )) {
        error = "Payment amount must round to a positive value within DECIMAL(12,2).";
        return false;
    }
    std::int64_t amountCents = 0;
    if (!hotel_money::toCents(
            normalizedAmount,
            hotel_money::kDecimal12_2MaxCents,
            amountCents
        )) {
        error = "Payment amount could not be normalized to cents.";
        return false;
    }
    std::string amountText = std::to_string(amountCents / 100) + ".";
    const int fractionalCents = static_cast<int>(amountCents % 100);
    if (fractionalCents < 10) {
        amountText += "0";
    }
    amountText += std::to_string(fractionalCents);

    DatabaseStatement statement(
        database.getConnection(),
        "INSERT INTO payments "
        "(reservation_id, amount, payment_method, payment_date) "
        "SELECT reservation_id, total_cost, ?, ? FROM reservations "
        "WHERE reservation_id = ? "
        "AND total_cost = CAST(? AS DECIMAL(12, 2))",
        error
    );
    std::string paymentMethod = method;
    std::string paymentDate = date;
    if (!statement.valid() ||
        !statement.bindText(0, paymentMethod, error) ||
        !statement.bindText(1, paymentDate, error) ||
        !statement.bindInt(2, reservationId, error) ||
        !statement.bindText(3, amountText, error) ||
        !statement.execute(error)) {
        if (statement.valid() && mysql_stmt_errno(statement.get()) == 1062) {
            error = "A payment already exists for this reservation.";
        }
        return false;
    }
    if (statement.affectedRows() == 0) {
        error = "Payment amount does not match the reservation total in MariaDB.";
        return false;
    }
    if (statement.affectedRows() != 1 || statement.insertId() == 0 ||
        statement.insertId() >
            static_cast<my_ulonglong>(std::numeric_limits<int>::max())) {
        error = "MariaDB did not return a valid generated payment ID.";
        return false;
    }
    payment = Payment(
        static_cast<int>(statement.insertId()),
        reservationId,
        normalizedAmount,
        method,
        date
    );
    return true;
}
