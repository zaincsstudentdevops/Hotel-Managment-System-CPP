#ifndef PAYMENTREPOSITORY_H
#define PAYMENTREPOSITORY_H

#include "Database.h"
#include "Payment.h"

#include <string>
#include <vector>

class PaymentRepository {
public:
    explicit PaymentRepository(Database& database);

    bool loadPayments(std::vector<Payment>& payments, std::string& error) const;
    bool insertPayment(
        int reservationId,
        double amount,
        const std::string& method,
        const std::string& date,
        Payment& payment,
        std::string& error
    ) const;

private:
    Database& database;
};

#endif
