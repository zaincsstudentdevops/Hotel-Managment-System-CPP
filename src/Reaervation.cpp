#include "Reservation.h"
#include <iostream>
#include <iomanip>

Reservation::Reservation()
    : reservationId(0), customerId(0), roomNumber(0), numberOfNights(0), totalCost(0.0), status("Confirmed") {}

Reservation::Reservation(int resId, int custId, int rmNum, string inDate, string outDate, int nights, double ratePerNight)
    : reservationId(resId), customerId(custId), roomNumber(rmNum),
      checkInDate(inDate), checkOutDate(outDate), numberOfNights(nights), status("Confirmed") {
    totalCost = numberOfNights * ratePerNight;
}

int Reservation::getReservationId() const { return reservationId; }
int Reservation::getCustomerId() const { return customerId; }
int Reservation::getRoomNumber() const { return roomNumber; }
string Reservation::getCheckInDate() const { return checkInDate; }
string Reservation::getCheckOutDate() const { return checkOutDate; }
int Reservation::getNumberOfNights() const { return numberOfNights; }
double Reservation::getTotalCost() const { return totalCost; }
string Reservation::getStatus() const { return status; }

void Reservation::setStatus(string newStatus) { status = newStatus; }

bool Reservation::overlapsWith(string reqStart, string reqEnd) const {
    if (status == "Cancelled" || status == "CheckedOut") return false;
    return (checkInDate < reqEnd) && (checkOutDate > reqStart);
}

void Reservation::displayReservation() const {
    cout << "Booking #" << reservationId
         << " | Guest ID: " << customerId
         << " | Room #" << roomNumber
         << " | " << checkInDate << " to " << checkOutDate
         << " (" << numberOfNights << " nights)"
         << " | Total: Rs." << fixed << setprecision(2) << totalCost
         << " | Status: " << status << endl;
}
