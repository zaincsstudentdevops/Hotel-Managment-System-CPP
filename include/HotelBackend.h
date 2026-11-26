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
#include "WaitingListRepository.h"
#include "WaitingQueue.h"

#include <string>

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
