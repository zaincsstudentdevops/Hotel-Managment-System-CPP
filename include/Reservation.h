#ifndef RESERVATION_H
#define RESERVATION_H

#include "Payment.h"
#include <string>
using namespace std;

class Reservation {
private:
    int reservationId;
    int customerId;
    int roomNumber;
    string checkInDate;   // Format: YYYY-MM-DD
    string checkOutDate;  // Format: YYYY-MM-DD
    int numberOfNights;
    double totalCost;
    string status;        // "Confirmed", "CheckedIn", "CheckedOut", "Cancelled"
    Payment payment;
    bool paymentRecorded;

public:
    Reservation();
    Reservation(int resId, int custId, int rmNum, string inDate, string outDate, int nights, double ratePerNight);
    static Reservation fromStoredData(
        int resId,
        int custId,
        int rmNum,
        string inDate,
        string outDate,
        int nights,
        double storedTotal,
        string storedStatus
    );

    int getReservationId() const;
    int getCustomerId() const;
    int getRoomNumber() const;
    string getCheckInDate() const;
    string getCheckOutDate() const;
    int getNumberOfNights() const;
    double getTotalCost() const;
    string getStatus() const;
    bool hasPayment() const;
    const Payment& getPayment() const;

    void setStatus(string newStatus);
    bool recordPayment(const Payment& payment);
    bool overlapsWith(string reqStart, string reqEnd) const;
    void displayReservation() const;
};

#endif
