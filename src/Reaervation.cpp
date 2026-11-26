#include "Reservation.h"
#include "Money.h"

#include <iostream>
#include <iomanip>

Reservation::Reservation()
    : reservationId(0), customerId(0), roomNumber(0), numberOfNights(0),
      totalCost(0.0), status("Confirmed"), paymentRecorded(false) {}

Reservation::Reservation(int resId, int custId, int rmNum, string inDate, string outDate, int nights, double ratePerNight)
    : reservationId(resId), customerId(custId), roomNumber(rmNum),
      checkInDate(inDate), checkOutDate(outDate), numberOfNights(nights),
      status("Confirmed"), paymentRecorded(false) {
    totalCost = numberOfNights * ratePerNight;
}

Reservation Reservation::fromStoredData(
    int resId,
    int custId,
    int rmNum,
    string inDate,
    string outDate,
    int nights,
    double storedTotal,
    string storedStatus
) {
    Reservation reservation;
    reservation.reservationId = resId;
    reservation.customerId = custId;
    reservation.roomNumber = rmNum;
    reservation.checkInDate = std::move(inDate);
    reservation.checkOutDate = std::move(outDate);
    reservation.numberOfNights = nights;
    reservation.totalCost = storedTotal;
    reservation.status = std::move(storedStatus);
    return reservation;
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
const Payment& Reservation::getPayment() const { return payment; }

void Reservation::setStatus(string newStatus) { status = newStatus; }

bool Reservation::recordPayment(const Payment& newPayment) {
    std::int64_t paymentCents = 0;
    std::int64_t totalCents = 0;
    if (paymentRecorded ||
        newPayment.getPaymentId() <= 0 ||
        newPayment.getReservationId() != reservationId ||
        !hotel_money::toCents(
            newPayment.getAmount(),
            hotel_money::kDecimal12_2MaxCents,
            paymentCents
        ) ||
        !hotel_money::toCents(
            totalCost, hotel_money::kDecimal12_2MaxCents, totalCents
        ) ||
        paymentCents != totalCents ||
        (newPayment.getPaymentMethod() != "Cash" &&
         newPayment.getPaymentMethod() != "Card") ||
        newPayment.getPaymentDate().empty()) {
        return false;
    }

    payment = Payment(
        newPayment.getPaymentId(),
        newPayment.getReservationId(),
        hotel_money::fromCents(paymentCents),
        newPayment.getPaymentMethod(),
        newPayment.getPaymentDate()
    );
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
