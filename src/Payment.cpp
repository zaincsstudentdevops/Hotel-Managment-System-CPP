#include "Payment.h"
#include <iostream>
#include <iomanip>

Payment::Payment() : paymentId(0), reservationId(0), amount(0.0), paymentMethod("Cash"), paymentDate("") {}

Payment::Payment(int payId, int resId, double amt, string method, string date)
    : paymentId(payId), reservationId(resId), amount(amt), paymentMethod(method), paymentDate(date) {}

int Payment::getPaymentId() const { return paymentId; }
int Payment::getReservationId() const { return reservationId; }
double Payment::getAmount() const { return amount; }
string Payment::getPaymentMethod() const { return paymentMethod; }

void Payment::displayReceipt() const {
    cout << "=== PAYMENT RECEIPT #" << paymentId << " ===" << endl;
    cout << "Booking ID   : #" << reservationId << endl;
    cout << "Amount Paid  : Rs." << fixed << setprecision(2) << amount << endl;
    cout << "Method       : " << paymentMethod << endl;
    cout << "Date         : " << paymentDate << endl;
    cout << "=============================" << endl;
}
