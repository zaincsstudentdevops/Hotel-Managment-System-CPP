#ifndef MONEY_H
#define MONEY_H

#include <cmath>
#include <cstdint>

namespace hotel_money {

constexpr std::int64_t kDecimal10_2MaxCents = 9999999999LL;
constexpr std::int64_t kDecimal12_2MaxCents = 999999999999LL;

inline bool toCents(
    double amount,
    std::int64_t maximumCents,
    std::int64_t& cents
) {
    if (!std::isfinite(amount) || amount <= 0.0 || maximumCents <= 0) {
        return false;
    }

    const double roundedCents = std::round(amount * 100.0);
    if (!std::isfinite(roundedCents) || roundedCents < 1.0 ||
        roundedCents > static_cast<double>(maximumCents)) {
        return false;
    }

    cents = static_cast<std::int64_t>(roundedCents);
    return true;
}

inline double fromCents(std::int64_t cents) {
    return static_cast<double>(cents) / 100.0;
}

inline bool normalize(
    double amount,
    std::int64_t maximumCents,
    double& normalized
) {
    std::int64_t cents = 0;
    if (!toCents(amount, maximumCents, cents)) {
        return false;
    }
    normalized = fromCents(cents);
    return true;
}

}

#endif
