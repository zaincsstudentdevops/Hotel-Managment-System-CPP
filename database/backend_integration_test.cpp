#include "ActionHistoryRepository.h"
#include "Database.h"
#include "DatabaseStatement.h"
#include "DatabaseTransaction.h"
#include "HotelBackend.h"
#include "PaymentRepository.h"
#include "RoomRepository.h"
#include "WaitingListRepository.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

bool rowExists(
    MYSQL* connection,
    const char* sql,
    int id,
    bool& exists,
    std::string& error
) {
    DatabaseStatement statement(connection, sql, error);
    int value = id;
    if (!statement.valid() || !statement.bindInt(0, value, error) ||
        !statement.execute(error) || !statement.storeResult(error)) {
        return false;
    }
    int result = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND binding[1]{};
    bindResultInt(binding[0], result, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), binding) != 0) {
        error = std::string("Could not bind existence result: ") +
                statement.statementError();
        return false;
    }
    const int fetchStatus = statement.fetch();
    if (fetchStatus == MYSQL_NO_DATA) {
        exists = false;
        error.clear();
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError) {
        error = std::string("Could not fetch existence result: ") +
                statement.statementError();
        return false;
    }
    exists = true;
    error.clear();
    return true;
}

bool textExists(
    MYSQL* connection,
    const char* sql,
    const std::string& value,
    bool& exists,
    std::string& error
) {
    DatabaseStatement statement(connection, sql, error);
    std::string parameter = value;
    if (!statement.valid() ||
        !statement.bindText(0, parameter, error) ||
        !statement.execute(error) || !statement.storeResult(error)) {
        return false;
    }
    int result = 0;
    my_bool isNull = 0;
    my_bool hasError = 0;
    MYSQL_BIND binding[1]{};
    bindResultInt(binding[0], result, isNull, hasError);
    if (mysql_stmt_bind_result(statement.get(), binding) != 0) {
        error = std::string("Could not bind text-existence result: ") +
                statement.statementError();
        return false;
    }
    const int fetchStatus = statement.fetch();
    if (fetchStatus == MYSQL_NO_DATA) {
        exists = false;
        error.clear();
        return true;
    }
    if (fetchStatus != 0 || isNull || hasError) {
        error = std::string("Could not fetch text-existence result: ") +
                statement.statementError();
        return false;
    }
    exists = true;
    error.clear();
    return true;
}

bool textPatternExists(
    MYSQL* connection,
    const std::string& pattern,
    bool& exists,
    std::string& error
) {
    return textExists(
        connection,
        "SELECT 1 FROM action_history WHERE action_text LIKE ? LIMIT 1",
        pattern,
        exists,
        error
    );
}

bool deleteByText(
    MYSQL* connection,
    const char* sql,
    const std::string& value,
    std::string& error
) {
    DatabaseStatement statement(connection, sql, error);
    std::string parameter = value;
    if (!statement.valid() ||
        !statement.bindText(0, parameter, error) ||
        !statement.execute(error)) {
        return false;
    }
    if (statement.affectedRows() > 1) {
        error = "Cleanup matched more than one row; transaction will roll back.";
        return false;
    }
    return true;
}

bool deleteTemporaryReservation(
    MYSQL* connection,
    int reservationId,
    int customerId,
    int roomNumber,
    std::string& error
) {
    DatabaseStatement statement(
        connection,
        "DELETE FROM reservations WHERE reservation_id = ? "
        "AND customer_id = ? AND room_number = ?",
        error
    );
    int id = reservationId;
    int customer = customerId;
    int room = roomNumber;
    return statement.valid() &&
           statement.bindInt(0, id, error) &&
           statement.bindInt(1, customer, error) &&
           statement.bindInt(2, room, error) &&
           statement.execute(error) &&
           statement.affectedRows() <= 1;
}

bool deleteTemporaryPayment(
    MYSQL* connection,
    int reservationId,
    int customerId,
    int roomNumber,
    std::string& error
) {
    DatabaseStatement statement(
        connection,
        "DELETE FROM payments WHERE reservation_id = ? AND EXISTS "
        "(SELECT 1 FROM reservations WHERE reservation_id = ? "
        "AND customer_id = ? AND room_number = ?)",
        error
    );
    int paymentReservation = reservationId;
    int ownerReservation = reservationId;
    int customer = customerId;
    int room = roomNumber;
    return statement.valid() &&
           statement.bindInt(0, paymentReservation, error) &&
           statement.bindInt(1, ownerReservation, error) &&
           statement.bindInt(2, customer, error) &&
           statement.bindInt(3, room, error) &&
           statement.execute(error) &&
           statement.affectedRows() <= 1;
}

bool cleanupTemporaryRows(
    Database& database,
    int customerId,
    const std::string& email,
    const std::string& identityDocument,
    int roomNumber,
    const std::vector<int>& reservationIds,
    const std::string& waitingNameA,
    const std::string& waitingNameB,
    const std::string& waitingNameTooLong,
    const std::string& waitingNameTruncated,
    const std::vector<std::string>& actionTexts,
    std::string& error
) {
    MYSQL* connection = database.getConnection();
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }

    bool success = true;
    for (int reservationId : reservationIds) {
        if (!deleteTemporaryPayment(
                connection, reservationId, customerId, roomNumber, error
            ) ||
            !deleteTemporaryReservation(
                connection, reservationId, customerId, roomNumber, error
            )) {
            success = false;
            break;
        }
    }

    DatabaseStatement roomDelete(
        connection,
        "DELETE FROM rooms WHERE room_number = ? AND category = ? "
        "AND price_per_night = ?",
        error
    );
    int roomId = roomNumber;
    std::string category = "Double";
    double price = 125.26;
    success = success && roomDelete.valid() &&
        roomDelete.bindInt(0, roomId, error) &&
        roomDelete.bindText(1, category, error) &&
        roomDelete.bindDouble(2, price, error) &&
        roomDelete.execute(error) &&
        roomDelete.affectedRows() <= 1;

    DatabaseStatement customerDelete(
        connection,
        "DELETE FROM customers WHERE customer_id = ? "
        "AND (email = ? OR identity_document = ?)",
        error
    );
    int customer = customerId;
    std::string emailValue = email;
    std::string identityValue = identityDocument;
    success = success && customerDelete.valid() &&
        customerDelete.bindInt(0, customer, error) &&
        customerDelete.bindText(1, emailValue, error) &&
        customerDelete.bindText(2, identityValue, error) &&
        customerDelete.execute(error) &&
        customerDelete.affectedRows() <= 1;

    success = success &&
        deleteByText(connection,
                     "DELETE FROM waiting_list WHERE customer_name = ?",
                     waitingNameA, error) &&
        deleteByText(connection,
                     "DELETE FROM waiting_list WHERE customer_name = ?",
                     waitingNameB, error) &&
        deleteByText(connection,
                     "DELETE FROM waiting_list WHERE customer_name = ?",
                     waitingNameTooLong, error) &&
        deleteByText(connection,
                     "DELETE FROM waiting_list WHERE customer_name = ?",
                     waitingNameTruncated, error);

    for (const std::string& action : actionTexts) {
        if (!success) {
            break;
        }
        success = deleteByText(
            connection,
            "DELETE FROM action_history WHERE action_text = ?",
            action,
            error
        );
    }
    if (!success) {
        std::string rollbackError;
        transaction.rollback(rollbackError);
        if (!rollbackError.empty()) {
            error += " " + rollbackError;
        }
        return false;
    }
    return transaction.commit(error);
}

bool verifyClean(
    MYSQL* connection,
    int customerId,
    int roomNumber,
    int rollbackRoomNumber,
    const std::vector<int>& reservationIds,
    const std::string& waitingNameA,
    const std::string& waitingNameB,
    const std::string& waitingNameTooLong,
    const std::string& waitingNameTruncated,
    const std::vector<std::string>& actionTexts,
    std::string& error
) {
    const struct {
        const char* query;
        int id;
    } checks[] = {
        {"SELECT 1 FROM customers WHERE customer_id = ? LIMIT 1", customerId},
        {"SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1", roomNumber},
        {"SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
         rollbackRoomNumber}
    };
    for (const auto& check : checks) {
        bool exists = false;
        if (!rowExists(connection, check.query, check.id, exists, error) ||
            exists) {
            if (exists) {
                error = "A temporary database ID remains after cleanup.";
            }
            return false;
        }
    }
    for (int reservationId : reservationIds) {
        for (const char* query : {
                 "SELECT 1 FROM reservations WHERE reservation_id = ? LIMIT 1",
                 "SELECT 1 FROM payments WHERE reservation_id = ? LIMIT 1"
             }) {
            bool exists = false;
            if (!rowExists(
                    connection, query, reservationId, exists, error
                ) ||
                exists) {
                if (exists) {
                    error = "A temporary reservation or payment remains after cleanup.";
                }
                return false;
            }
        }
    }
    for (const std::string* name : {
             &waitingNameA, &waitingNameB, &waitingNameTooLong,
             &waitingNameTruncated
         }) {
        bool exists = false;
        if (!textExists(
                connection,
                "SELECT 1 FROM waiting_list WHERE customer_name = ? LIMIT 1",
                *name,
                exists,
                error
            ) ||
            exists) {
            if (exists) {
                error = "A temporary waiting-list entry remains after cleanup.";
            }
            return false;
        }
    }
    for (const std::string& action : actionTexts) {
        bool exists = false;
        if (!textExists(
                connection,
                "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
                action,
                exists,
                error
            ) ||
            exists) {
            if (exists) {
                error = "A temporary action-history entry remains after cleanup.";
            }
            return false;
        }
    }
    return true;
}

void report(bool condition, const char* message, bool& success) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    success = false;
}

bool backendRejectsWithoutWrite(
    HotelBackend& backend,
    ReservationLinkedList& reservations,
    MYSQL* connection,
    const Reservation& reservation,
    const std::string& actionText,
    std::string& error
) {
    std::string rejection;
    if (backend.createReservation(reservation, rejection) !=
            ReservationCreateResult::error ||
        rejection.empty()) {
        error = rejection.empty()
            ? "Invalid reservation was not rejected."
            : rejection;
        return false;
    }

    bool reservationExists = false;
    bool actionExists = false;
    if (reservations.searchById(reservation.getReservationId()) != nullptr ||
        !rowExists(
            connection,
            "SELECT 1 FROM reservations WHERE reservation_id = ? LIMIT 1",
            reservation.getReservationId(),
            reservationExists,
            error
        ) ||
        reservationExists ||
        !textExists(
            connection,
            "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
            actionText,
            actionExists,
            error
        ) ||
        actionExists) {
        if (error.empty()) {
            error = "Rejected reservation left in-memory or database records.";
        }
        return false;
    }
    return true;
}

bool customerRejectedWithoutWrite(
    HotelBackend& backend,
    CustomerLinkedList& customers,
    MYSQL* connection,
    const Customer& customer,
    const std::string& actionText,
    std::string& error
) {
    std::string rejection;
    if (backend.registerCustomer(customer, rejection) ||
        rejection.find("exceeds the database limit") == std::string::npos) {
        error = rejection.empty()
            ? "Oversized customer input was not rejected by field validation."
            : rejection;
        return false;
    }

    bool customerExists = false;
    bool actionExists = false;
    if (customers.searchById(customer.getCustomerId()) != nullptr ||
        !rowExists(
            connection,
            "SELECT 1 FROM customers WHERE customer_id = ? LIMIT 1",
            customer.getCustomerId(),
            customerExists,
            error
        ) ||
        customerExists ||
        !textExists(
            connection,
            "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
            actionText,
            actionExists,
            error
        ) ||
        actionExists) {
        if (error.empty()) {
            error = "Rejected customer input left persistent or in-memory data.";
        }
        return false;
    }
    error.clear();
    return true;
}

bool repositoryRejectsWithRollback(
    Database& database,
    const Reservation& reservation,
    std::string& error
) {
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }

    ReservationRepository repository(database);
    Reservation stored;
    const ReservationCreateResult result =
        repository.createReservation(reservation, stored, error);
    std::string rollbackError;
    const bool rolledBack = transaction.rollback(rollbackError);
    if (!rollbackError.empty()) {
        if (!error.empty()) {
            error += " ";
        }
        error += rollbackError;
    }
    return result == ReservationCreateResult::error && rolledBack;
}

bool rejectsOrphanOccupiedRoom(
    Database& database,
    int roomNumber,
    std::string& error
) {
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }

    DatabaseStatement insert(
        database.getConnection(),
        "INSERT INTO rooms "
        "(room_number, category, price_per_night, status) "
        "VALUES (?, ?, ?, ?)",
        error
    );
    std::string category = "Single";
    double price = 1.0;
    std::string status = "Occupied";
    const bool inserted = insert.valid() &&
        insert.bindInt(0, roomNumber, error) &&
        insert.bindText(1, category, error) &&
        insert.bindDouble(2, price, error) &&
        insert.bindText(3, status, error) &&
        insert.execute(error) &&
        insert.affectedRows() == 1;
    if (!inserted && error.empty()) {
        error = "Could not insert the temporary occupied-room fixture.";
    }

    bool startupRejected = false;
    std::string startupError;
    if (inserted) {
        Hotel hotel;
        CustomerLinkedList customers;
        ReservationLinkedList reservations;
        WaitingQueue waitingList;
        ActionStack actionHistory;
        HotelBackend backend(
            database, hotel, customers, reservations, waitingList,
            actionHistory
        );
        startupRejected = !backend.loadAll(startupError) &&
            startupError.find(std::to_string(roomNumber)) != std::string::npos;
    }

    std::string rollbackError;
    const bool rolledBack = transaction.rollback(rollbackError);
    bool roomStillExists = false;
    const bool checkedAbsent = rowExists(
        database.getConnection(),
        "SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
        roomNumber,
        roomStillExists,
        error
    );
    if (!rollbackError.empty()) {
        error = rollbackError;
    }
    if (!inserted || !startupRejected || !rolledBack ||
        !checkedAbsent || roomStillExists) {
        if (error.empty()) {
            error = startupError.empty()
                ? "Orphan occupied-room validation or rollback failed."
                : startupError;
        }
        return false;
    }
    error.clear();
    return true;
}

bool runIntegrationTest() {
    Database database;
    if (!database.connect()) {
        std::cerr << "FAIL: Database startup connection.\n";
        return false;
    }
    MYSQL* connection = database.getConnection();
    if (connection == nullptr) {
        std::cerr << "FAIL: Database connection handle is unavailable.\n";
        return false;
    }

    Hotel hotel;
    CustomerLinkedList customers;
    ReservationLinkedList reservations;
    WaitingQueue waitingList;
    ActionStack actionHistory;
    HotelBackend backend(
        database, hotel, customers, reservations, waitingList, actionHistory
    );
    std::string error;
    if (!backend.loadAll(error)) {
        std::cerr << "FAIL: Startup loading: " << error << '\n';
        return false;
    }
    std::cout << "PASS: Database startup and persisted-data loading.\n";

    if (!waitingList.isEmpty()) {
        std::cerr << "FAIL: Waiting-list FIFO test requires an initially empty "
                     "queue; no test records were inserted.\n";
        return false;
    }
    if (hotel.getRoomCount() >= 50) {
        std::cerr << "FAIL: Backend integration test requires room-array "
                     "capacity for its temporary room; no test records were "
                     "inserted.\n";
        return false;
    }

    std::random_device randomDevice;
    std::mt19937 generator(
        randomDevice() ^
        static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        )
    );
    std::uniform_int_distribution<int> distribution(1100000000, 1700000000);
    int runId = distribution(generator);
    const int customerId = runId;
    const int roomNumber = runId;
    const int rollbackRoomNumber = runId + 3;
    const int cancelledReservationId = runId;
    const int reservationId = runId + 1;
    const int conflictingReservationId = runId + 2;
    const int invalidDateReservationId = runId + 4;
    const int invalidCalendarReservationId = runId + 5;
    const int invalidNightsReservationId = runId + 6;
    const int invalidTotalReservationId = runId + 7;
    const int repositoryInvalidDateReservationId = runId + 8;
    const int repositoryInvalidNightsReservationId = runId + 9;
    const int repositoryInvalidTotalReservationId = runId + 10;
    const std::vector<int> reservationIds = {
        cancelledReservationId,
        reservationId,
        conflictingReservationId,
        invalidDateReservationId,
        invalidCalendarReservationId,
        invalidNightsReservationId,
        invalidTotalReservationId,
        repositoryInvalidDateReservationId,
        repositoryInvalidNightsReservationId,
        repositoryInvalidTotalReservationId
    };
    const std::string runToken = std::to_string(runId);
    const std::string fullName = "Backend Test " + runToken;
    const std::string email = "backend-" + runToken + "@example.test";
    const std::string identityDocument = "BACKEND-" + runToken;
    const std::string waitingNameA = "Backend FIFO A " + runToken;
    const std::string waitingNameB = "Backend FIFO B " + runToken;
    const std::string waitingNameTooLong =
        "Backend FIFO Oversize " + runToken + std::string(151, 'W');
    const std::string waitingNameTruncated =
        waitingNameTooLong.substr(0, 150);
    const std::string longAction =
        "Backend action-history reload test " + runToken +
        std::string(5000, 'A');
    const std::string oversizedAction =
        "Backend action-history over-limit test " + runToken +
        std::string(65536, 'B');
    const std::string truncatedOversizedAction =
        oversizedAction.substr(0, 65535);
    const std::vector<Customer> oversizedCustomers = {
        Customer(customerId, std::string(151, 'N'), "555-" + runToken,
                 email, identityDocument),
        Customer(customerId, fullName, std::string(31, 'P'),
                 email, identityDocument),
        Customer(customerId, fullName, "555-" + runToken,
                 std::string(255, 'E'), identityDocument),
        Customer(customerId, fullName, "555-" + runToken,
                 email, std::string(51, 'I'))
    };
    std::vector<std::string> oversizedCustomerActions;
    for (const Customer& customer : oversizedCustomers) {
        oversizedCustomerActions.push_back(
            "Customer " + customer.getFullName() + " registered"
        );
    }
    const std::string roomAction =
        "Room " + std::to_string(roomNumber) + " added";
    const std::string customerAction =
        "Customer " + fullName + " registered";
    const std::string customerUpdateAction =
        "Customer " + std::to_string(customerId) + " updated";
    const std::string cancelledCreateAction =
        "Reservation " + std::to_string(cancelledReservationId) + " created";
    const std::string cancelAction =
        "Reservation " + std::to_string(cancelledReservationId) + " cancelled";
    const std::string reservationCreateAction =
        "Reservation " + std::to_string(reservationId) + " created";
    const std::string invalidDateAction =
        "Reservation " + std::to_string(invalidDateReservationId) + " created";
    const std::string invalidCalendarAction =
        "Reservation " + std::to_string(invalidCalendarReservationId) +
        " created";
    const std::string invalidNightsAction =
        "Reservation " + std::to_string(invalidNightsReservationId) + " created";
    const std::string invalidTotalAction =
        "Reservation " + std::to_string(invalidTotalReservationId) + " created";
    const std::string checkInAction =
        "Reservation " + std::to_string(reservationId) + " checked in";
    const std::string checkOutAction =
        "Reservation " + std::to_string(reservationId) + " checked out";
    const std::string waitingActionA = waitingNameA + " added to waiting queue";
    const std::string waitingActionB = waitingNameB + " added to waiting queue";
    const std::string notificationAction =
        "Waiting customer " + waitingNameA +
        " notified about available room " + std::to_string(roomNumber);

    bool identifiersUnused = true;
    const struct {
        const char* query;
        int id;
    } idChecks[] = {
        {"SELECT 1 FROM customers WHERE customer_id = ? LIMIT 1", customerId},
        {"SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1", roomNumber},
        {"SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
         rollbackRoomNumber}
    };
    for (const auto& check : idChecks) {
        bool exists = false;
        if (!rowExists(connection, check.query, check.id, exists, error) ||
            exists) {
            identifiersUnused = false;
            break;
        }
    }
    for (int id : reservationIds) {
        for (const char* query : {
                 "SELECT 1 FROM reservations WHERE reservation_id = ? LIMIT 1",
                 "SELECT 1 FROM payments WHERE reservation_id = ? LIMIT 1"
             }) {
            bool exists = false;
            if (!identifiersUnused ||
                !rowExists(connection, query, id, exists, error) ||
                exists) {
                identifiersUnused = false;
                break;
            }
        }
        if (!identifiersUnused) {
            break;
        }
    }
    for (const std::string* name : {
             &waitingNameA, &waitingNameB, &waitingNameTooLong,
             &waitingNameTruncated
         }) {
        bool exists = false;
        if (!identifiersUnused ||
            !textExists(
                connection,
                "SELECT 1 FROM waiting_list WHERE customer_name = ? LIMIT 1",
                *name,
                exists,
                error
            ) ||
            exists) {
            identifiersUnused = false;
            break;
        }
    }
    std::vector<std::string> fixedActionTexts = {
        roomAction,
        customerAction,
        customerUpdateAction,
        cancelledCreateAction,
        cancelAction,
        reservationCreateAction,
        invalidDateAction,
        invalidCalendarAction,
        invalidNightsAction,
        invalidTotalAction,
        checkInAction,
        checkOutAction,
        waitingActionA,
        waitingActionB,
        notificationAction,
        waitingNameTooLong + " added to waiting queue",
        longAction,
        oversizedAction,
        truncatedOversizedAction
    };
    fixedActionTexts.insert(
        fixedActionTexts.end(),
        oversizedCustomerActions.begin(),
        oversizedCustomerActions.end()
    );
    for (const std::string& action : fixedActionTexts) {
        bool exists = false;
        if (!identifiersUnused ||
            !textExists(
                connection,
                "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
                action,
                exists,
                error
            ) ||
            exists) {
            identifiersUnused = false;
            break;
        }
    }
    const std::string paymentActionPattern =
        "% recorded for reservation " + std::to_string(reservationId);
    bool priorPaymentAction = false;
    if (identifiersUnused &&
        (!textPatternExists(
             connection, paymentActionPattern, priorPaymentAction, error
         ) ||
         priorPaymentAction)) {
        identifiersUnused = false;
    }
    if (!identifiersUnused) {
        std::cerr << "FAIL: Temporary identifiers were not confirmed unused: "
                  << error << '\n';
        return false;
    }
    std::cout << "PASS: All temporary customer, room, reservation, payment, "
                 "and waiting-list identifiers are unused.\n";

    bool success = true;
    std::vector<std::string> actionTexts = fixedActionTexts;

    report(
        rejectsOrphanOccupiedRoom(database, rollbackRoomNumber, error),
        "Startup rejects an occupied room without a checked-in reservation "
        "and leaves the database unchanged.",
        success
    );

    ActionHistoryRepository actionHistoryRepository(database);
    if (success) {
        DatabaseTransaction transaction(database);
        unsigned long long actionId = 0;
        const bool began = transaction.begin(error);
        const bool recorded = began &&
            actionHistoryRepository.recordAction(longAction, actionId, error);
        const bool committed = recorded && transaction.commit(error);
        if (transaction.isActive()) {
            std::string rollbackError;
            transaction.rollback(rollbackError);
            if (!rollbackError.empty()) {
                error += " " + rollbackError;
            }
        }

        std::vector<std::string> loadedActions;
        const bool loaded = committed &&
            actionHistoryRepository.loadActions(loadedActions, error);
        const bool found = loaded &&
            std::find(loadedActions.begin(), loadedActions.end(), longAction) !=
                loadedActions.end();
        report(found,
               "Action-history text longer than 4 KB survives repository reload.",
               success);
    }
    if (success) {
        unsigned long long actionId = 0;
        std::string rejection;
        const bool recorded = actionHistoryRepository.recordAction(
            oversizedAction, actionId, rejection
        );
        bool oversizedExists = false;
        bool truncatedExists = false;
        const bool absent =
            textExists(
                connection,
                "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
                oversizedAction,
                oversizedExists,
                error
            ) &&
            !oversizedExists &&
            textExists(
                connection,
                "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
                truncatedOversizedAction,
                truncatedExists,
                error
            ) &&
            !truncatedExists;
        report(!recorded && rejection == "Action-history entry is invalid." &&
                   absent,
               "Action-history writes beyond the MariaDB TEXT byte limit are "
               "rejected without truncation.",
               success);
    }

    RoomRepository roomRepository(database);
    bool nonAvailableRoomsRejected = true;
    for (const std::string& status : {std::string("Occupied"),
                                      std::string("Maintenance")}) {
        Room rejectedRoom(rollbackRoomNumber, "Single", 10.0, status);
        std::string validationError;
        nonAvailableRoomsRejected =
            nonAvailableRoomsRejected &&
            !roomRepository.insertRoom(rejectedRoom, validationError) &&
            !backend.addRoom(rejectedRoom, validationError);
    }
    bool rejectedRoomExists = false;
    const bool rejectedRoomAbsent = rowExists(
        connection,
        "SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
        rollbackRoomNumber,
        rejectedRoomExists,
        error
    ) && !rejectedRoomExists;
    report(nonAvailableRoomsRejected && rejectedRoomAbsent,
           "Repository and service reject non-Available new rooms.", success);

    {
        DatabaseTransaction transaction(database);
        Room temporaryRollbackRoom(
            rollbackRoomNumber, "Single", 1.0, "Available"
        );
        bool rollbackWorked = transaction.begin(error) &&
            roomRepository.insertRoom(temporaryRollbackRoom, error) &&
            transaction.rollback(error);
        if (!rollbackWorked && transaction.isActive()) {
            std::string rollbackError;
            transaction.rollback(rollbackError);
        }
        bool exists = false;
        const bool absent = rowExists(
            connection,
            "SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
            rollbackRoomNumber,
            exists,
            error
        ) && !exists;
        report(rollbackWorked && absent,
               "Transaction rollback leaves no temporary room.", success);
    }

    Customer testCustomer(
        customerId,
        fullName,
        "555-" + runToken,
        email,
        identityDocument
    );
    bool oversizedCustomersRejected = true;
    for (const Customer& oversizedCustomer : oversizedCustomers) {
        const std::string action =
            "Customer " + oversizedCustomer.getFullName() + " registered";
        if (!customerRejectedWithoutWrite(
                backend, customers, connection, oversizedCustomer,
                action, error
            )) {
            oversizedCustomersRejected = false;
            break;
        }
    }
    report(oversizedCustomersRejected,
           "Customer field limits reject oversized registration without writes.",
           success);

    if (success) {
        std::string rejection;
        const bool enqueued =
            backend.enqueueWaitingCustomer(waitingNameTooLong, rejection);
        bool entryExists = false;
        bool actionExists = false;
        const bool absent =
            textExists(
                connection,
                "SELECT 1 FROM waiting_list WHERE customer_name = ? LIMIT 1",
                waitingNameTooLong,
                entryExists,
                error
            ) &&
            !entryExists &&
            textExists(
                connection,
                "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
                waitingNameTooLong + " added to waiting queue",
                actionExists,
                error
            ) &&
            !actionExists;
        report(!enqueued &&
                   rejection.find("exceeds the database limit") !=
                       std::string::npos &&
                   waitingList.isEmpty() && absent,
               "Waiting-list length limit rejects input without writes.",
               success);
    }

    Room testRoom(roomNumber, "Double", 125.256, "Available");
    const bool roomCreated = success && backend.addRoom(testRoom, error);
    const Room* createdRoom = hotel.findRoom(roomNumber);
    report(roomCreated && createdRoom != nullptr &&
               std::fabs(createdRoom->getPricePerNight() - 125.26) < 0.000001,
           "Room persistence normalizes price to cents in memory.", success);
    const bool customerCreated = success &&
        backend.registerCustomer(testCustomer, error);
    report(customerCreated,
           "Customer repository compatibility and persistence.", success);
    if (success) {
        Customer oversizedUpdate(
            customerId,
            std::string(151, 'U'),
            testCustomer.getPhoneNumber(),
            testCustomer.getEmail(),
            testCustomer.getIdentityDocument()
        );
        std::string rejection;
        const CustomerUpdateStatus updateStatus =
            backend.updateCustomer(oversizedUpdate, rejection);
        Customer reloadedCustomer;
        const CustomerLookupStatus lookupStatus =
            backend.findCustomerById(customerId, reloadedCustomer, error);
        bool updateActionExists = false;
        const bool updateActionAbsent = textExists(
            connection,
            "SELECT 1 FROM action_history WHERE action_text = ? LIMIT 1",
            customerUpdateAction,
            updateActionExists,
            error
        ) && !updateActionExists;
        const Customer* inMemoryCustomer = customers.searchById(customerId);
        report(updateStatus == CustomerUpdateStatus::error &&
                   rejection.find("exceeds the database limit") !=
                       std::string::npos &&
                   lookupStatus == CustomerLookupStatus::found &&
                   reloadedCustomer.getFullName() == fullName &&
                   inMemoryCustomer != nullptr &&
                   inMemoryCustomer->getFullName() == fullName &&
                   updateActionAbsent,
               "Oversized customer update leaves database and memory unchanged.",
               success);
    }
    if (success) {
        Customer updatedCustomer(
            customerId,
            fullName,
            "556-" + runToken,
            email,
            identityDocument
        );
        const CustomerUpdateStatus updateStatus =
            backend.updateCustomer(updatedCustomer, error);
        Customer reloadedCustomer;
        const CustomerLookupStatus lookupStatus =
            backend.findCustomerById(customerId, reloadedCustomer, error);
        report(updateStatus == CustomerUpdateStatus::updated &&
                   lookupStatus == CustomerLookupStatus::found &&
                   reloadedCustomer.getPhoneNumber() ==
                       updatedCustomer.getPhoneNumber(),
               "Transactional customer update and retrieval.", success);
        if (success) {
            testCustomer = updatedCustomer;
        }
    }

    if (success) {
        const Reservation invalidDate = Reservation::fromStoredData(
            invalidDateReservationId, customerId, roomNumber,
            "2099-12-12", "2099-12-10", 2, 250.52, "Confirmed"
        );
        report(
            backendRejectsWithoutWrite(
                backend, reservations, connection, invalidDate,
                invalidDateAction, error
            ),
            "Backend rejects reversed reservation dates without writing.",
            success
        );
    }
    if (success) {
        const Reservation invalidCalendar = Reservation::fromStoredData(
            invalidCalendarReservationId, customerId, roomNumber,
            "2099-02-29", "2099-03-02", 1, 125.26, "Confirmed"
        );
        report(
            backendRejectsWithoutWrite(
                backend, reservations, connection, invalidCalendar,
                invalidCalendarAction, error
            ),
            "Backend rejects invalid calendar dates without writing.",
            success
        );
    }
    if (success) {
        const Reservation invalidNights(
            invalidNightsReservationId, customerId, roomNumber,
            "2099-12-10", "2099-12-12", 3, 125.26
        );
        report(
            backendRejectsWithoutWrite(
                backend, reservations, connection, invalidNights,
                invalidNightsAction, error
            ),
            "Backend rejects a night count that differs from the date range.",
            success
        );
    }
    if (success) {
        const Reservation invalidTotal = Reservation::fromStoredData(
            invalidTotalReservationId, customerId, roomNumber,
            "2099-12-10", "2099-12-12", 2, 250.53, "Confirmed"
        );
        report(
            backendRejectsWithoutWrite(
                backend, reservations, connection, invalidTotal,
                invalidTotalAction, error
            ),
            "Backend rejects a total that differs from the normalized room rate.",
            success
        );
    }

    if (success) {
        const Reservation repositoryInvalidDate = Reservation::fromStoredData(
            repositoryInvalidDateReservationId, customerId, roomNumber,
            "2099-12-12", "2099-12-10", 2, 250.52, "Confirmed"
        );
        report(
            repositoryRejectsWithRollback(
                database, repositoryInvalidDate, error
            ),
            "Repository independently rejects invalid date order.",
            success
        );
    }
    if (success) {
        const Reservation repositoryInvalidNights =
            Reservation::fromStoredData(
                repositoryInvalidNightsReservationId, customerId, roomNumber,
                "2099-12-10", "2099-12-12", 1, 125.26, "Confirmed"
            );
        report(
            repositoryRejectsWithRollback(
                database, repositoryInvalidNights, error
            ),
            "Repository independently rejects mismatched nights.",
            success
        );
    }
    if (success) {
        const Reservation repositoryInvalidTotal = Reservation::fromStoredData(
            repositoryInvalidTotalReservationId, customerId, roomNumber,
            "2099-12-10", "2099-12-12", 2, 250.53, "Confirmed"
        );
        report(
            repositoryRejectsWithRollback(
                database, repositoryInvalidTotal, error
            ),
            "Repository independently rejects a mismatched total.",
            success
        );
    }

    const Reservation cancelledReservation = Reservation::fromStoredData(
        cancelledReservationId, customerId, roomNumber,
        "2099-12-10", "2099-12-12", 2, 250.524, "Confirmed"
    );
    ReservationCreateResult createResult = ReservationCreateResult::error;
    if (success) {
        createResult = backend.createReservation(cancelledReservation, error);
        const Reservation* stored =
            reservations.searchById(cancelledReservationId);
        report(createResult == ReservationCreateResult::created &&
                   stored != nullptr &&
                   stored->getNumberOfNights() == 2 &&
                   std::fabs(stored->getTotalCost() - 250.52) < 0.000001,
               "Valid reservation persists its date-derived nights and "
               "normalized total.", success);
    }

    if (success) {
        DatabaseTransaction transaction(database);
        ReservationRepository repository(database);
        Reservation unusedReservation(
            conflictingReservationId, customerId, roomNumber,
            "2099-12-11", "2099-12-13", 2, 125.26
        );
        Reservation stored;
        const bool began = transaction.begin(error);
        const ReservationCreateResult conflict = began
            ? repository.createReservation(unusedReservation, stored, error)
            : ReservationCreateResult::error;
        std::string rollbackError;
        const bool rolledBack =
            !transaction.isActive() || transaction.rollback(rollbackError);
        if (!rollbackError.empty()) {
            error += " " + rollbackError;
        }
        report(began && rolledBack &&
                   conflict == ReservationCreateResult::overlappingDates,
               "Database overlap rejection while holding the room lock.",
               success);
    }
    if (success) {
        const bool cancelled =
            backend.cancelReservation(cancelledReservationId, error);
        report(cancelled, "Reservation cancellation.", success);
    }

    if (success) {
        createResult = backend.createReservation(
            Reservation(
                reservationId, customerId, roomNumber,
                "2099-12-10", "2099-12-12", 2, 125.26
            ),
            error
        );
        report(createResult == ReservationCreateResult::created,
               "Cancelled dates can be reserved again.", success);
    }

    Payment payment;
    if (success) {
        DatabaseTransaction transaction(database);
        PaymentRepository repository(database);
        Payment mismatched;
        const bool began = transaction.begin(error);
        const bool inserted = began && repository.insertPayment(
            reservationId, 250.51, "Cash", "2099-12-10", mismatched, error
        );
        std::string rollbackError;
        const bool rolledBack =
            !transaction.isActive() || transaction.rollback(rollbackError);
        if (!rollbackError.empty()) {
            error += " " + rollbackError;
        }
        bool paymentExists = false;
        const bool absent = rowExists(
            connection,
            "SELECT 1 FROM payments WHERE reservation_id = ? LIMIT 1",
            reservationId,
            paymentExists,
            error
        ) && !paymentExists;
        report(began && rolledBack && !inserted && absent,
               "Repository rejects a payment amount that differs from the "
               "database reservation total.",
               success);
    }
    if (success) {
        const bool paid = backend.recordPayment(
            reservationId, 250.524, "Cash", "2099-12-10", payment, error
        );
        report(paid && payment.getPaymentId() > 0 &&
                   std::fabs(payment.getAmount() - 250.52) < 0.000001,
               "Payment creation normalizes cents and uses a generated ID.",
               success);
        if (paid) {
            actionTexts.push_back(
                "Payment " + std::to_string(payment.getPaymentId()) +
                " recorded for reservation " + std::to_string(reservationId)
            );
        }
    }
    if (success) {
        DatabaseTransaction transaction(database);
        PaymentRepository repository(database);
        Payment duplicate;
        const bool began = transaction.begin(error);
        const bool inserted = began && repository.insertPayment(
            reservationId, 250.52, "Cash", "2099-12-10", duplicate, error
        );
        std::string rollbackError;
        const bool rolledBack =
            !transaction.isActive() || transaction.rollback(rollbackError);
        if (!rollbackError.empty()) {
            error += " " + rollbackError;
        }
        report(began && rolledBack && !inserted,
               "Database uniqueness rejects a second payment.", success);
    }

    if (success) {
        const bool checkedIn = backend.checkIn(reservationId, error);
        report(checkedIn, "Transactional check-in updates reservation and room.",
               success);
    }
    if (success) {
        const bool queuedA = backend.enqueueWaitingCustomer(waitingNameA, error);
        report(queuedA, "Waiting-list first entry persistence.", success);
    }
    if (success) {
        const bool queuedB = backend.enqueueWaitingCustomer(waitingNameB, error);
        report(queuedB, "Waiting-list second entry persistence.", success);
    }

    std::string notifiedCustomer;
    if (success) {
        const bool checkedOut =
            backend.checkOut(reservationId, notifiedCustomer, error);
        report(checkedOut && notifiedCustomer == waitingNameA &&
                   waitingList.size() == 1,
               "Transactional checkout and FIFO notification.", success);
    }

    if (success) {
        Hotel restartedHotel;
        CustomerLinkedList restartedCustomers;
        ReservationLinkedList restartedReservations;
        WaitingQueue restartedWaitingList;
        ActionStack restartedActionHistory;
        HotelBackend restartedBackend(
            database, restartedHotel, restartedCustomers,
            restartedReservations, restartedWaitingList,
            restartedActionHistory
        );
        const bool loaded = restartedBackend.loadAll(error);
        const Customer* reloadedCustomer =
            restartedCustomers.searchById(customerId);
        const Room* reloadedRoom = restartedHotel.findRoom(roomNumber);
        Reservation* reloadedCancelled =
            restartedReservations.searchById(cancelledReservationId);
        Reservation* reloadedReservation =
            restartedReservations.searchById(reservationId);
        std::string frontAction;
        std::string frontWaitingName;
        const bool restartVerified =
            loaded && reloadedCustomer != nullptr &&
            reloadedCustomer->getPhoneNumber() == testCustomer.getPhoneNumber() &&
            reloadedRoom != nullptr &&
            reloadedRoom->getStatus() == "Available" &&
            std::fabs(reloadedRoom->getPricePerNight() - 125.26) < 0.000001 &&
            reloadedCancelled != nullptr &&
            reloadedCancelled->getStatus() == "Cancelled" &&
            reloadedReservation != nullptr &&
            reloadedReservation->getStatus() == "CheckedOut" &&
            reloadedReservation->hasPayment() &&
            std::fabs(
                reloadedReservation->getPayment().getAmount() - 250.52
            ) < 0.000001 &&
            std::fabs(reloadedReservation->getTotalCost() - 250.52) < 0.001 &&
            restartedWaitingList.peek(frontWaitingName) &&
            frontWaitingName == waitingNameB &&
            restartedActionHistory.peek(frontAction) &&
            frontAction ==
                "Waiting customer " + waitingNameA +
                " notified about available room " + std::to_string(roomNumber);
        bool longActionReloaded = false;
        while (!restartedActionHistory.isEmpty()) {
            if (restartedActionHistory.pop() == longAction) {
                longActionReloaded = true;
                break;
            }
        }
        report(restartVerified && longActionReloaded,
               "Application restart reloads room, customer, reservation, "
               "payment, queue, and action history including long entries.",
               success);
    }

    if (!actionTexts.empty() || roomCreated || customerCreated) {
        std::string cleanupError;
        const bool cleaned = cleanupTemporaryRows(
            database,
            customerId,
            email,
            identityDocument,
            roomNumber,
            reservationIds,
            waitingNameA,
            waitingNameB,
            waitingNameTooLong,
            waitingNameTruncated,
            actionTexts,
            cleanupError
        );
        const bool verified = cleaned && verifyClean(
            connection,
            customerId,
            roomNumber,
            rollbackRoomNumber,
            reservationIds,
            waitingNameA,
            waitingNameB,
            waitingNameTooLong,
            waitingNameTruncated,
            actionTexts,
            cleanupError
        );
        report(verified,
               "Only temporary test rows were cleaned up and all were verified absent.",
               success);
        if (!verified) {
            std::cerr << "Cleanup detail: " << cleanupError << '\n';
        }
    } else {
        bool exists = false;
        const bool absent = rowExists(
            connection,
            "SELECT 1 FROM rooms WHERE room_number = ? LIMIT 1",
            rollbackRoomNumber,
            exists,
            error
        ) && !exists;
        report(absent, "Rolled-back test room remains absent.", success);
    }

    if (success) {
        std::cout << "Backend integration test completed successfully.\n";
    }
    return success;
}

}

int main() {
    return runIntegrationTest() ? 0 : 1;
}
