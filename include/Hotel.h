
#ifndef HOTEL_H
#define HOTEL_H

#include "Room.h"
#include <string>
#include <vector>

class Hotel {
private:
    Room rooms[50];
    int roomCount;

public:
    Hotel();

    bool addRoom(Room room);
    bool canAddRoom(int roomNumber) const;
    bool loadRooms(const std::vector<Room>& loadedRooms, std::string& error);
    int findRoomIndex(int roomNumber) const;
    const Room* findRoom(int roomNumber) const;
    std::size_t getRoomCount() const;
    std::vector<Room> getAllRooms() const;
    bool setRoomStatus(int roomNumber, const std::string& newStatus);
    bool updateRoomStatus(int roomNumber, string newStatus);
    void displayAllRooms() const;
};

#endif
