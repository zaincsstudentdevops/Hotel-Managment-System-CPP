# Hotel Management System (C++)

A Windows console application for managing hotel rooms, customers, reservations, payments, and a waiting list. The project demonstrates object-oriented C++ and manually implemented data structures. It is an in-memory learning project; information is not saved after the program exits.

## Technologies

- C++
- C++17
- GCC `g++` on Windows (the project setup targets MSYS2 UCRT64)
- VS Code can be used as the editor

No database, server, or external library is required.

## Features

- **Room management:** Add and display rooms. The `Hotel` class stores up to 50 rooms in a fixed-size array, rejects duplicate room numbers, and updates room status.
- **Customer management:** Register and display customers. Duplicate customer IDs are rejected.
- **Reservation management:** Create and display reservations. Customers and rooms must exist, reservation IDs must be unique, and dates and number of nights are validated.
- **Booking conflict prevention:** Reservations for the same room cannot overlap. A checkout date may coincide with the next reservation's check-in date.
- **Check-in and check-out:** Check-in requires a confirmed reservation and an available room. Check-out requires a checked-in reservation and marks its room available.
- **Payments and receipts:** Record one full payment per reservation. The amount must match the reservation total; receipts show the reservation, amount, payment method, date, and status.
- **Waiting queue:** Add customer names to a manually implemented FIFO queue. After a successful checkout, the first waiting name is notified that the room is available. The program does not create a reservation automatically.
- **Action history:** A manually implemented LIFO stack records actions such as room and customer additions, reservation creation, payment, check-in, check-out, and waiting-list notifications.
- **Input handling:** Numeric and required text input is validated. Invalid input is reported, and end-of-file exits safely.

## Project structure

```text
Hotel-Management-System-CPP/
├── .gitignore
├── README.md
├── main.cpp
├── include/
│   ├── ActionStack.h
│   ├── Customer.h
│   ├── CustomerLinkedList.h
│   ├── Hotel.h
│   ├── Payment.h
│   ├── Reservation.h
│   ├── ReservationLinkedList.h
│   ├── Room.h
│   └── WaitingQueue.h
└── src/
    ├── ActionStack.cpp
    ├── Customer.cpp
    ├── CustomerLinkedList.cpp
    ├── Hotel.cpp
    ├── Payment.cpp
    ├── Reaervation.cpp
    ├── ReservationLinkList.cpp
    ├── Room.cpp
    └── WaitingQueue.cpp
```

The source filenames `Reaervation.cpp` and `ReservationLinkList.cpp` are spelled as they currently exist in the project. Local ignored build artifacts may also be present in the project folder: `Customer.o`, `Room.o`, and `hotel_app.exe`.

## Main classes and data structures

### Core classes

- **`Room`** stores a room number, category, price per night, and status (`Available`, `Occupied`, or `Maintenance`).
- **`Hotel`** manages a fixed array of up to 50 rooms, room lookup, display, and status updates.
- **`Customer`** stores a customer's ID, name, phone number, email, and identity document.
- **`Reservation`** stores the customer and room IDs, check-in/check-out dates, number of nights, total cost, reservation status, and an optional payment.
- **`Payment`** stores the payment and reservation IDs, amount, method, date, and payment status, and can display a receipt.

### Manually implemented data structures

- **`CustomerLinkedList`** stores customers in a singly linked list. It supports insertion, search by ID or name, deletion, and display.
- **`ReservationLinkedList`** stores reservations in a singly linked list. It supports insertion, search, deletion, display, and checking for overlapping room bookings.
- **`ActionStack`** is a linked-node LIFO stack for action history.
- **`WaitingQueue`** is a linked-node FIFO queue that stores customer names and removes the earliest entry when a room becomes available.

The pointer-owning structures release their nodes in their destructors and disable copying to avoid shallow-copy ownership errors.

## Build and run on Windows

Open a terminal in the project root with GCC `g++` available (the project setup targets the MSYS2 UCRT64 environment), then run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude main.cpp src/*.cpp -o hotel_app.exe
.\hotel_app.exe
```

The program displays nine top-level menu options. Option 5 opens a submenu for creating reservations, checking in, checking out, recording payments, or returning to the main menu.

## Important validation and booking rules

- Room numbers, customer IDs, and reservation IDs must be positive; duplicate room, customer, and reservation IDs are rejected.
- Room categories are `Single`, `Double`, and `Deluxe`; room statuses are `Available`, `Occupied`, and `Maintenance`.
- Reservations require an existing customer and room.
- Dates must be real dates in `YYYY-MM-DD` format, checkout must be after check-in, and the number of nights must match the date range.
- A room cannot have overlapping reservations. Checkout/check-in boundary dates are allowed. Cancelled reservations are excluded from conflict checks; checked-out reservations continue to reserve their original date range.
- Check-in is allowed only for a confirmed reservation whose room is available. Check-out is allowed only after check-in and updates the room to available.
- Payment must be a positive, finite amount matching the reservation total. Only `Cash` and `Card` are accepted, and a reservation cannot have a second payment recorded.
- A checkout notification removes and notifies the first waiting-queue entry. The customer must confirm separately; the program does not book the room on their behalf.

## Current limitations

- **In-memory data:** Rooms, customers, reservations, payments, action history, and waiting-list entries are lost when the program exits.
- **Console notifications:** Waiting-list notifications are printed in the console; no email, SMS, or other external notification is sent.
- **No current-date enforcement:** Reservation dates are validated as calendar dates and checked for conflicts, but check-in/check-out dates are not automatically compared with today's date.
