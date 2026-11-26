# Hotel Management System (C++)

A Windows console application for managing hotel rooms, customers, reservations, payments, and a waiting list. The project demonstrates object-oriented C++ and manually implemented data structures. All six application data areas are persisted in MariaDB and loaded before the console menu is displayed.

## Technologies

- C++
- C++17
- GCC `g++` on Windows (the project setup targets MSYS2 UCRT64)
- MariaDB Connector/C 3.4.11
- XAMPP MariaDB
- VS Code can be used as the editor

MariaDB and MariaDB Connector/C are required to build and run the integrated application.

## Features

- **Room management:** Add and display rooms. The `Hotel` class stores up to 50 rooms in a fixed-size array, rejects duplicate room numbers, and persists room status.
- **Customer management:** Load, register, search, update, display, and delete customers in MariaDB. Duplicate customer IDs are rejected, and customers with reservations cannot be deleted.
- **Reservation management:** Create, display, and cancel persisted reservations. Customers and rooms must exist, reservation IDs must be unique, and dates and number of nights are validated.
- **Booking conflict prevention:** Transactions lock the room row before checking for overlapping reservations and inserting a booking, preventing concurrent application bookings from overlapping. A checkout date may coincide with the next reservation's check-in date.
- **Check-in and check-out:** Reservation and room statuses are updated in one transaction. Checkout also removes the oldest waiting-list entry and records the notification atomically.
- **Payments and receipts:** Record one full payment per reservation. The database supplies payment IDs, and a unique reservation constraint prevents duplicate payments.
- **Waiting queue:** Add customer names to a persisted FIFO queue backed by the existing linked-node queue. After a successful checkout, the first waiting name is notified that the room is available. The program does not create a reservation automatically.
- **Action history:** Persist actions and load them into the existing LIFO stack, preserving newest-first display order.
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
│   ├── CustomerRepository.h
│   ├── Database.h
│   ├── DatabaseStatement.h
│   ├── DatabaseTransaction.h
│   ├── HotelBackend.h
│   ├── Hotel.h
│   ├── ActionHistoryRepository.h
│   ├── Payment.h
│   ├── PaymentRepository.h
│   ├── Reservation.h
│   ├── ReservationLinkedList.h
│   ├── ReservationRepository.h
│   ├── Room.h
│   ├── RoomRepository.h
│   ├── WaitingListRepository.h
│   └── WaitingQueue.h
├── src/
│   ├── ActionStack.cpp
│   ├── Customer.cpp
│   ├── CustomerLinkedList.cpp
│   ├── CustomerRepository.cpp
│   ├── Database.cpp
│   ├── DatabaseTransaction.cpp
│   ├── HotelBackend.cpp
│   ├── ActionHistoryRepository.cpp
│   ├── Hotel.cpp
│   ├── Payment.cpp
│   ├── PaymentRepository.cpp
│   ├── Reaervation.cpp
│   ├── ReservationLinkList.cpp
│   ├── ReservationRepository.cpp
│   ├── Room.cpp
│   ├── RoomRepository.cpp
│   ├── WaitingListRepository.cpp
│   └── WaitingQueue.cpp
└── database/
    ├── schema.sql
    ├── connection_test.cpp
    ├── database_query_test.cpp
    ├── customer_crud_test.cpp
    ├── customer_integration_test.cpp
    └── backend_integration_test.cpp
```

The source filenames `Reaervation.cpp` and `ReservationLinkList.cpp` are spelled as they currently exist in the project. Generated executable and object files are ignored by Git.

## Main classes and data structures

### Core classes

- **`Room`** stores a room number, category, price per night, and status (`Available`, `Occupied`, or `Maintenance`).
- **`Hotel`** manages a fixed array of up to 50 rooms, room lookup, display, and status updates.
- **`Customer`** stores a customer's ID, name, phone number, email, and identity document.
- **`CustomerRepository`** loads and persists customers through the existing `Database` connection and synchronizes the in-memory customer list after successful database operations.
- **Domain repositories** keep room, reservation, payment, waiting-list, and action-history SQL out of the console menu.
- **`HotelBackend`** coordinates startup loading and multi-table transactions. In-memory objects are updated after database commits.
- **`Reservation`** stores the customer and room IDs, check-in/check-out dates, number of nights, total cost, reservation status, and an optional payment.
- **`Payment`** stores the payment and reservation IDs, amount, method, date, and payment status, and can display a receipt.

### Manually implemented data structures

- **`CustomerLinkedList`** stores customers in a singly linked list. It supports insertion, search by ID or name, deletion, and display.
- **`ReservationLinkedList`** stores reservations in a singly linked list. It supports insertion, search, deletion, display, and checking for overlapping room bookings.
- **`ActionStack`** is a linked-node LIFO stack for action history.
- **`WaitingQueue`** is a linked-node FIFO queue that stores customer names and removes the earliest entry when a room becomes available.

The pointer-owning structures release their nodes in their destructors and disable copying to avoid shallow-copy ownership errors.

## Build and run on Windows

Start the MariaDB service in XAMPP and make sure the `hotel_management` database and six tables from `database/schema.sql` exist. Install/configure MariaDB Connector/C for MSYS2 UCRT64. The application connects and loads customers, rooms, reservations, payments, waiting-list entries, and action history before showing the menu. Startup fails clearly if the connection, data validation, or 50-room capacity check fails.

The VS Code task **Build Hotel Management System** builds the application with the configured UCRT64 compiler and Connector/C library. To build manually from PowerShell in the project root:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude -IC:\msys64\ucrt64\include\mariadb main.cpp src\*.cpp -LC:\msys64\ucrt64\lib -lmariadb -o hotel_app_ucrt64.exe
.\hotel_app_ucrt64.exe
```

The original main-menu options remain available. Customer search, update, and delete are options 10–12. Option 5 opens a submenu for creating reservations, checking in, checking out, recording payments, returning to the main menu, or cancelling a confirmed reservation.

The separate VS Code tasks build the connection, query, CRUD, customer integration, and complete backend integration tests. The tests check temporary identifiers before insertion and verify that only their own temporary rows are removed. The backend integration test refuses to run its checkout/FIFO scenario if the persisted waiting list is nonempty, so it never consumes an existing queue entry.

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

- **Fixed room capacity:** The existing `Hotel` array supports at most 50 rooms; startup aborts rather than silently truncating a larger database.
- **Console notifications:** Waiting-list notifications are printed in the console; no email, SMS, or other external notification is sent.
- **No current-date enforcement:** Reservation dates are validated as calendar dates and checked for conflicts, but check-in/check-out dates are not automatically compared with today's date.
