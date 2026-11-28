#ifndef RESERVATIONVALIDATION_H
#define RESERVATIONVALIDATION_H

#include "Money.h"
#include "Reservation.h"

#include <cstdint>
#include <ctime>
#include <string>

namespace reservation_validation {

inline bool dateOrdinal(const std::string& date, std::int64_t& ordinal) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    int values[3]{};
    int component = 0;
    for (std::size_t index = 0; index < date.size(); ++index) {
        if (index == 4 || index == 7) {
            ++component;
            continue;
        }
        if (date[index] < '0' || date[index] > '9') {
            return false;
        }
        values[component] = values[component] * 10 +
                            date[index] - '0';
    }

    const int year = values[0];
    const int month = values[1];
    const int day = values[2];
    if (year < 1000 || month < 1 || month > 12) {
        return false;
    }

    const bool leapYear =
        (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int daysInMonth[] = {
        31, leapYear ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    if (day < 1 || day > daysInMonth[month - 1]) {
        return false;
    }

    const std::int64_t previousYear = year - 1;
    ordinal = previousYear * 365 + previousYear / 4 -
              previousYear / 100 + previousYear / 400;
    for (int index = 0; index < month - 1; ++index) {
        ordinal += daysInMonth[index];
    }
    ordinal += day;
    return true;
}

inline bool currentIsoDate(std::string& date) {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    if (localtime_s(&localTime, &now) != 0) {
        return false;
    }
#else
    if (localtime_r(&now, &localTime) == nullptr) {
        return false;
    }
#endif

    char buffer[11]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &localTime) != 10) {
        return false;
    }
    date.assign(buffer, 10);
    return true;
}

inline bool validateCheckInDateNotPast(
    const std::string& checkInDate,
    std::string& error
) {
    std::int64_t checkInOrdinal = 0;
    std::int64_t todayOrdinal = 0;
    std::string today;
    if (!dateOrdinal(checkInDate, checkInOrdinal) ||
        !currentIsoDate(today) ||
        !dateOrdinal(today, todayOrdinal)) {
        error = "Could not validate the check-in date.";
        return false;
    }
    if (checkInOrdinal < todayOrdinal) {
        error = "Check-in date must be today or later.";
        return false;
    }
    error.clear();
    return true;
}

inline bool validateDatesAndNights(
    const Reservation& reservation,
    std::string& error
) {
    std::int64_t checkInOrdinal = 0;
    std::int64_t checkOutOrdinal = 0;
    if (!dateOrdinal(reservation.getCheckInDate(), checkInOrdinal) ||
        !dateOrdinal(reservation.getCheckOutDate(), checkOutOrdinal) ||
        checkOutOrdinal <= checkInOrdinal) {
        error = "Reservation dates must be valid and check-out must follow "
                "check-in.";
        return false;
    }

    if (!validateCheckInDateNotPast(
            reservation.getCheckInDate(), error
        )) {
        return false;
    }

    const std::int64_t actualNights = checkOutOrdinal - checkInOrdinal;
    if (reservation.getNumberOfNights() <= 0 ||
        actualNights != reservation.getNumberOfNights()) {
        error = "Reservation nights must match the date range.";
        return false;
    }
    return true;
}

inline bool validateTotalForRoomRate(
    const Reservation& reservation,
    double roomPricePerNight,
    std::string& error
) {
    std::int64_t roomPriceCents = 0;
    std::int64_t totalCents = 0;
    if (!hotel_money::toCents(
            roomPricePerNight,
            hotel_money::kDecimal10_2MaxCents,
            roomPriceCents
        ) ||
        !hotel_money::toCents(
            reservation.getTotalCost(),
            hotel_money::kDecimal12_2MaxCents,
            totalCents
        )) {
        error = "Room price and reservation total must be positive valid "
                "monetary values.";
        return false;
    }

    const std::int64_t nights = reservation.getNumberOfNights();
    if (nights <= 0 ||
        roomPriceCents > hotel_money::kDecimal12_2MaxCents / nights) {
        error = "Reservation total exceeds the supported DECIMAL(12,2) range.";
        return false;
    }
    const std::int64_t expectedTotalCents = roomPriceCents * nights;
    if (totalCents != expectedTotalCents) {
        error = "Reservation total must equal the room rate times the number "
                "of nights to two decimal places.";
        return false;
    }
    return true;
}

inline bool validateForRoomRate(
    const Reservation& reservation,
    double roomPricePerNight,
    std::string& error
) {
    return validateDatesAndNights(reservation, error) &&
           validateTotalForRoomRate(
               reservation, roomPricePerNight, error
           );
}

}

#endif
