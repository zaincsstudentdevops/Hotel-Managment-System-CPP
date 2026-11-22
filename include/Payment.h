#ifndef PAYMENT_H
#define PAYMENT_H

#include <string>
using namespace std;

class Payment {
private:
    int paymentId;
    int reservationId;
    double amount;
    string paymentMethod; // "Cash" or "Card"
    string paymentDate;

public:
    Payment();
    Payment(int payId, int resId, double amt, string method, string date);

    int getPaymentId() const;
    int getReservationId() const;
    double getAmount() const;
    string getPaymentMethod() const;
    void displayReceipt() const;
};

#endif
