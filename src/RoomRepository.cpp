#include "RoomRepository.h"

#include "DatabaseStatement.h"
#include "Money.h"

#include <cmath>
#include <set>

namespace {

bool validCategory(const std::string& category) {
    return category == "Single" || category == "Double" ||
           category == "Deluxe";
}

bool validStatus(const std::string& status) {
    return status == "Available" || status == "Occupied" ||
           status == "Maintenance";
}

}

RoomRepository::RoomRepository(Database& database) : database(database) {}

bool RoomRepository::loadRooms(
    std::vector<Room>& rooms,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }

    DatabaseStatement statement(
        database.getConnection(),
        "SELECT room_number, category, price_per_night, status "
        "FROM rooms ORDER BY room_number",
        error
    );
    if (!statement.valid() || !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    int number = 0;
    double price = 0.0;
    char category[32]{};
    char status[32]{};
    unsigned long categoryLength = 0;
    unsigned long statusLength = 0;
    my_bool nulls[4]{};
    my_bool errors[4]{};
    MYSQL_BIND results[4]{};
    bindResultInt(results[0], number, nulls[0], errors[0]);
    bindResultText(results[1], category, sizeof(category), categoryLength,
                   nulls[1], errors[1]);
    bindResultDouble(results[2], price, nulls[2], errors[2]);
    bindResultText(results[3], status, sizeof(status), statusLength,
                   nulls[3], errors[3]);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Could not bind room results: ") +
                statement.statementError();
        return false;
    }

    std::vector<Room> loaded;
    std::set<int> roomNumbers;
    while (true) {
        const int fetchStatus = statement.fetch();
        if (fetchStatus == MYSQL_NO_DATA) {
            break;
        }
        if (fetchStatus != 0) {
            error = std::string("Could not fetch room records: ") +
                    statement.statementError();
            return false;
        }
        if (nulls[0] || nulls[1] || nulls[2] || nulls[3] ||
            errors[0] || errors[1] || errors[2] || errors[3] ||
            categoryLength >= sizeof(category) ||
            statusLength >= sizeof(status)) {
            error = "A room row contains NULL or truncated data.";
            return false;
        }

        const std::string categoryValue(category, categoryLength);
        const std::string statusValue(status, statusLength);
        if (number <= 0 || !validCategory(categoryValue) ||
            !std::isfinite(price) || price <= 0.0 ||
            !validStatus(statusValue) || !roomNumbers.insert(number).second) {
            error = "The database contains an invalid room record.";
            return false;
        }
        loaded.emplace_back(number, categoryValue, price, statusValue);
    }

    rooms.swap(loaded);
    error.clear();
    return true;
}

bool RoomRepository::insertRoom(const Room& room, std::string& error) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    if (room.getRoomNumber() <= 0 || !validCategory(room.getCategory()) ||
        !std::isfinite(room.getPricePerNight()) ||
        room.getPricePerNight() <= 0.0 || !validStatus(room.getStatus())) {
        error = "Room information is invalid.";
        return false;
    }
    if (room.getStatus() != "Available") {
        error = "New rooms must start as Available.";
        return false;
    }

    double normalizedPrice = 0.0;
    if (!hotel_money::normalize(
            room.getPricePerNight(),
            hotel_money::kDecimal10_2MaxCents,
            normalizedPrice
        )) {
        error = "Room price must round to a positive value within DECIMAL(10,2).";
        return false;
    }

    DatabaseStatement statement(
        database.getConnection(),
        "INSERT INTO rooms (room_number, category, price_per_night, status) "
        "VALUES (?, ?, ?, ?)",
        error
    );
    int roomNumber = room.getRoomNumber();
    std::string category = room.getCategory();
    double price = normalizedPrice;
    std::string status = room.getStatus();
    if (!statement.valid() ||
        !statement.bindInt(0, roomNumber, error) ||
        !statement.bindText(1, category, error) ||
        !statement.bindDouble(2, price, error) ||
        !statement.bindText(3, status, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "MariaDB did not insert exactly one room.";
        return false;
    }
    return true;
}

bool RoomRepository::updateStatus(
    int roomNumber,
    const std::string& status,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    if (roomNumber <= 0 || !validStatus(status)) {
        error = "Room status update parameters are invalid.";
        return false;
    }

    DatabaseStatement statement(
        database.getConnection(),
        "UPDATE rooms SET status = ? WHERE room_number = ?",
        error
    );
    std::string statusValue = status;
    if (!statement.valid() ||
        !statement.bindText(0, statusValue, error) ||
        !statement.bindInt(1, roomNumber, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "Room status update did not change exactly one room.";
        return false;
    }
    return true;
}

bool RoomRepository::transitionStatus(
    int roomNumber,
    const std::string& expectedStatus,
    const std::string& newStatus,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || roomNumber <= 0 ||
        !validStatus(expectedStatus) || !validStatus(newStatus)) {
        error = "Room status transition parameters are invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "UPDATE rooms SET status = ? "
        "WHERE room_number = ? AND status = ?",
        error
    );
    std::string target = newStatus;
    int number = roomNumber;
    std::string expected = expectedStatus;
    if (!statement.valid() ||
        !statement.bindText(0, target, error) ||
        !statement.bindInt(1, number, error) ||
        !statement.bindText(2, expected, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "Room status transition failed because its current status changed.";
        return false;
    }
    return true;
}

bool RoomRepository::lockRoomStatus(
    int roomNumber,
    std::string& status,
    bool& found,
    std::string& error
) const {
    error.clear();
    found = false;
    if (!database.isConnected() || roomNumber <= 0) {
        error = "Room lock parameters or connection are invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "SELECT status FROM rooms WHERE room_number = ? FOR UPDATE",
        error
    );
    int requestedRoom = roomNumber;
    if (!statement.valid() ||
        !statement.bindInt(0, requestedRoom, error) ||
        !statement.execute(error) || !statement.storeResult(error)) {
        return false;
    }
    char statusBuffer[32]{};
    unsigned long statusLength = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND result[1]{};
    bindResultText(result[0], statusBuffer, sizeof(statusBuffer),
                   statusLength, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), result) != 0) {
        error = std::string("Could not bind locked-room status: ") +
                statement.statementError();
        return false;
    }
    const int fetchStatus = statement.fetch();
    if (fetchStatus == MYSQL_NO_DATA) {
        error.clear();
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError ||
        statusLength >= sizeof(statusBuffer)) {
        error = std::string("Could not read locked-room status: ") +
                statement.statementError();
        return false;
    }
    status.assign(statusBuffer, statusLength);
    if (!validStatus(status)) {
        error = "The database contains an invalid room status.";
        return false;
    }
    found = true;
    error.clear();
    return true;
}
