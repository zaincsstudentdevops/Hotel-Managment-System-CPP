#ifndef HOTELBACKEND_H
#define HOTELBACKEND_H

#include "ActionHistoryRepository.h"
#include "ActionStack.h"
#include "CustomerRepository.h"
#include "Database.h"
#include "Hotel.h"
#include "PaymentRepository.h"
#include "ReservationLinkedList.h"
#include "ReservationRepository.h"
#include "RoomRepository.h"
#include "SmartRoomFinder.h"
#include "WaitingListRepository.h"
#include "WaitingQueue.h"

#include <cstddef>
#include <string>
#include <vector>

struct DashboardSnapshot {
    std::size_t totalCustomers = 0;
    std::size_t totalRooms = 0;
    std::size_t availableRooms = 0;
    std::size_t occupiedRooms = 0;
    std::size_t maintenanceRooms = 0;
    std::size_t totalReservations = 0;
    std::size_t confirmedReservations = 0;
    std::size_t checkedInReservations = 0;
    std::size_t checkedOutReservations = 0;
    std::size_t cancelledReservations = 0;
    std::size_t totalPayments = 0;
    std::size_t waitingListSize = 0;
    double totalPaymentsAmount = 0.0;
};

class HotelBackend {
public:
    HotelBackend(
        Database& database,
        Hotel& hotel,
        CustomerLinkedList& customers,
        ReservationLinkedList& reservations,
        WaitingQueue& waitingList,
        ActionStack& actionHistory
    );

    bool loadAll(std::string& error);
    std::vector<Customer> getCustomersSnapshot() const;
    std::vector<Room> getRoomsSnapshot() const;
    std::vector<Reservation> getReservationsSnapshot() const;
    std::vector<Payment> getPaymentsSnapshot() const;
    std::vector<SmartRoomRecommendation> findSmartRoomRecommendations(
        const SmartRoomRequirement& requirement,
        std::string& error
    ) const;
    bool getWaitingListSnapshot(
        std::vector<WaitingListEntrySnapshot>& entries,
        std::string& error
    ) const;
    DashboardSnapshot getDashboardSnapshot() const;

    bool addRoom(const Room& room, std::string& error);
    bool registerCustomer(const Customer& customer, std::string& error);
    CustomerLookupStatus findCustomerById(
        int customerId,
        Customer& customer,
        std::string& error
    );
    CustomerUpdateStatus updateCustomer(
        const Customer& customer,
        std::string& error
    );
    CustomerDeleteStatus deleteCustomer(int customerId, std::string& error);

    ReservationCreateResult createReservation(
        const Reservation& reservation,
        std::string& error
    );
    bool cancelReservation(int reservationId, std::string& error);
    bool checkIn(int reservationId, std::string& error);
    bool checkOut(
        int reservationId,
        std::string& notifiedCustomer,
        std::string& error
    );
    bool recordPayment(
        int reservationId,
        double amount,
        const std::string& method,
        const std::string& date,
        Payment& payment,
        std::string& error
    );
    bool enqueueWaitingCustomer(const std::string& name, std::string& error);
    bool enqueueWaitingCustomer(
        const std::string& name,
        unsigned long long& entryId,
        std::string& error
    );

private:
    Database& database;
    Hotel& hotel;
    CustomerLinkedList& customers;
    ReservationLinkedList& reservations;
    WaitingQueue& waitingList;
    ActionStack& actionHistory;

    CustomerRepository customerRepository;
    RoomRepository roomRepository;
    ReservationRepository reservationRepository;
    PaymentRepository paymentRepository;
    WaitingListRepository waitingListRepository;
    ActionHistoryRepository actionHistoryRepository;
};

#endif
