#include "SmartRoomFinder.h"
#include "ReservationValidation.h"

#include <cmath>
#include <iomanip>
#include <queue>
#include <sstream>
#include <vector>

namespace {

struct SearchCandidate {
    Room room;
    int nights = 0;
    int maximumGuests = 0;
    double totalCost = 0.0;

    bool operator>(const SearchCandidate& other) const {
        if (totalCost != other.totalCost) {
            return totalCost > other.totalCost;
        }
        return room.getRoomNumber() > other.room.getRoomNumber();
    }
};

int maximumGuestsForCategory(const std::string& category) {
    if (category == "Single") {
        return 1;
    }
    if (category == "Double" || category == "Deluxe") {
        return 2;
    }
    return 0;
}

bool isValidIsoDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    for (std::size_t index = 0; index < date.size(); ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        if (date[index] < '0' || date[index] > '9') {
            return false;
        }
        int value = date[index] - '0';
        if (index < 4) {
            year = year * 10 + value;
        } else if (index < 7) {
            month = month * 10 + value;
        } else {
            day = day * 10 + value;
        }
    }
    if (year <= 0 || month < 1 || month > 12) {
        return false;
    }
    const bool leap =
        (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int days[] = {
        31, leap ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    return day >= 1 && day <= days[month - 1];
}

int dateDistance(const std::string& first, const std::string& second) {
    int firstYear = std::stoi(first.substr(0, 4));
    int firstMonth = std::stoi(first.substr(5, 2));
    int firstDay = std::stoi(first.substr(8, 2));
    int secondYear = std::stoi(second.substr(0, 4));
    int secondMonth = std::stoi(second.substr(5, 2));
    int secondDay = std::stoi(second.substr(8, 2));

    const auto daysBeforeMonth = [](int month, bool leap) {
        static const int normal[] = {0, 31, 59, 90, 120, 151,
                                     181, 212, 243, 273, 304, 334};
        static const int leapDays[] = {0, 31, 60, 91, 121, 152,
                                       182, 213, 244, 274, 305, 335};
        return leap ? leapDays[month - 1] : normal[month - 1];
    };

    const auto ordinal = [&](int year, int month, int day) {
        int previousYear = year - 1;
        bool leapYear =
            (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return previousYear * 365 + previousYear / 4 -
               previousYear / 100 + previousYear / 400 +
               daysBeforeMonth(month, leapYear) + day;
    };

    return ordinal(secondYear, secondMonth, secondDay) -
           ordinal(firstYear, firstMonth, firstDay);
}

}

bool SmartRoomFinder::validateDateRange(
    const SmartRoomRequirement& requirement,
    int& nights,
    std::string& error
) const {
    if (!isValidIsoDate(requirement.checkInDate) ||
        !isValidIsoDate(requirement.checkOutDate)) {
        error = "Check-in and check-out must be valid YYYY-MM-DD dates.";
        return false;
    }
    if (!reservation_validation::validateCheckInDateNotPast(
            requirement.checkInDate, error
        )) {
        return false;
    }
    nights = dateDistance(requirement.checkInDate,
                          requirement.checkOutDate);
    if (nights <= 0) {
        error = "Check-out date must be after the check-in date.";
        return false;
    }
    if (!std::isfinite(requirement.maximumBudgetPerNight) ||
        requirement.maximumBudgetPerNight <= 0.0) {
        error = "Maximum budget per night must be a positive number.";
        return false;
    }
    if (requirement.numberOfGuests <= 0) {
        error = "Number of guests must be a positive whole number.";
        return false;
    }
    if (!requirement.category.empty() &&
        requirement.category != "Any" &&
        requirement.category != "Single" &&
        requirement.category != "Double" &&
        requirement.category != "Deluxe") {
        error = "Room category must be Any, Single, Double, or Deluxe.";
        return false;
    }
    error.clear();
    return true;
}

std::vector<SmartRoomRecommendation> SmartRoomFinder::findRecommendations(
    const std::vector<Room>& rooms,
    const std::vector<Reservation>& reservations,
    const SmartRoomRequirement& requirement,
    std::string& error
) const {
    int nights = 0;
    if (!validateDateRange(requirement, nights, error)) {
        return {};
    }

    std::priority_queue<
        SearchCandidate,
        std::vector<SearchCandidate>,
        std::greater<SearchCandidate>
    > frontier;

    for (const Room& room : rooms) {
        if (room.getStatus() != "Available" &&
            room.getStatus() != "Occupied") {
            continue;
        }
        if (!requirement.category.empty() &&
            requirement.category != "Any" &&
            room.getCategory() != requirement.category) {
            continue;
        }
        const int roomCapacity = maximumGuestsForCategory(room.getCategory());
        if (roomCapacity < requirement.numberOfGuests) {
            continue;
        }
        if (room.getPricePerNight() > requirement.maximumBudgetPerNight) {
            continue;
        }

        bool hasConflict = false;
        for (const Reservation& reservation : reservations) {
            if (reservation.getRoomNumber() == room.getRoomNumber() &&
                reservation.overlapsWith(requirement.checkInDate,
                                         requirement.checkOutDate)) {
                hasConflict = true;
                break;
            }
        }
        if (hasConflict) {
            continue;
        }

        frontier.push({
            room,
            nights,
            roomCapacity,
            room.getPricePerNight() * static_cast<double>(nights)
        });
    }

    std::vector<SmartRoomRecommendation> recommendations;
    while (!frontier.empty()) {
        SearchCandidate candidate = frontier.top();
        frontier.pop();

        std::ostringstream reason;
        reason << "Available for " << nights << " night"
               << (nights == 1 ? "" : "s")
               << "; category " << candidate.room.getCategory()
               << "; accommodates up to " << candidate.maximumGuests
               << (candidate.maximumGuests == 1 ? " guest" : " guests")
               << "; Rs." << std::fixed << std::setprecision(2)
               << candidate.room.getPricePerNight()
               << " per night; no date conflict";

        recommendations.push_back({
            candidate.room,
            candidate.nights,
            candidate.maximumGuests,
            candidate.totalCost,
            reason.str()
        });
    }

    error.clear();
    return recommendations;
}
