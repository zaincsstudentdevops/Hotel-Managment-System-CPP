#include "HotelBackend.h"

#include "DatabaseTransaction.h"
#include "Money.h"
#include "ReservationValidation.h"

#include <cmath>
#include <set>
#include <vector>

namespace {

bool rollbackWithError(DatabaseTransaction& transaction, std::string& error) {
    std::string rollbackError;
    if (!transaction.rollback(rollbackError)) {
        if (!error.empty()) {
            error += " ";
        }
        error += rollbackError;
        return false;
    }
    return true;
}

bool validPaymentDate(const std::string& date) {
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

HotelBackend::HotelBackend(
    Database& database,
    Hotel& hotel,
    CustomerLinkedList& customers,
    ReservationLinkedList& reservations,
    WaitingQueue& waitingList,
    ActionStack& actionHistory
) : database(database),
    hotel(hotel),
    customers(customers),
    reservations(reservations),
    waitingList(waitingList),
    actionHistory(actionHistory),
    customerRepository(database, customers),
    roomRepository(database),
    reservationRepository(database),
    paymentRepository(database),
    waitingListRepository(database),
    actionHistoryRepository(database) {}

bool HotelBackend::loadAll(std::string& error) {
    error.clear();
    if (!database.isConnected()) {
        error = "MariaDB connection is not available.";
        return false;
    }

    if (!customerRepository.loadCustomers(error)) {
        error = "Could not load customers: " + error;
        return false;
    }

    std::vector<Room> loadedRooms;
    if (!roomRepository.loadRooms(loadedRooms, error)) {
        error = "Could not load rooms: " + error;
        return false;
    }
    if (loadedRooms.size() > 50) {
        error = "Database startup aborted: the Hotel array supports at most 50 rooms.";
        return false;
    }
    if (!hotel.loadRooms(loadedRooms, error)) {
        error = "Could not install loaded rooms: " + error;
        return false;
    }

    std::vector<Reservation> loadedReservations;
    if (!reservationRepository.loadReservations(loadedReservations, error)) {
        error = "Could not load reservations: " + error;
        return false;
    }

    std::set<int> checkedInRooms;
    for (const Reservation& reservation : loadedReservations) {
        if (customers.searchById(reservation.getCustomerId()) == nullptr) {
            error = "Reservation " +
                    std::to_string(reservation.getReservationId()) +
                    " references a customer that was not loaded.";
            return false;
        }
        const Room* room = hotel.findRoom(reservation.getRoomNumber());
        if (room == nullptr) {
            error = "Reservation " +
                    std::to_string(reservation.getReservationId()) +
                    " references a room that was not loaded.";
            return false;
        }
        if (reservation.getStatus() == "CheckedIn") {
            if (room->getStatus() != "Occupied" ||
                !checkedInRooms.insert(reservation.getRoomNumber()).second) {
                error = "A checked-in reservation has an inconsistent room status.";
                return false;
            }
        }
        for (const Reservation& earlier : loadedReservations) {
            if (earlier.getReservationId() >= reservation.getReservationId()) {
                break;
            }
            if (earlier.getRoomNumber() == reservation.getRoomNumber() &&
                earlier.overlapsWith(
                    reservation.getCheckInDate(),
                    reservation.getCheckOutDate()
                ) &&
                reservation.getStatus() != "Cancelled") {
                error = "The database contains overlapping active reservations.";
                return false;
            }
        }
    }
    for (const Room& room : loadedRooms) {
        if (room.getStatus() == "Occupied" &&
            checkedInRooms.find(room.getRoomNumber()) == checkedInRooms.end()) {
            error = "Occupied room " + std::to_string(room.getRoomNumber()) +
                    " has no checked-in reservation.";
            return false;
        }
    }
    for (const Reservation& reservation : loadedReservations) {
        if (!reservations.insertReservation(reservation)) {
            error = "Could not add a reservation to the in-memory list.";
            return false;
        }
    }

    std::vector<Payment> loadedPayments;
    if (!paymentRepository.loadPayments(loadedPayments, error)) {
        error = "Could not load payments: " + error;
        return false;
    }
    for (const Payment& payment : loadedPayments) {
        Reservation* reservation =
            reservations.searchById(payment.getReservationId());
        if (reservation == nullptr || !reservation->recordPayment(payment)) {
            error = "A payment is orphaned, duplicated, or does not match its "
                    "reservation total.";
            return false;
        }
    }

    std::vector<std::string> loadedWaitingNames;
    if (!waitingListRepository.loadEntries(loadedWaitingNames, error)) {
        error = "Could not load waiting-list entries: " + error;
        return false;
    }
    for (const std::string& name : loadedWaitingNames) {
        waitingList.enqueue(name);
    }

    std::vector<std::string> loadedActions;
    if (!actionHistoryRepository.loadActions(loadedActions, error)) {
        error = "Could not load action history: " + error;
        return false;
    }
    for (const std::string& action : loadedActions) {
        actionHistory.push(action);
    }

    error.clear();
    return true;
}

std::vector<Customer> HotelBackend::getCustomersSnapshot() const {
    return customers.getAllCustomers();
}

std::vector<Room> HotelBackend::getRoomsSnapshot() const {
    return hotel.getAllRooms();
}

std::vector<Reservation> HotelBackend::getReservationsSnapshot() const {
    return reservations.getAllReservations();
}

std::vector<Payment> HotelBackend::getPaymentsSnapshot() const {
    std::vector<Payment> payments;
    const std::vector<Reservation> allReservations =
        reservations.getAllReservations();
    for (const Reservation& reservation : allReservations) {
        if (reservation.hasPayment()) {
            payments.push_back(reservation.getPayment());
        }
    }
    return payments;
}

bool HotelBackend::getWaitingListSnapshot(
    std::vector<WaitingListEntrySnapshot>& entries,
    std::string& error
) const {
    return waitingListRepository.loadEntrySnapshots(entries, error);
}

DashboardSnapshot HotelBackend::getDashboardSnapshot() const {
    DashboardSnapshot snapshot;
    const std::vector<Customer> allCustomers =
        customers.getAllCustomers();
    const std::vector<Room> allRooms = hotel.getAllRooms();
    const std::vector<Reservation> allReservations =
        reservations.getAllReservations();
    const std::vector<Payment> allPayments = getPaymentsSnapshot();

    snapshot.totalCustomers = allCustomers.size();
    snapshot.totalRooms = allRooms.size();
    snapshot.totalReservations = allReservations.size();
    snapshot.totalPayments = allPayments.size();
    snapshot.waitingListSize = waitingList.size();

    long double totalPaymentsAmount = 0.0L;
    for (const Room& room : allRooms) {
        if (room.getStatus() == "Available") {
            ++snapshot.availableRooms;
        } else if (room.getStatus() == "Occupied") {
            ++snapshot.occupiedRooms;
        } else if (room.getStatus() == "Maintenance") {
            ++snapshot.maintenanceRooms;
        }
    }
    for (const Reservation& reservation : allReservations) {
        if (reservation.getStatus() == "Confirmed") {
            ++snapshot.confirmedReservations;
        } else if (reservation.getStatus() == "CheckedIn") {
            ++snapshot.checkedInReservations;
        } else if (reservation.getStatus() == "CheckedOut") {
            ++snapshot.checkedOutReservations;
        } else if (reservation.getStatus() == "Cancelled") {
            ++snapshot.cancelledReservations;
        }
    }
    for (const Payment& payment : allPayments) {
        totalPaymentsAmount += payment.getAmount();
    }
    snapshot.totalPaymentsAmount =
        static_cast<double>(totalPaymentsAmount);
    return snapshot;
}

bool HotelBackend::addRoom(const Room& room, std::string& error) {
    error.clear();
    if (room.getStatus() != "Available") {
        error = "New rooms must start as Available.";
        return false;
    }
    if (hotel.getRoomCount() >= 50) {
        error = "Hotel is full. Cannot add more rooms.";
        return false;
    }
    if (hotel.findRoom(room.getRoomNumber()) != nullptr) {
        error = "Room number already exists.";
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
    const Room normalizedRoom(
        room.getRoomNumber(),
        room.getCategory(),
        normalizedPrice,
        room.getStatus()
    );

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    if (!roomRepository.insertRoom(normalizedRoom, error)) {
        rollbackWithError(transaction, error);
        return false;
    }

    unsigned long long actionId = 0;
    const std::string action =
        "Room " + std::to_string(room.getRoomNumber()) + " added";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!hotel.addRoom(normalizedRoom)) {
        error = "Room was committed but could not be added to the in-memory Hotel.";
        return false;
    }
    actionHistory.push(action);
    return true;
}

bool HotelBackend::registerCustomer(
    const Customer& customer,
    std::string& error
) {
    error.clear();
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    if (!customerRepository.registerCustomer(customer, error, false)) {
        rollbackWithError(transaction, error);
        return false;
    }

    unsigned long long actionId = 0;
    const std::string action =
        "Customer " + customer.getFullName() + " registered";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!customers.insertCustomer(customer)) {
        error = "Customer was committed but could not be added to the "
                "in-memory list.";
        return false;
    }
    actionHistory.push(action);
    return true;
}

CustomerLookupStatus HotelBackend::findCustomerById(
    int customerId,
    Customer& customer,
    std::string& error
) {
    error.clear();
    return customerRepository.findCustomerById(customerId, customer, error);
}

CustomerUpdateStatus HotelBackend::updateCustomer(
    const Customer& customer,
    std::string& error
) {
    error.clear();
    if (customers.searchById(customer.getCustomerId()) == nullptr) {
        error = "Customer ID was not found in the in-memory list.";
        return CustomerUpdateStatus::notFound;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return CustomerUpdateStatus::error;
    }
    const CustomerUpdateStatus status =
        customerRepository.updateCustomer(customer, error, false);
    if (status != CustomerUpdateStatus::updated) {
        rollbackWithError(transaction, error);
        return status;
    }

    unsigned long long actionId = 0;
    const std::string action =
        "Customer " + std::to_string(customer.getCustomerId()) + " updated";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return CustomerUpdateStatus::error;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return CustomerUpdateStatus::error;
    }
    if (!customerRepository.synchronizeCustomer(customer, error)) {
        error = "Customer update committed but the in-memory list could not "
                "be synchronized: " + error;
        return CustomerUpdateStatus::error;
    }
    actionHistory.push(action);
    return CustomerUpdateStatus::updated;
}

CustomerDeleteStatus HotelBackend::deleteCustomer(
    int customerId,
    std::string& error
) {
    error.clear();
    if (customers.searchById(customerId) == nullptr) {
        error = "Customer ID was not found in the in-memory list.";
        return CustomerDeleteStatus::notFound;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return CustomerDeleteStatus::error;
    }
    const CustomerDeleteStatus status =
        customerRepository.deleteCustomer(customerId, error, false);
    if (status != CustomerDeleteStatus::deleted) {
        rollbackWithError(transaction, error);
        return status;
    }

    unsigned long long actionId = 0;
    const std::string action =
        "Customer " + std::to_string(customerId) + " deleted";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return CustomerDeleteStatus::error;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return CustomerDeleteStatus::error;
    }
    if (!customers.deleteCustomer(customerId)) {
        error = "Customer deletion committed but the in-memory list could not "
                "be synchronized.";
        return CustomerDeleteStatus::error;
    }
    actionHistory.push(action);
    return CustomerDeleteStatus::deleted;
}

ReservationCreateResult HotelBackend::createReservation(
    const Reservation& reservation,
    std::string& error
) {
    error.clear();
    if (reservations.searchById(reservation.getReservationId()) != nullptr) {
        error = "Reservation ID already exists.";
        return ReservationCreateResult::duplicateId;
    }
    if (customers.searchById(reservation.getCustomerId()) == nullptr) {
        error = "Customer ID was not found.";
        return ReservationCreateResult::error;
    }
    const Room* room = hotel.findRoom(reservation.getRoomNumber());
    if (room == nullptr) {
        error = "Room number was not found.";
        return ReservationCreateResult::roomNotFound;
    }
    if (!reservation_validation::validateForRoomRate(
            reservation, room->getPricePerNight(), error
        )) {
        return ReservationCreateResult::error;
    }
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return ReservationCreateResult::error;
    }
    Reservation storedReservation;
    const ReservationCreateResult result =
        reservationRepository.createReservation(
            reservation, storedReservation, error
        );
    if (result != ReservationCreateResult::created) {
        rollbackWithError(transaction, error);
        return result;
    }

    unsigned long long actionId = 0;
    const std::string action = "Reservation " +
        std::to_string(reservation.getReservationId()) + " created";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return ReservationCreateResult::error;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return ReservationCreateResult::error;
    }
    if (!reservations.insertReservation(storedReservation)) {
        error = "Reservation committed but could not be added to memory.";
        return ReservationCreateResult::error;
    }
    actionHistory.push(action);
    return ReservationCreateResult::created;
}

bool HotelBackend::cancelReservation(
    int reservationId,
    std::string& error
) {
    error.clear();
    Reservation* reservation = reservations.searchById(reservationId);
    if (reservation == nullptr || reservation->getStatus() != "Confirmed") {
        error = "Only an existing confirmed reservation can be cancelled.";
        return false;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    if (!reservationRepository.updateStatus(
            reservationId, "Confirmed", "Cancelled", error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }
    unsigned long long actionId = 0;
    const std::string action =
        "Reservation " + std::to_string(reservationId) + " cancelled";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    reservation->setStatus("Cancelled");
    actionHistory.push(action);
    return true;
}

bool HotelBackend::checkIn(int reservationId, std::string& error) {
    error.clear();
    Reservation* reservation = reservations.searchById(reservationId);
    if (reservation == nullptr || reservation->getStatus() != "Confirmed") {
        error = "Only an existing confirmed reservation can be checked in.";
        return false;
    }
    const int roomNumber = reservation->getRoomNumber();
    const Room* room = hotel.findRoom(roomNumber);
    if (room == nullptr || room->getStatus() != "Available") {
        error = "Room is not available for check-in.";
        return false;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    std::string lockedStatus;
    bool roomFound = false;
    if (!roomRepository.lockRoomStatus(
            roomNumber, lockedStatus, roomFound, error
        ) ||
        !roomFound || lockedStatus != "Available") {
        if (error.empty()) {
            error = roomFound ? "Room is not available for check-in."
                              : "Room was not found.";
        }
        rollbackWithError(transaction, error);
        return false;
    }
    if (!reservationRepository.updateStatus(
            reservationId, "Confirmed", "CheckedIn", error
        ) ||
        !roomRepository.transitionStatus(
            roomNumber, "Available", "Occupied", error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }

    unsigned long long actionId = 0;
    const std::string action =
        "Reservation " + std::to_string(reservationId) + " checked in";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    reservation->setStatus("CheckedIn");
    hotel.setRoomStatus(roomNumber, "Occupied");
    actionHistory.push(action);
    return true;
}

bool HotelBackend::checkOut(
    int reservationId,
    std::string& notifiedCustomer,
    std::string& error
) {
    error.clear();
    notifiedCustomer.clear();
    Reservation* reservation = reservations.searchById(reservationId);
    if (reservation == nullptr || reservation->getStatus() != "CheckedIn") {
        error = "An existing checked-in reservation is required for checkout.";
        return false;
    }
    const int roomNumber = reservation->getRoomNumber();
    const Room* room = hotel.findRoom(roomNumber);
    if (room == nullptr || room->getStatus() != "Occupied") {
        error = "Room status is inconsistent; checkout was not completed.";
        return false;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    std::string lockedStatus;
    bool roomFound = false;
    if (!roomRepository.lockRoomStatus(
            roomNumber, lockedStatus, roomFound, error
        ) ||
        !roomFound || lockedStatus != "Occupied") {
        if (error.empty()) {
            error = roomFound ? "Room status is inconsistent."
                              : "Room was not found.";
        }
        rollbackWithError(transaction, error);
        return false;
    }
    if (!reservationRepository.updateStatus(
            reservationId, "CheckedIn", "CheckedOut", error
        ) ||
        !roomRepository.transitionStatus(
            roomNumber, "Occupied", "Available", error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }

    std::string dequeuedName;
    unsigned long long waitingEntryId = 0;
    bool hasWaitingCustomer = false;
    if (!waitingListRepository.dequeueOldest(
            dequeuedName, waitingEntryId, hasWaitingCustomer, error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (hasWaitingCustomer) {
        std::string inMemoryFront;
        if (!waitingList.peek(inMemoryFront) || inMemoryFront != dequeuedName) {
            error = "The in-memory waiting queue differs from MariaDB; checkout "
                    "was rolled back.";
            rollbackWithError(transaction, error);
            return false;
        }
    } else if (!waitingList.isEmpty()) {
        error = "The in-memory waiting queue differs from MariaDB; checkout "
                "was rolled back.";
        rollbackWithError(transaction, error);
        return false;
    }

    unsigned long long checkoutActionId = 0;
    const std::string checkoutAction =
        "Reservation " + std::to_string(reservationId) + " checked out";
    if (!actionHistoryRepository.recordAction(
            checkoutAction, checkoutActionId, error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }
    std::string notificationAction;
    if (hasWaitingCustomer) {
        notificationAction =
            "Waiting customer " + dequeuedName +
            " notified about available room " + std::to_string(roomNumber);
        unsigned long long notificationActionId = 0;
        if (!actionHistoryRepository.recordAction(
                notificationAction, notificationActionId, error
            )) {
            rollbackWithError(transaction, error);
            return false;
        }
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }

    reservation->setStatus("CheckedOut");
    hotel.setRoomStatus(roomNumber, "Available");
    actionHistory.push(checkoutAction);
    if (hasWaitingCustomer) {
        waitingList.dequeue();
        notifiedCustomer = dequeuedName;
        actionHistory.push(notificationAction);
    }
    return true;
}

bool HotelBackend::recordPayment(
    int reservationId,
    double amount,
    const std::string& method,
    const std::string& date,
    Payment& payment,
    std::string& error
) {
    error.clear();
    Reservation* reservation = reservations.searchById(reservationId);
    if (reservation == nullptr) {
        error = "Reservation was not found.";
        return false;
    }
    if (reservation->hasPayment()) {
        error = "A payment is already recorded for this reservation.";
        return false;
    }
    std::int64_t amountCents = 0;
    std::int64_t totalCents = 0;
    if (!hotel_money::toCents(
            amount, hotel_money::kDecimal12_2MaxCents, amountCents
        ) ||
        !hotel_money::toCents(
            reservation->getTotalCost(),
            hotel_money::kDecimal12_2MaxCents,
            totalCents
        ) ||
        amountCents != totalCents ||
        !validPaymentDate(date)) {
        error = "Payment must match the reservation total to two decimal "
                "places and use valid details.";
        return false;
    }
    const double normalizedAmount = hotel_money::fromCents(amountCents);
    Payment provisional(1, reservationId, normalizedAmount, method, date);
    Reservation validationCopy = *reservation;
    if (!validationCopy.recordPayment(provisional)) {
        error = "Payment must match the reservation total and use valid details.";
        return false;
    }

    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    Payment storedPayment;
    if (!paymentRepository.insertPayment(
            reservationId, normalizedAmount, method, date, storedPayment, error
        )) {
        rollbackWithError(transaction, error);
        return false;
    }
    unsigned long long actionId = 0;
    const std::string action =
        "Payment " + std::to_string(storedPayment.getPaymentId()) +
        " recorded for reservation " + std::to_string(reservationId);
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!reservation->recordPayment(storedPayment)) {
        error = "Payment committed but could not be attached to its reservation.";
        return false;
    }
    payment = storedPayment;
    actionHistory.push(action);
    return true;
}

bool HotelBackend::enqueueWaitingCustomer(
    const std::string& name,
    std::string& error
) {
    unsigned long long entryId = 0;
    return enqueueWaitingCustomer(name, entryId, error);
}

bool HotelBackend::enqueueWaitingCustomer(
    const std::string& name,
    unsigned long long& createdEntryId,
    std::string& error
) {
    error.clear();
    DatabaseTransaction transaction(database);
    if (!transaction.begin(error)) {
        return false;
    }
    unsigned long long entryId = 0;
    if (!waitingListRepository.enqueue(name, entryId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    unsigned long long actionId = 0;
    const std::string action = name + " added to waiting queue";
    if (!actionHistoryRepository.recordAction(action, actionId, error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    if (!transaction.commit(error)) {
        rollbackWithError(transaction, error);
        return false;
    }
    waitingList.enqueue(name);
    actionHistory.push(action);
    createdEntryId = entryId;
    return true;
}
