#include "SmartRoomFinder.h"
#include "ReservationValidation.h"

#include <iostream>
#include <vector>

int main() {
    std::vector<Room> rooms = {
        Room(101, "Single", 5000.0, "Available"),
        Room(102, "Double", 7000.0, "Available"),
        Room(103, "Deluxe", 12000.0, "Available"),
        Room(104, "Double", 6500.0, "Maintenance")
    };

    std::vector<Reservation> reservations = {
        Reservation(1, 1, 102, "2026-12-10", "2026-12-15",
                    5, 7000.0)
    };

    SmartRoomFinder finder;
    std::string error;

    SmartRoomRequirement suitable{
        "2026-12-12",
        "2026-12-14",
        "Double",
        8000.0,
        2
    };
    const auto noSuitable = finder.findRecommendations(
        rooms, reservations, suitable, error
    );
    if (!noSuitable.empty()) {
        std::cerr << "Date conflict test failed.\n";
        return 1;
    }

    suitable.checkInDate = "2026-12-20";
    suitable.checkOutDate = "2026-12-22";
    const auto suitableRooms = finder.findRecommendations(
        rooms, reservations, suitable, error
    );
    if (suitableRooms.size() != 1 ||
        suitableRooms[0].room.getRoomNumber() != 102 ||
        suitableRooms[0].numberOfNights != 2 ||
        suitableRooms[0].totalCost != 14000.0) {
        std::cerr << "Suitable-room ordering test failed.\n";
        return 1;
    }

    SmartRoomRequirement invalid{
        "2026-12-22",
        "2026-12-20",
        "Any",
        5000.0,
        1
    };
    const auto invalidRooms = finder.findRecommendations(
        rooms, reservations, invalid, error
    );
    if (!invalidRooms.empty() || error.empty()) {
        std::cerr << "Invalid-date test failed.\n";
        return 1;
    }

    SmartRoomRequirement pastDate{
        "2000-01-01",
        "2000-01-02",
        "Any",
        20000.0,
        1
    };
    if (!finder.findRecommendations(
            rooms, reservations, pastDate, error
        ).empty() ||
        error != "Check-in date must be today or later.") {
        std::cerr << "Past-date finder validation test failed.\n";
        return 1;
    }

    std::string today;
    if (!reservation_validation::currentIsoDate(today) ||
        !reservation_validation::validateCheckInDateNotPast(today, error)) {
        std::cerr << "Today check-in validation test failed.\n";
        return 1;
    }

    const Reservation historicalReservation(
        3, 1, 101, "2000-01-01", "2000-01-02", 1, 5000.0
    );
    if (reservation_validation::validateDatesAndNights(
            historicalReservation, error
        ) ||
        error != "Check-in date must be today or later.") {
        std::cerr << "Past-date booking validation test failed.\n";
        return 1;
    }

    SmartRoomRequirement budgetOnly{
        "2026-12-20",
        "2026-12-21",
        "Any",
        6000.0,
        1
    };
    const auto orderedRooms = finder.findRecommendations(
        rooms, reservations, budgetOnly, error
    );
    if (orderedRooms.size() != 1 ||
        orderedRooms[0].room.getRoomNumber() != 101) {
        std::cerr << "Budget UCS test failed.\n";
        return 1;
    }

    SmartRoomRequirement underBudget{
        "2026-12-20",
        "2026-12-21",
        "Any",
        4000.0,
        1
    };
    if (!finder.findRecommendations(
            rooms, reservations, underBudget, error
        ).empty()) {
        std::cerr << "Insufficient-budget test failed.\n";
        return 1;
    }

    SmartRoomRequirement unavailableCategory{
        "2026-12-20",
        "2026-12-21",
        "Deluxe",
        8000.0,
        1
    };
    if (!finder.findRecommendations(
            rooms, reservations, unavailableCategory, error
        ).empty()) {
        std::cerr << "Category no-match test failed.\n";
        return 1;
    }

    SmartRoomRequirement tooManyGuests{
        "2026-12-20",
        "2026-12-21",
        "Any",
        20000.0,
        3
    };
    const auto capacityLimited = finder.findRecommendations(
        rooms, reservations, tooManyGuests, error
    );
    if (!capacityLimited.empty()) {
        std::cerr << "Guest capacity no-match test failed.\n";
        return 1;
    }

    tooManyGuests.numberOfGuests = 2;
    const auto capacityMatches = finder.findRecommendations(
        rooms, reservations, tooManyGuests, error
    );
    if (capacityMatches.size() != 2 ||
        capacityMatches[0].maximumGuests != 2 ||
        capacityMatches[0].reason.find("accommodates up to 2 guests") ==
            std::string::npos) {
        std::cerr << "Guest capacity matching test failed.\n";
        return 1;
    }

    tooManyGuests.numberOfGuests = 0;
    const auto invalidGuestCount = finder.findRecommendations(
        rooms, reservations, tooManyGuests, error
    );
    if (!invalidGuestCount.empty() || error.empty()) {
        std::cerr << "Invalid guest-count test failed.\n";
        return 1;
    }

    const std::vector<Room> occupiedRooms = {
        Room(105, "Double", 7000.0, "Occupied")
    };
    const std::vector<Reservation> occupiedReservations = {
        Reservation::fromStoredData(
            2, 1, 105, "2026-12-10", "2026-12-15",
            5, 35000.0, "CheckedIn"
        )
    };
    SmartRoomRequirement afterCheckout{
        "2026-12-15",
        "2026-12-17",
        "Double",
        8000.0,
        2
    };
    const auto futureOccupiedRoom = finder.findRecommendations(
        occupiedRooms, occupiedReservations, afterCheckout, error
    );
    if (futureOccupiedRoom.size() != 1 ||
        futureOccupiedRoom[0].room.getRoomNumber() != 105) {
        std::cerr << "Non-overlapping occupied-room test failed.\n";
        return 1;
    }

    afterCheckout.checkInDate = "2026-12-14";
    afterCheckout.checkOutDate = "2026-12-16";
    const auto conflictingOccupiedRoom = finder.findRecommendations(
        occupiedRooms, occupiedReservations, afterCheckout, error
    );
    if (!conflictingOccupiedRoom.empty()) {
        std::cerr << "Occupied-room conflict test failed.\n";
        return 1;
    }

    const std::vector<Room> maintenanceRooms = {
        Room(106, "Double", 7000.0, "Maintenance")
    };
    const auto excludedMaintenanceRoom = finder.findRecommendations(
        maintenanceRooms, occupiedReservations, afterCheckout, error
    );
    if (!excludedMaintenanceRoom.empty()) {
        std::cerr << "Maintenance-room exclusion test failed.\n";
        return 1;
    }

    if (!finder.findRecommendations(
            {}, {}, suitable, error
        ).empty()) {
        std::cerr << "Empty inventory test failed.\n";
        return 1;
    }

    std::cout << "Smart room finder tests passed.\n";
    return 0;
}
