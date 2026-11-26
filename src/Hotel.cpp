
#include "Hotel.h"
#include <iostream>
#include <set>

using namespace std;

Hotel::Hotel() {
    roomCount = 0;
}

bool Hotel::addRoom(Room room) {
    if (!canAddRoom(room.getRoomNumber())) {
        return false;
    }

    rooms[roomCount] = room;
    roomCount++;

    return true;
}

bool Hotel::canAddRoom(int roomNumber) const {
    if (roomCount >= 50) {
        cout << "Hotel is full. Cannot add more rooms.\n";
        return false;
    }

    if (findRoomIndex(roomNumber) != -1) {
        cout << "Room number already exists.\n";
        return false;
    }
    return true;
}

bool Hotel::loadRooms(
    const std::vector<Room>& loadedRooms,
    std::string& error
) {
    if (loadedRooms.size() > 50) {
        error = "The database contains more than the supported 50 rooms.";
        return false;
    }

    std::set<int> roomNumbers;
    for (const Room& room : loadedRooms) {
        if (room.getRoomNumber() <= 0 ||
            (room.getCategory() != "Single" &&
             room.getCategory() != "Double" &&
             room.getCategory() != "Deluxe") ||
            room.getPricePerNight() <= 0.0 ||
            (room.getStatus() != "Available" &&
             room.getStatus() != "Occupied" &&
             room.getStatus() != "Maintenance") ||
            !roomNumbers.insert(room.getRoomNumber()).second) {
            error = "The database contains an invalid or duplicate room record.";
            return false;
        }
    }

    roomCount = 0;
    for (const Room& room : loadedRooms) {
        rooms[roomCount++] = room;
    }
    error.clear();
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

const Room* Hotel::findRoom(int roomNumber) const {
    int index = findRoomIndex(roomNumber);
    if (index == -1) {
        return nullptr;
    }

    return &rooms[index];
}

std::size_t Hotel::getRoomCount() const {
    return static_cast<std::size_t>(roomCount);
}

bool Hotel::setRoomStatus(int roomNumber, const string& newStatus) {
    const int index = findRoomIndex(roomNumber);
    if (index == -1) {
        return false;
    }
    rooms[index].setStatus(newStatus);
    return true;
}

bool Hotel::updateRoomStatus(int roomNumber, string newStatus) {
    if (!setRoomStatus(roomNumber, newStatus)) {
        cout << "Room not found.\n";
        return false;
    }
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
