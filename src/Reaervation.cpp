#include "Reservation.h"
#include <iostream>
#include <iomanip>
#include <cmath>

Reservation::Reservation()
    : reservationId(0), customerId(0), roomNumber(0), numberOfNights(0),
      totalCost(0.0), status("Confirmed"), paymentRecorded(false) {}

Reservation::Reservation(int resId, int custId, int rmNum, string inDate, string outDate, int nights, double ratePerNight)
    : reservationId(resId), customerId(custId), roomNumber(rmNum),
      checkInDate(inDate), checkOutDate(outDate), numberOfNights(nights),
      status("Confirmed"), paymentRecorded(false) {
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
bool Reservation::hasPayment() const { return paymentRecorded; }

void Reservation::setStatus(string newStatus) { status = newStatus; }

bool Reservation::recordPayment(const Payment& newPayment) {
    if (paymentRecorded ||
        newPayment.getPaymentId() <= 0 ||
        newPayment.getReservationId() != reservationId ||
        !isfinite(newPayment.getAmount()) ||
        newPayment.getAmount() <= 0.0 ||
        !isfinite(totalCost) ||
        totalCost <= 0.0 ||
        fabs(newPayment.getAmount() - totalCost) > 0.005 ||
        (newPayment.getPaymentMethod() != "Cash" &&
         newPayment.getPaymentMethod() != "Card") ||
        newPayment.getPaymentDate().empty()) {
        return false;
    }

    payment = newPayment;
    paymentRecorded = true;
    return true;
}

bool Reservation::overlapsWith(string reqStart, string reqEnd) const {
    if (status == "Cancelled") return false;
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

    if (paymentRecorded) {
        payment.displayReceipt();
    } else {
        cout << "Payment Status: Unpaid"
             << " | Reservation ID: #" << reservationId
             << " | Amount Due: Rs." << fixed << setprecision(2) << totalCost
             << endl;
    }
}
