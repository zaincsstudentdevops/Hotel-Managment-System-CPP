#include "Payment.h"
#include <iostream>
#include <iomanip>

Payment::Payment()
    : paymentId(0), reservationId(0), amount(0.0), paymentMethod("Cash"),
      paymentDate(""), paymentStatus("Unpaid") {}

Payment::Payment(int payId, int resId, double amt, string method, string date)
    : paymentId(payId), reservationId(resId), amount(amt), paymentMethod(method),
      paymentDate(date), paymentStatus("Paid") {}

int Payment::getPaymentId() const { return paymentId; }
int Payment::getReservationId() const { return reservationId; }
double Payment::getAmount() const { return amount; }
string Payment::getPaymentMethod() const { return paymentMethod; }
string Payment::getPaymentDate() const { return paymentDate; }
string Payment::getPaymentStatus() const { return paymentStatus; }

void Payment::displayReceipt() const {
    cout << "=== PAYMENT RECEIPT #" << paymentId << " ===" << endl;
    cout << "Booking ID   : #" << reservationId << endl;
    cout << "Amount Paid  : Rs." << fixed << setprecision(2) << amount << endl;
    cout << "Method       : " << paymentMethod << endl;
    cout << "Date         : " << paymentDate << endl;
    cout << "Status       : " << paymentStatus << endl;
    cout << "=============================" << endl;
}
