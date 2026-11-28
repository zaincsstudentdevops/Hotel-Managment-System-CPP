#ifndef SMARTROOMFINDER_H
#define SMARTROOMFINDER_H

#include "Reservation.h"
#include "Room.h"

#include <string>
#include <vector>

struct SmartRoomRequirement {
    std::string checkInDate;
    std::string checkOutDate;
    std::string category;
    double maximumBudgetPerNight = 0.0;
    int numberOfGuests = 1;
};

struct SmartRoomRecommendation {
    Room room;
    int numberOfNights = 0;
    int maximumGuests = 0;
    double totalCost = 0.0;
    std::string reason;
};

class SmartRoomFinder {
public:
    std::vector<SmartRoomRecommendation> findRecommendations(
        const std::vector<Room>& rooms,
        const std::vector<Reservation>& reservations,
        const SmartRoomRequirement& requirement,
        std::string& error
    ) const;

private:
    bool validateDateRange(
        const SmartRoomRequirement& requirement,
        int& nights,
        std::string& error
    ) const;
};

#endif
