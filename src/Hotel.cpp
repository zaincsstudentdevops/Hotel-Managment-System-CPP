
#include "Hotel.h"
#include <iostream>

using namespace std;

Hotel::Hotel() {
    roomCount = 0;
}

bool Hotel::addRoom(Room room) {
    if (roomCount >= 50) {
        cout << "Hotel is full. Cannot add more rooms.\n";
        return false;
    }

    if (findRoomIndex(room.getRoomNumber()) != -1) {
        cout << "Room number already exists.\n";
        return false;
    }

    rooms[roomCount] = room;
    roomCount++;

    cout << "Room added successfully.\n";
    return true;
}

int Hotel::findRoomIndex(int roomNumber) const {
    for (int i = 0; i < roomCount; i++) {
        if (rooms[i].getRoomNumber() == roomNumber) {
            return i;
        }
    }

    return -1;
}

bool Hotel::updateRoomStatus(int roomNumber, string newStatus) {
    int index = findRoomIndex(roomNumber);

    if (index == -1) {
        cout << "Room not found.\n";
        return false;
    }

    rooms[index].setStatus(newStatus);
    cout << "Room status updated successfully.\n";
    return true;
}

void Hotel::displayAllRooms() const {
    if (roomCount == 0) {
        cout << "No rooms have been added yet.\n";
        return;
    }

    cout << "\n===== HOTEL ROOM LIST =====\n";

    for (int i = 0; i < roomCount; i++) {
        rooms[i].displayRoom();
    }
}
