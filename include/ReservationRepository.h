#ifndef RESERVATIONREPOSITORY_H
#define RESERVATIONREPOSITORY_H

#include "Database.h"
#include "Reservation.h"

#include <string>
#include <vector>

enum class ReservationCreateResult {
    created,
    duplicateId,
    roomNotFound,
    overlappingDates,
    error
};

class ReservationRepository {
public:
    explicit ReservationRepository(Database& database);

    bool loadReservations(
        std::vector<Reservation>& reservations,
        std::string& error
    ) const;
    ReservationCreateResult createReservation(
        const Reservation& reservation,
        Reservation& storedReservation,
        std::string& error
    ) const;
    bool findReservation(
        int reservationId,
        Reservation& reservation,
        bool& found,
        std::string& error
    ) const;
    bool updateStatus(
        int reservationId,
        const std::string& expectedStatus,
        const std::string& newStatus,
        std::string& error
    ) const;

private:
    Database& database;
};

#endif
