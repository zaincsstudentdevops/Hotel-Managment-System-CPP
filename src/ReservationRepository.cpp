#include "ReservationRepository.h"

#include "DatabaseStatement.h"
#include "ReservationValidation.h"

#include <cmath>
#include <cstdlib>
#include <set>

namespace {

const char* kReservationColumns =
    "reservation_id, customer_id, room_number, check_in_date, "
    "check_out_date, number_of_nights, total_cost, status";

bool validReservationStatus(const std::string& status) {
    return status == "Confirmed" || status == "CheckedIn" ||
           status == "CheckedOut" || status == "Cancelled";
}

struct ReservationRow {
    int reservationId = 0;
    int customerId = 0;
    int roomNumber = 0;
    int nights = 0;
    double total = 0.0;
    char checkIn[16]{};
    char checkOut[16]{};
    char status[32]{};
    unsigned long checkInLength = 0;
    unsigned long checkOutLength = 0;
    unsigned long statusLength = 0;
    my_bool nulls[8]{};
    my_bool errors[8]{};
};

void bindReservationRow(ReservationRow& row, MYSQL_BIND results[8]) {
    bindResultInt(results[0], row.reservationId, row.nulls[0], row.errors[0]);
    bindResultInt(results[1], row.customerId, row.nulls[1], row.errors[1]);
    bindResultInt(results[2], row.roomNumber, row.nulls[2], row.errors[2]);
    bindResultText(results[3], row.checkIn, sizeof(row.checkIn),
                   row.checkInLength, row.nulls[3], row.errors[3]);
    bindResultText(results[4], row.checkOut, sizeof(row.checkOut),
                   row.checkOutLength, row.nulls[4], row.errors[4]);
    bindResultInt(results[5], row.nights, row.nulls[5], row.errors[5]);
    bindResultDouble(results[6], row.total, row.nulls[6], row.errors[6]);
    bindResultText(results[7], row.status, sizeof(row.status),
                   row.statusLength, row.nulls[7], row.errors[7]);
}

bool convertReservationRow(
    const ReservationRow& row,
    Reservation& reservation,
    std::string& error
) {
    for (unsigned int index = 0; index < 8; ++index) {
        if (row.nulls[index] || row.errors[index]) {
            error = "A reservation row contains NULL or truncated data.";
            return false;
        }
    }
    if (row.reservationId <= 0 || row.customerId <= 0 ||
        row.roomNumber <= 0 || row.nights <= 0 ||
        row.checkInLength != 10 || row.checkOutLength != 10 ||
        row.statusLength >= sizeof(row.status) ||
        !std::isfinite(row.total) || row.total <= 0.0) {
        error = "The database contains an invalid reservation record.";
        return false;
    }

    const std::string checkIn(row.checkIn, row.checkInLength);
    const std::string checkOut(row.checkOut, row.checkOutLength);
    const std::string status(row.status, row.statusLength);
    if (checkOut <= checkIn || !validReservationStatus(status)) {
        error = "The database contains invalid reservation dates or status.";
        return false;
    }

    reservation = Reservation::fromStoredData(
        row.reservationId, row.customerId, row.roomNumber,
        checkIn, checkOut, row.nights, row.total, status
    );
    return true;
}

bool bindReservationResults(
    DatabaseStatement& statement,
    ReservationRow& row,
    MYSQL_BIND results[8],
    std::string& error
) {
    bindReservationRow(row, results);
    if (mysql_stmt_bind_result(statement.get(), results) != 0) {
        error = std::string("Could not bind reservation results: ") +
                statement.statementError();
        return false;
    }
    return true;
}

}

ReservationRepository::ReservationRepository(Database& database)
    : database(database) {}

bool ReservationRepository::loadReservations(
    std::vector<Reservation>& reservations,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }
    const std::string sql =
        std::string("SELECT ") + kReservationColumns +
        " FROM reservations ORDER BY reservation_id";
    DatabaseStatement statement(database.getConnection(), sql.c_str(), error);
    if (!statement.valid() || !statement.execute(error) ||
        !statement.storeResult(error)) {
        return false;
    }

    ReservationRow row;
    MYSQL_BIND results[8]{};
    if (!bindReservationResults(statement, row, results, error)) {
        return false;
    }

    std::vector<Reservation> loaded;
    std::set<int> ids;
    while (true) {
        const int fetchStatus = statement.fetch();
        if (fetchStatus == MYSQL_NO_DATA) {
            break;
        }
        if (fetchStatus != 0) {
            error = std::string("Could not fetch reservation records: ") +
                    statement.statementError();
            return false;
        }
        Reservation reservation;
        if (!convertReservationRow(row, reservation, error) ||
            !ids.insert(reservation.getReservationId()).second) {
            if (error.empty()) {
                error = "Duplicate reservation ID returned by MariaDB.";
            }
            return false;
        }
        loaded.push_back(reservation);
    }
    reservations.swap(loaded);
    error.clear();
    return true;
}

bool ReservationRepository::findReservation(
    int reservationId,
    Reservation& reservation,
    bool& found,
    std::string& error
) const {
    error.clear();
    found = false;
    if (!database.isConnected() || reservationId <= 0) {
        error = "Reservation lookup parameters or connection are invalid.";
        return false;
    }
    const std::string sql =
        std::string("SELECT ") + kReservationColumns +
        " FROM reservations WHERE reservation_id = ?";
    DatabaseStatement statement(database.getConnection(), sql.c_str(), error);
    int requestedId = reservationId;
    if (!statement.valid() ||
        !statement.bindInt(0, requestedId, error) ||
        !statement.execute(error) || !statement.storeResult(error)) {
        return false;
    }

    ReservationRow row;
    MYSQL_BIND results[8]{};
    if (!bindReservationResults(statement, row, results, error)) {
        return false;
    }
    const int fetchStatus = statement.fetch();
    if (fetchStatus == MYSQL_NO_DATA) {
        error.clear();
        return true;
    }
    if (fetchStatus != 0) {
        error = std::string("Could not fetch reservation: ") +
                statement.statementError();
        return false;
    }
    if (!convertReservationRow(row, reservation, error)) {
        return false;
    }
    found = true;
    error.clear();
    return true;
}

ReservationCreateResult ReservationRepository::createReservation(
    const Reservation& reservation,
    Reservation& storedReservation,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() ||
        reservation.getReservationId() <= 0 ||
        reservation.getCustomerId() <= 0 ||
        reservation.getRoomNumber() <= 0 ||
        reservation.getNumberOfNights() <= 0 ||
        !std::isfinite(reservation.getTotalCost()) ||
        reservation.getTotalCost() <= 0.0 ||
        reservation.getStatus() != "Confirmed") {
        error = "Reservation information is invalid.";
        return ReservationCreateResult::error;
    }
    if (!reservation_validation::validateDatesAndNights(
            reservation, error
        )) {
        return ReservationCreateResult::error;
    }

    int roomNumber = reservation.getRoomNumber();
    {
        DatabaseStatement roomLock(
            database.getConnection(),
            "SELECT room_number, price_per_night FROM rooms "
            "WHERE room_number = ? FOR UPDATE",
            error
        );
        if (!roomLock.valid() || !roomLock.bindInt(0, roomNumber, error) ||
            !roomLock.execute(error) || !roomLock.storeResult(error)) {
            return ReservationCreateResult::error;
        }
        int lockedRoom = 0;
        double lockedPrice = 0.0;
        my_bool roomNull = 0;
        my_bool roomError = 0;
        my_bool priceNull = 0;
        my_bool priceError = 0;
        MYSQL_BIND roomResult[2]{};
        bindResultInt(roomResult[0], lockedRoom, roomNull, roomError);
        bindResultDouble(roomResult[1], lockedPrice, priceNull, priceError);
        if (mysql_stmt_bind_result(roomLock.get(), roomResult) != 0) {
            error = std::string("Could not bind locked-room result: ") +
                    roomLock.statementError();
            return ReservationCreateResult::error;
        }
        const int roomFetch = roomLock.fetch();
        if (roomFetch == MYSQL_NO_DATA) {
            error = "Room does not exist.";
            return ReservationCreateResult::roomNotFound;
        }
        if (roomFetch != 0 || roomNull || roomError || priceNull ||
            priceError || lockedRoom != roomNumber ||
            !std::isfinite(lockedPrice) || lockedPrice <= 0.0) {
            error = std::string("Could not lock the requested room: ") +
                    roomLock.statementError();
            return ReservationCreateResult::error;
        }
        if (!reservation_validation::validateTotalForRoomRate(
                reservation, lockedPrice, error
            )) {
            return ReservationCreateResult::error;
        }
    }

    DatabaseStatement overlap(
        database.getConnection(),
        "SELECT reservation_id FROM reservations "
        "WHERE room_number = ? AND status <> 'Cancelled' "
        "AND check_in_date < ? AND check_out_date > ? LIMIT 1 FOR UPDATE",
        error
    );
    int overlapRoom = reservation.getRoomNumber();
    std::string requestedCheckOut = reservation.getCheckOutDate();
    std::string requestedCheckIn = reservation.getCheckInDate();
    if (!overlap.valid() ||
        !overlap.bindInt(0, overlapRoom, error) ||
        !overlap.bindText(1, requestedCheckOut, error) ||
        !overlap.bindText(2, requestedCheckIn, error) ||
        !overlap.execute(error) || !overlap.storeResult(error)) {
        return ReservationCreateResult::error;
    }
    int conflictingId = 0;
    my_bool conflictNull = 0;
    my_bool conflictError = 0;
    MYSQL_BIND conflictResult[1]{};
    bindResultInt(conflictResult[0], conflictingId, conflictNull, conflictError);
    if (mysql_stmt_bind_result(overlap.get(), conflictResult) != 0) {
        error = std::string("Could not bind reservation overlap result: ") +
                overlap.statementError();
        return ReservationCreateResult::error;
    }
    const int overlapFetch = overlap.fetch();
    if (overlapFetch == 0) {
        error = "Room has an overlapping active reservation.";
        return ReservationCreateResult::overlappingDates;
    }
    if (overlapFetch != MYSQL_NO_DATA) {
        error = std::string("Could not check reservation overlap: ") +
                overlap.statementError();
        return ReservationCreateResult::error;
    }

    DatabaseStatement insert(
        database.getConnection(),
        "INSERT INTO reservations "
        "(reservation_id, customer_id, room_number, check_in_date, "
        "check_out_date, total_cost, status) VALUES (?, ?, ?, ?, ?, ?, ?)",
        error
    );
    int reservationId = reservation.getReservationId();
    int customerId = reservation.getCustomerId();
    int insertRoomNumber = reservation.getRoomNumber();
    std::string checkIn = reservation.getCheckInDate();
    std::string checkOut = reservation.getCheckOutDate();
    double total = reservation.getTotalCost();
    std::string status = "Confirmed";
    if (!insert.valid() ||
        !insert.bindInt(0, reservationId, error) ||
        !insert.bindInt(1, customerId, error) ||
        !insert.bindInt(2, insertRoomNumber, error) ||
        !insert.bindText(3, checkIn, error) ||
        !insert.bindText(4, checkOut, error) ||
        !insert.bindDouble(5, total, error) ||
        !insert.bindText(6, status, error) ||
        !insert.execute(error)) {
        if (insert.valid() && mysql_stmt_errno(insert.get()) == 1062) {
            error = "Reservation ID already exists.";
            return ReservationCreateResult::duplicateId;
        }
        return ReservationCreateResult::error;
    }
    if (insert.affectedRows() != 1) {
        error = "MariaDB did not insert exactly one reservation.";
        return ReservationCreateResult::error;
    }

    bool found = false;
    if (!findReservation(reservationId, storedReservation, found, error) ||
        !found) {
        if (error.empty()) {
            error = "Inserted reservation could not be read back.";
        }
        return ReservationCreateResult::error;
    }
    return ReservationCreateResult::created;
}

bool ReservationRepository::updateStatus(
    int reservationId,
    const std::string& expectedStatus,
    const std::string& newStatus,
    std::string& error
) const {
    error.clear();
    if (!database.isConnected() || reservationId <= 0 ||
        !validReservationStatus(expectedStatus) ||
        !validReservationStatus(newStatus)) {
        error = "Reservation status update parameters are invalid.";
        return false;
    }
    DatabaseStatement statement(
        database.getConnection(),
        "UPDATE reservations SET status = ? "
        "WHERE reservation_id = ? AND status = ?",
        error
    );
    std::string status = newStatus;
    int id = reservationId;
    std::string expected = expectedStatus;
    if (!statement.valid() ||
        !statement.bindText(0, status, error) ||
        !statement.bindInt(1, id, error) ||
        !statement.bindText(2, expected, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() != 1) {
        error = "Reservation status did not change; it may have changed concurrently.";
        return false;
    }
    return true;
}
