#include <iostream>
#include "Room.h"
#include "Customer.h"
#include "Reservation.h"
#include "Payment.h"

using namespace std;

int main() {
    cout << "--- STAGE 1 TEST: CONNECTING ALL 4 CORE CLASSES ---\n\n";

    Room room101(101, "Deluxe", 8500.0, "Available");
    room101.displayRoom();

    Customer guest1(1, "Zain ul Abedin", "0300-1234567", "zain@example.com", "34402-1234567-1");
    guest1.displayCustomer();

    if (room101.isAvailable()) {
        Reservation booking1(501, guest1.getCustomerId(), room101.getRoomNumber(),
                             "2026-11-25", "2026-11-28", 3, room101.getPricePerNight());
        room101.setStatus("Occupied");
        booking1.setStatus("CheckedIn");

        cout << "\n[Booking Created & Guest Checked In]\n";
        booking1.displayReservation();
        room101.displayRoom();

        cout << "\n[Processing Check-Out & Billing]\n";
        Payment receipt1(9001, booking1.getReservationId(), booking1.getTotalCost(), "Card", "2026-11-28");
        booking1.setStatus("CheckedOut");
        room101.setStatus("Available");

        receipt1.displayReceipt();
        room101.displayRoom();
    }

    return 0;
}
