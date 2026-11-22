
#include <iostream>
#include "Hotel.h"

using namespace std;

int main() {
    Hotel myHotel;

    cout << "===== STAGE 2: HOTEL ROOM ARRAY TEST =====\n";

    Room room1(101, "Single", 4000.0, "Available");
    Room room2(102, "Double", 6000.0, "Available");
    Room room3(103, "Deluxe", 8500.0, "Available");

    myHotel.addRoom(room1);
    myHotel.addRoom(room2);
    myHotel.addRoom(room3);

    myHotel.displayAllRooms();

    cout << "\n===== SEARCHING FOR ROOM 102 =====\n";

    int index = myHotel.findRoomIndex(102);

    if (index != -1) {
        cout << "Room 102 found!\n";
    } else {
        cout << "Room not found.\n";
    }

    cout << "\n===== UPDATING ROOM STATUS =====\n";

    myHotel.updateRoomStatus(102, "Occupied");

    myHotel.displayAllRooms();

    return 0;
}
