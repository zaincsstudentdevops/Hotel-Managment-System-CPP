#include "Room.h"
#include <iostream>
#include <iomanip>

Room::Room() : roomNumber(0), category("Single"), pricePerNight(0.0), status("Available") {}

Room::Room(int number, string cat, double price, string stat)
    : roomNumber(number), category(cat), pricePerNight(price), status(stat) {}

int Room::getRoomNumber() const { return roomNumber; }
string Room::getCategory() const { return category; }
double Room::getPricePerNight() const { return pricePerNight; }
string Room::getStatus() const { return status; }

void Room::setCategory(string cat) { category = cat; }
void Room::setPricePerNight(double price) { pricePerNight = price; }
void Room::setStatus(string stat) { status = stat; }
bool Room::isAvailable() const { return status == "Available"; }

void Room::displayRoom() const {
    cout << "Room #" << roomNumber
         << " | Category: " << setw(8) << left << category
         << " | Rate: Rs." << fixed << setprecision(2) << pricePerNight
         << " | Status: " << status << endl;
}
