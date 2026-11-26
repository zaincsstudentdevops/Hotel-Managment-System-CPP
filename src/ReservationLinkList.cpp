
#include "ReservationLinkedList.h"
#include <iostream>

using namespace std;

ReservationLinkedList::ReservationLinkedList() {
    head = nullptr;
}

bool ReservationLinkedList::insertReservation(Reservation reservation) {
    if (searchById(reservation.getReservationId()) != nullptr) {
        return false;
    }

    Node* newNode = new Node(reservation);

    if (head == nullptr) {
        head = newNode;
        return true;
    }

    Node* current = head;

    while (current->next != nullptr) {
        current = current->next;
    }

    current->next = newNode;
    return true;
}

Reservation* ReservationLinkedList::searchById(int id) {
    Node* current = head;

    while (current != nullptr) {
        if (current->data.getReservationId() == id) {
            return &(current->data);
        }

        current = current->next;
    }

    return nullptr;
}

bool ReservationLinkedList::hasOverlappingReservation(
    int roomNumber,
    const string& checkInDate,
    const string& checkOutDate
) const {
    Node* current = head;
    while (current != nullptr) {
        if (current->data.getRoomNumber() == roomNumber &&
            current->data.overlapsWith(checkInDate, checkOutDate)) {
            return true;
        }

        current = current->next;
    }

    return false;
}

bool ReservationLinkedList::deleteReservation(int id) {
    if (head == nullptr) {
        return false;
    }

    if (head->data.getReservationId() == id) {
        Node* temp = head;
        head = head->next;
        delete temp;
        return true;
    }

    Node* current = head;

    while (current->next != nullptr) {
        if (current->next->data.getReservationId() == id) {
            Node* temp = current->next;
            current->next = temp->next;
            delete temp;
            return true;
        }

        current = current->next;
    }

    return false;
}

std::size_t ReservationLinkedList::size() const {
    std::size_t count = 0;
    Node* current = head;
    while (current != nullptr) {
        ++count;
        current = current->next;
    }
    return count;
}

void ReservationLinkedList::displayAllReservations() {
    Node* current = head;

    if (current == nullptr) {
        cout << "No reservations found.\n";
        return;
    }

    while (current != nullptr) {
        current->data.displayReservation();
        current = current->next;
    }
}

ReservationLinkedList::~ReservationLinkedList() {
    Node* current = head;

    while (current != nullptr) {
        Node* temp = current;
        current = current->next;
        delete temp;
    }
}
