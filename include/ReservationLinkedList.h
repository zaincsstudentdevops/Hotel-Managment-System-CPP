
#ifndef RESERVATIONLINKEDLIST_H
#define RESERVATIONLINKEDLIST_H

#include "Reservation.h"

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

    void insertReservation(Reservation reservation);
    Reservation* searchById(int id);
    bool deleteReservation(int id);
    void displayAllReservations();
};

#endif
