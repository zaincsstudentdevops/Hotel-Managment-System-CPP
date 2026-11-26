
#ifndef RESERVATIONLINKEDLIST_H
#define RESERVATIONLINKEDLIST_H

#include "Reservation.h"
#include <cstddef>
#include <string>

class ReservationLinkedList {
private:
    struct Node {
        Reservation data;
        Node* next;

        Node(Reservation reservation) {
            data = reservation;
            next = nullptr;
        }
    };

    Node* head;

public:
    ReservationLinkedList();
    ~ReservationLinkedList();

    ReservationLinkedList(const ReservationLinkedList&) = delete;
    ReservationLinkedList& operator=(const ReservationLinkedList&) = delete;

    bool insertReservation(Reservation reservation);
    Reservation* searchById(int id);
    bool hasOverlappingReservation(
        int roomNumber,
        const string& checkInDate,
        const string& checkOutDate
    ) const;
    bool deleteReservation(int id);
    std::size_t size() const;
    void displayAllReservations();
};

#endif
