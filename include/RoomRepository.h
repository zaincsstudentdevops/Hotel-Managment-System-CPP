#ifndef ROOMREPOSITORY_H
#define ROOMREPOSITORY_H

#include "Database.h"
#include "Room.h"

#include <string>
#include <vector>

class RoomRepository {
public:
    explicit RoomRepository(Database& database);

    bool loadRooms(std::vector<Room>& rooms, std::string& error) const;
    bool insertRoom(const Room& room, std::string& error) const;
    bool updateStatus(
        int roomNumber,
        const std::string& status,
        std::string& error
    ) const;
    bool transitionStatus(
        int roomNumber,
        const std::string& expectedStatus,
        const std::string& newStatus,
        std::string& error
    ) const;
    bool lockRoomStatus(
        int roomNumber,
        std::string& status,
        bool& found,
        std::string& error
    ) const;

private:
    Database& database;
};

#endif
