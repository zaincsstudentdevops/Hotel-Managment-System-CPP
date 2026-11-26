#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>

#include "Hotel.h"
#include "CustomerLinkedList.h"
#include "HotelBackend.h"
#include "Database.h"
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
        "Enter Room Status (Available): ", status)) {
        if (status == "Available") {
            return true;
        }

        cout << "New rooms must start as Available.\n";
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

void displayCustomerDetails(const Customer& customer) {
    cout << "Customer ID: " << customer.getCustomerId() << '\n'
         << "Full Name: " << customer.getFullName() << '\n'
         << "Phone Number: " << customer.getPhoneNumber() << '\n'
         << "Email: " << customer.getEmail() << '\n'
         << "Identity Document: " << customer.getIdentityDocument() << '\n';
}

}

int main() {
    Database database;
    if (!database.connect()) {
        cerr << "Application startup aborted because MariaDB is unavailable.\n";
        return 1;
    }

    Hotel myHotel;
    CustomerLinkedList customers;
    ReservationLinkedList reservations;
    ActionStack actionHistory;
    WaitingQueue waitingList;
    HotelBackend backend(
        database, myHotel, customers, reservations, waitingList, actionHistory
    );
    string startupError;
    if (!backend.loadAll(startupError)) {
        cerr << "Application startup aborted. " << startupError << '\n';
        return 1;
    }
    cout << "Loaded " << customers.size() << " customer(s), "
         << myHotel.getRoomCount() << " room(s), "
         << reservations.size() << " reservation(s), "
         << waitingList.size() << " waiting-list entry/entries, and action "
         << "history from MariaDB.\n";

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
        cout << "10. Search Customer\n";
        cout << "11. Update Customer\n";
        cout << "12. Delete Customer\n";

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
                string error;
                if (backend.addRoom(newRoom, error)) {
                    cout << "\nRoom added successfully!\n";
                } else {
                    cout << "\nRoom could not be added: " << error << '\n';
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
                string error;
                if (backend.registerCustomer(newCustomer, error)) {
                    cout << "\nCustomer registered successfully!\n";
                } else {
                    cout << "\nCustomer was not registered: " << error << '\n';
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
                cout << "6. Cancel Reservation\n";
                string error;

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

                        const ReservationCreateResult createResult =
                            backend.createReservation(newReservation, error);
                        if (createResult == ReservationCreateResult::created) {
                            cout << "\nReservation created successfully!\n";
                        } else {
                            cout << "\nReservation was not created: "
                                 << error << '\n';
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
                        const Room* room =
                            myHotel.findRoom(reservation->getRoomNumber());
                        if (room == nullptr) {
                            cout << "\nRoom not found. Check-in was not completed.\n";
                            break;
                        }
                        if (!room->isAvailable()) {
                            cout << "\nRoom is not available. Check-in was not completed.\n";
                            break;
                        }

                        if (backend.checkIn(reservationId, error)) {
                            cout << "Check-in completed successfully.\n";
                        } else {
                            cout << "Check-in was not completed: " << error << '\n';
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
                        const Room* room =
                            myHotel.findRoom(reservation->getRoomNumber());
                        if (room == nullptr) {
                            cout << "\nRoom not found. Check-out was not completed.\n";
                            break;
                        }
                        if (room->getStatus() != "Occupied") {
                            cout << "\nRoom status is inconsistent. "
                                    "Check-out was not completed.\n";
                            break;
                        }

                        cout << fixed << setprecision(2)
                             << "\nCheckout total for Reservation #"
                             << reservationId << ": Rs."
                             << reservation->getTotalCost() << endl;
                        cout << "Payment status: "
                             << (reservation->hasPayment() ? "Paid" : "Unpaid")
                             << endl;

                        string notifiedCustomer;
                        if (backend.checkOut(
                                reservationId, notifiedCustomer, error)) {
                            cout << "Check-out completed successfully.\n";
                            if (notifiedCustomer.empty()) {
                                cout << "Waiting queue is empty; no notification was sent.\n";
                            } else {
                                cout << "Waiting customer notified: "
                                     << notifiedCustomer
                                     << ". Room #"
                                     << reservation->getRoomNumber()
                                     << " is available.\n";
                                cout << "Customer confirmation is required; "
                                        "no reservation was created.\n";
                                cout << "\nRemaining waiting customers:\n";
                                waitingList.display();
                            }
                        } else {
                            cout << "Check-out was not completed: " << error << '\n';
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

                        Payment payment;
                        if (!backend.recordPayment(
                                reservationId, amount, method, paymentDate,
                                payment, error)) {
                            cout << fixed << setprecision(2)
                                 << "\nPayment was not recorded. The amount must match "
                                    "the reservation total of Rs."
                                 << reservation->getTotalCost() << ".\n";
                            cout << error << '\n';
                            break;
                        }

                        cout << "\nPayment recorded successfully.\n";
                        payment.displayReceipt();
                        break;
                    }

                    case 5:
                        break;

                    case 6: {
                        int reservationId;
                        if (!readPositiveInteger(
                                "\nEnter Reservation ID to cancel: ",
                                reservationId)) {
                            running = false;
                            inputEnded = true;
                            break;
                        }
                        if (backend.cancelReservation(reservationId, error)) {
                            cout << "Reservation cancelled successfully.\n";
                        } else {
                            cout << "Reservation was not cancelled: "
                                 << error << '\n';
                        }
                        break;
                    }

                    default:
                        cout << "\nInvalid reservation action. Please select 1 to 6.\n";
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

                string error;
                if (!backend.enqueueWaitingCustomer(customerName, error)) {
                    cout << "\nCustomer was not added to the waiting queue: "
                         << error << '\n';
                    break;
                }
                cout << "\nCustomer added to waiting queue!\n";
                cout << "\n========== WAITING QUEUE ==========\n";
                waitingList.display();

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

            case 10: {
                int customerId;
                if (!readPositiveInteger("\nEnter Customer ID to search: ",
                                         customerId)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                Customer customer;
                string error;
                const CustomerLookupStatus status =
                    backend.findCustomerById(
                        customerId, customer, error
                    );
                if (status == CustomerLookupStatus::found) {
                    displayCustomerDetails(customer);
                } else if (status == CustomerLookupStatus::notFound) {
                    cout << "\nCustomer ID was not found.\n";
                } else {
                    cout << "\nCustomer search failed: " << error << '\n';
                }
                break;
            }

            case 11: {
                int customerId;
                if (!readPositiveInteger("\nEnter Customer ID to update: ",
                                         customerId)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                Customer existingCustomer;
                string error;
                const CustomerLookupStatus lookupStatus =
                    backend.findCustomerById(
                        customerId, existingCustomer, error
                    );
                if (lookupStatus == CustomerLookupStatus::notFound) {
                    cout << "\nCustomer ID was not found; no update was made.\n";
                    break;
                }
                if (lookupStatus == CustomerLookupStatus::error) {
                    cout << "\nCustomer lookup failed: " << error << '\n';
                    break;
                }

                cout << "\nCurrent customer:\n";
                displayCustomerDetails(existingCustomer);

                string fullName;
                string phone;
                string email;
                string identityDocument;
                if (!readNonEmptyLine("Enter new Full Name: ", fullName) ||
                    !readNonEmptyLine("Enter new Phone Number: ", phone) ||
                    !readNonEmptyLine("Enter new Email: ", email) ||
                    !readNonEmptyLine(
                        "Enter new Identity Document: ", identityDocument
                    )) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                Customer updatedCustomer(
                    customerId, fullName, phone, email, identityDocument
                );
                const CustomerUpdateStatus updateStatus =
                    backend.updateCustomer(updatedCustomer, error);
                if (updateStatus == CustomerUpdateStatus::updated) {
                    cout << "\nCustomer updated successfully.\n";
                } else if (updateStatus == CustomerUpdateStatus::unchanged) {
                    cout << "\nNo customer fields changed.\n";
                } else if (updateStatus == CustomerUpdateStatus::notFound) {
                    cout << "\nCustomer was not found in MariaDB; no update was made.\n";
                } else {
                    cout << "\nCustomer update failed: " << error << '\n';
                }
                break;
            }

            case 12: {
                int customerId;
                if (!readPositiveInteger("\nEnter Customer ID to delete: ",
                                         customerId)) {
                    running = false;
                    inputEnded = true;
                    break;
                }

                string confirmation;
                if (!readNonEmptyLine(
                        "Permanently delete this customer? (y/n): ",
                        confirmation
                    )) {
                    running = false;
                    inputEnded = true;
                    break;
                }
                if (confirmation != "y" && confirmation != "Y") {
                    cout << "\nCustomer deletion cancelled.\n";
                    break;
                }

                string error;
                const CustomerDeleteStatus status =
                    backend.deleteCustomer(customerId, error);
                if (status == CustomerDeleteStatus::deleted) {
                    cout << "\nCustomer deleted successfully.\n";
                } else if (status == CustomerDeleteStatus::notFound) {
                    cout << "\nCustomer ID was not found; nothing was deleted.\n";
                } else if (status == CustomerDeleteStatus::hasReservations) {
                    cout << "\nCustomer deletion restricted: " << error << '\n';
                } else {
                    cout << "\nCustomer deletion failed: " << error << '\n';
                }
                break;
            }

            default: {
                cout << "\nInvalid choice. Please select 1 to 12.\n";
                break;
            }
        }

        if (inputEnded) {
            cout << "\nInput ended. Exiting.\n";
        }
    }

    return 0;
}