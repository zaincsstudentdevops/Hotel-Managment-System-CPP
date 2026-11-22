
#ifndef HOTEL_H
#define HOTEL_H

#include "Room.h"

class Hotel {
private:
    Room rooms[50];
    int roomCount;

public:
    Hotel();

    bool addRoom(Room room);
    int findRoomIndex(int roomNumber) const;
    bool updateRoomStatus(int roomNumber, string newStatus);
    void displayAllRooms() const;
};

#endif
