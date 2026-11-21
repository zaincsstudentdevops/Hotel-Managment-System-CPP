#ifndef ROOM_H
#define ROOM_H

#include <string>
using namespace std;

class Room {
private:
    int roomNumber;
    string category;       // "Single", "Double", or "Deluxe"
    double pricePerNight;
    string status;         // "Available", "Occupied", or "Maintenance"

public:
    Room();
    Room(int number, string cat, double price, string stat = "Available");

    int getRoomNumber() const;
    string getCategory() const;
    double getPricePerNight() const;
    string getStatus() const;

    void setCategory(string cat);
    void setPricePerNight(double price);
    void setStatus(string stat);
    bool isAvailable() const;
    void displayRoom() const;
};

#endif
