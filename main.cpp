#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>

#include "Hotel.h"
#include "CustomerLinkedList.h"
#include "ReservationLinkedList.h"
#include "ActionStack.h"
#include "WaitingQueue.h"

using namespace std;

namespace {

bool readInputLine(const string& prompt, string& value) {
    cout << prompt;
    return static_cast<bool>(getline(cin, value));
}

bool readNonEmptyLine(const string& prompt, string& value) {
    while (readInputLine(prompt, value)) {
        if (value.find_first_not_of(" \t\r\n") != string::npos) {
            return true;
        }

        cout << "Input cannot be empty. Please try again.\n";
    }

    return false;
}

bool readInteger(const string& prompt, int& value) {
    while (true) {
        string input;
        if (!readInputLine(prompt, input)) {
            return false;
        }

        istringstream stream(input);
        int parsedValue;
        if (stream >> parsedValue) {
            stream >> ws;
            if (stream.eof()) {
                value = parsedValue;
                return true;
            }
        }

        cout << "Invalid input. Please enter a whole number.\n";
    }
}

bool readPositiveInteger(const string& prompt, int& value) {
    while (readInteger(prompt, value)) {
        if (value > 0) {
            return true;
        }

        cout << "Please enter a number greater than zero.\n";
    }

    return false;
}

bool readPositiveDouble(const string& prompt, double& value) {
    while (true) {
        string input;
        if (!readInputLine(prompt, input)) {
            return false;
        }

        istringstream stream(input);
        double parsedValue;
        if (stream >> parsedValue) {
            stream >> ws;
            if (stream.eof() && isfinite(parsedValue) && parsedValue > 0.0) {
                value = parsedValue;
                return true;
            }
        }

        cout << "Invalid input. Please enter a positive, finite number.\n";
    }
}

bool readRoomType(string& roomType) {
    while (readNonEmptyLine(
        "Enter Room Type (Single/Double/Deluxe): ", roomType)) {
        if (roomType == "Single" || roomType == "Double" ||
            roomType == "Deluxe") {
            return true;
        }

        cout << "Invalid room type. Enter Single, Double, or Deluxe.\n";
    }

    return false;
}

bool readRoomStatus(string& status) {
    while (readNonEmptyLine(
        "Enter Room Status (Available/Occupied/Maintenance): ", status)) {
        if (status == "Available" || status == "Occupied" ||
            status == "Maintenance") {
            return true;
        }

        cout << "Invalid room status. Enter Available, Occupied, or Maintenance.\n";
    }

    return false;
}

bool getDateOrdinal(const string& date, int& ordinal) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;

    for (size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (date[i] < '0' || date[i] > '9') {
            return false;
        }

        int digit = date[i] - '0';
        if (i < 4) {
            year = year * 10 + digit;
        } else if (i < 7) {
            month = month * 10 + digit;
        } else {
            day = day * 10 + digit;
        }
    }

    if (year == 0 || month < 1 || month > 12) {
        return false;
    }

    bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int daysInMonth[] = {31, isLeapYear ? 29 : 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
    if (day < 1 || day > daysInMonth[month - 1]) {
        return false;
    }

    int previousYear = year - 1;
    ordinal = previousYear * 365 + previousYear / 4 -
              previousYear / 100 + previousYear / 400;
    for (int i = 0; i < month - 1; ++i) {
        ordinal += daysInMonth[i];
    }
    ordinal += day;

    return true;
}

bool readValidDateRange(
    string& checkInDate,
    string& checkOutDate,
    int& nights
) {
    while (true) {
        if (!readNonEmptyLine("Enter Check-in Date (YYYY-MM-DD): ", checkInDate) ||
            !readNonEmptyLine("Enter Check-out Date (YYYY-MM-DD): ", checkOutDate)) {
            return false;
        }

        int checkInOrdinal;
        int checkOutOrdinal;
        if (!getDateOrdinal(checkInDate, checkInOrdinal) ||
            !getDateOrdinal(checkOutDate, checkOutOrdinal)) {
            cout << "Invalid date. Enter real dates in YYYY-MM-DD format.\n";
            continue;
        }

        if (checkOutOrdinal <= checkInOrdinal) {
            cout << "Check-out date must be after the check-in date.\n";
            continue;
        }

        nights = checkOutOrdinal - checkInOrdinal;
        return true;
    }
}

bool readMatchingNights(int expectedNights, int& nights) {
    while (readPositiveInteger("Enter Number of Nights: ", nights)) {
        if (nights == expectedNights) {
            return true;
        }

        cout << "Number of nights must match the date range ("
             << expectedNights << ").\n";
    }

    return false;
}

bool readPaymentMethod(string& method) {
    while (readNonEmptyLine("Payment Method (Cash/Card): ", method)) {
        if (method == "Cash" || method == "Card") {
            return true;
        }

        cout << "Invalid payment method. Enter Cash or Card.\n";
    }

    return false;
}

bool readPaymentDate(string& date) {
    while (readNonEmptyLine("Payment Date (YYYY-MM-DD): ", date)) {
        int ordinal;
        if (getDateOrdinal(date, ordinal)) {
            return true;
        }

        cout << "Invalid date. Enter a real date in YYYY-MM-DD format.\n";
    }

    return false;
}

}

int main() {
    Hotel myHotel;
    CustomerLinkedList customers;
    ReservationLinkedList reservations;
    ActionStack actionHistory;
    WaitingQueue waitingList;
    int nextPaymentId = 1;

    bool running = true;
    bool inputEnded = false;

    while (running) {
        int choice;

        cout << "\n====================================\n";
        cout << " HOTEL MANAGEMENT SYSTEM\n";
        cout << "====================================\n";
        cout << "1. Add Room\n";
        cout << "2. Display All Rooms\n";
        cout << "3. Register Customer\n";
        cout << "4. Display All Customers\n";
        cout << "5. Create Reservation\n";
        cout << "6. Display All Reservations\n";
        cout << "7. Add Customer to Waiting Queue\n";
        cout << "8. Display Action History\n";
        cout << "9. Exit\n";

        if (!readInteger("\nEnter your choice: ", choice)) {
            cout << "\nInput ended. Exiting.\n";
            break;
        }

        switch (choice) {
            case 1: {
                int roomNumber;
                double roomPrice;
                string roomType;
                string roomStatus;

                if (!readPositiveInteger("\nEnter Room Number: ", roomNumber) ||
                    !readRoomType(roomType) ||
                    !readPositiveDouble("Enter Price Per Night (Rs.): ", roomPrice) ||
                    !readRoomStatus(roomStatus)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                Room newRoom(roomNumber, roomType, roomPrice, roomStatus);
                if (myHotel.addRoom(newRoom)) {
                    cout << "\nRoom added successfully!\n";
                    actionHistory.push("Room " + to_string(roomNumber) + " added");
                } else {
                    cout << "\nRoom could not be added.\n";
                }

                break;
            }

            case 2: {
                cout << "\n========== ALL ROOMS ==========\n";
                myHotel.displayAllRooms();
                break;
            }

            case 3: {
                int customerId;
                string fullName;
                string phone;
                string email;
                string cnic;

                if (!readPositiveInteger("\nEnter Customer ID: ", customerId)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                if (customers.searchById(customerId) != nullptr) {
                    cout << "\nCustomer ID already exists. Customer was not registered.\n";
                    break;
                }

                if (!readNonEmptyLine("Enter Full Name: ", fullName) ||
                    !readNonEmptyLine("Enter Phone Number: ", phone) ||
                    !readNonEmptyLine("Enter Email: ", email) ||
                    !readNonEmptyLine("Enter CNIC: ", cnic)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                Customer newCustomer(customerId, fullName, phone, email, cnic);
                if (customers.insertCustomer(newCustomer)) {
                    cout << "\nCustomer registered successfully!\n";
                    actionHistory.push("Customer " + fullName + " registered");
                } else {
                    cout << "\nCustomer ID already exists. Customer was not registered.\n";
                }

                break;
            }

            case 4: {
                cout << "\n========== ALL CUSTOMERS ==========\n";
                customers.displayAllCustomers();
                break;
            }

            case 5: {
                int reservationAction;
                cout << "\n========== RESERVATION ACTIONS ==========\n";
                cout << "1. Create Reservation\n";
                cout << "2. Check In\n";
                cout << "3. Check Out\n";
                cout << "4. Record Payment\n";
                cout << "5. Back to Main Menu\n";

                if (!readInteger("\nEnter your choice: ", reservationAction)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                switch (reservationAction) {
                    case 1: {
                        int reservationId;
                        int customerId;
                        int roomNumber;
                        int numberOfNights;
                        int nightsFromDates;
                        string checkInDate;
                        string checkOutDate;

                        if (!readPositiveInteger("\nEnter Reservation ID: ", reservationId)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        if (reservations.searchById(reservationId) != nullptr) {
                            cout << "\nReservation ID already exists. "
                                    "Reservation was not created.\n";
                            break;
                        }

                        if (!readPositiveInteger("Enter Customer ID: ", customerId) ||
                            !readPositiveInteger("Enter Room Number: ", roomNumber)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        if (customers.searchById(customerId) == nullptr) {
                            cout << "\nCustomer ID not found. "
                                    "Reservation was not created.\n";
                            break;
                        }

                        const Room* room = myHotel.findRoom(roomNumber);
                        if (room == nullptr) {
                            cout << "\nRoom number not found. "
                                    "Reservation was not created.\n";
                            break;
                        }

                        if (!readValidDateRange(checkInDate, checkOutDate, nightsFromDates)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        if (reservations.hasOverlappingReservation(
                                roomNumber, checkInDate, checkOutDate)) {
                            cout << "\nRoom is already reserved for overlapping dates. "
                                    "Reservation was not created.\n";
                            break;
                        }

                        if (!readMatchingNights(nightsFromDates, numberOfNights)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        double pricePerNight = room->getPricePerNight();
                        double totalCost = pricePerNight * numberOfNights;
                        if (!isfinite(totalCost) || totalCost <= 0.0) {
                            cout << "\nReservation total is outside the supported "
                                    "numeric range. Reservation was not created.\n";
                            break;
                        }

                        Reservation newReservation(
                            reservationId,
                            customerId,
                            roomNumber,
                            checkInDate,
                            checkOutDate,
                            numberOfNights,
                            pricePerNight
                        );

                        if (reservations.insertReservation(newReservation)) {
                            cout << "\nReservation created successfully!\n";
                            actionHistory.push(
                                "Reservation " + to_string(reservationId) + " created"
                            );
                        } else {
                            cout << "\nReservation ID already exists. "
                                    "Reservation was not created.\n";
                        }

                        break;
                    }

                    case 2: {
                        int reservationId;
                        if (!readPositiveInteger("\nEnter Reservation ID to check in: ",
                                                 reservationId)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        Reservation* reservation = reservations.searchById(reservationId);
                        if (reservation == nullptr) {
                            cout << "\nReservation not found. Check-in was not completed.\n";
                            break;
                        }

                        if (reservation->getStatus() != "Confirmed") {
                            cout << "\nOnly confirmed reservations can be checked in.\n";
                            break;
                        }

                        const Room* room = myHotel.findRoom(reservation->getRoomNumber());
                        if (room == nullptr) {
                            cout << "\nRoom not found. Check-in was not completed.\n";
                            break;
                        }

                        if (!room->isAvailable()) {
                            cout << "\nRoom is not available. Check-in was not completed.\n";
                            break;
                        }

                        if (myHotel.updateRoomStatus(
                                reservation->getRoomNumber(), "Occupied")) {
                            reservation->setStatus("CheckedIn");
                            cout << "Check-in completed successfully.\n";
                            actionHistory.push(
                                "Reservation " + to_string(reservationId) + " checked in"
                            );
                        } else {
                            cout << "Check-in was not completed because the room "
                                    "could not be updated.\n";
                        }

                        break;
                    }

                    case 3: {
                        int reservationId;
                        if (!readPositiveInteger("\nEnter Reservation ID to check out: ",
                                                 reservationId)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        Reservation* reservation = reservations.searchById(reservationId);
                        if (reservation == nullptr) {
                            cout << "\nReservation not found. Check-out was not completed.\n";
                            break;
                        }

                        if (reservation->getStatus() != "CheckedIn") {
                            cout << "\nReservation must be checked in before check-out.\n";
                            break;
                        }

                        const Room* room = myHotel.findRoom(reservation->getRoomNumber());
                        if (room == nullptr) {
                            cout << "\nRoom not found. Check-out was not completed.\n";
                            break;
                        }

                        if (room->getStatus() != "Occupied") {
                            cout << "\nRoom status is inconsistent. Check-out was not completed.\n";
                            break;
                        }

                        cout << fixed << setprecision(2)
                             << "\nCheckout total for Reservation #"
                             << reservationId << ": Rs."
                             << reservation->getTotalCost() << endl;
                        cout << "Payment status: "
                             << (reservation->hasPayment() ? "Paid" : "Unpaid")
                             << endl;

                        if (myHotel.updateRoomStatus(
                                reservation->getRoomNumber(), "Available")) {
                            reservation->setStatus("CheckedOut");
                            cout << "Check-out completed successfully.\n";
                            actionHistory.push(
                                "Reservation " + to_string(reservationId) + " checked out"
                            );

                            if (waitingList.isEmpty()) {
                                cout << "Waiting queue is empty; no notification was sent.\n";
                            } else {
                                string waitingCustomer = waitingList.dequeue();
                                cout << "Waiting customer notified: " << waitingCustomer
                                     << ". Room #"
                                     << reservation->getRoomNumber()
                                     << " is available.\n";
                                cout << "Customer confirmation is required; "
                                        "no reservation was created.\n";
                                actionHistory.push(
                                    "Waiting customer " + waitingCustomer +
                                    " notified about available room " +
                                    to_string(reservation->getRoomNumber())
                                );
                                cout << "\nRemaining waiting customers:\n";
                                waitingList.display();
                            }
                        } else {
                            cout << "Check-out was not completed because the room "
                                    "could not be updated.\n";
                        }

                        break;
                    }

                    case 4:
                    {
                        int reservationId;
                        if (!readPositiveInteger("\nEnter Reservation ID for payment: ",
                                                 reservationId)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        Reservation* reservation = reservations.searchById(reservationId);
                        if (reservation == nullptr) {
                            cout << "\nReservation not found. Payment was not recorded.\n";
                            break;
                        }

                        if (reservation->hasPayment()) {
                            cout << "\nA payment is already recorded for this reservation.\n";
                            break;
                        }

                        double amount;
                        string method;
                        string paymentDate;
                        if (!isfinite(reservation->getTotalCost()) ||
                            reservation->getTotalCost() <= 0.0) {
                            cout << "\nReservation total is invalid. Payment was not recorded.\n";
                            break;
                        }

                        if (!readPositiveDouble("\nEnter Payment Amount (Rs.): ", amount) ||
                            !readPaymentMethod(method) ||
                            !readPaymentDate(paymentDate)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }

                        Payment payment(
                            nextPaymentId,
                            reservationId,
                            amount,
                            method,
                            paymentDate
                        );

                        if (!reservation->recordPayment(payment)) {
                            cout << fixed << setprecision(2)
                                 << "\nPayment was not recorded. The amount must match "
                                    "the reservation total of Rs."
                                 << reservation->getTotalCost() << ".\n";
                            break;
                        }

                        cout << "\nPayment recorded successfully.\n";
                        payment.displayReceipt();
                        actionHistory.push(
                            "Payment " + to_string(nextPaymentId) +
                            " recorded for reservation " + to_string(reservationId)
                        );
                        ++nextPaymentId;
                        break;
                    }

                    case 5:
                        break;

                    default:
                        cout << "\nInvalid reservation action. Please select 1 to 5.\n";
                        break;
                }

                break;
            }

            case 6: {
                cout << "\n========== ALL RESERVATIONS ==========\n";
                reservations.displayAllReservations();
                break;
            }

            case 7: {
                string customerName;
                if (!readNonEmptyLine("\nEnter Customer Name: ", customerName)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                waitingList.enqueue(customerName);
                cout << "\nCustomer added to waiting queue!\n";
                cout << "\n========== WAITING QUEUE ==========\n";
                waitingList.display();
                actionHistory.push(customerName + " added to waiting queue");

                break;
            }

            case 8: {
                cout << "\n========== ACTION HISTORY ==========\n";
                actionHistory.display();
                break;
            }

            case 9: {
                cout << "\nThank you for using our system!\n";
                running = false;
                break;
            }

            default: {
                cout << "\nInvalid choice. Please select 1 to 9.\n";
                break;
            }
        }

        if (inputEnded) {
            cout << "\nInput ended. Exiting.\n";
        }
    }

    return 0;
}