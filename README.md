# Hotel Management System (C++)

A C++ university hotel management project for organizing hotel rooms, customers, reservations, payments, the waiting list, and hotel activity. The project has two application entry points:

- an original console application;
- a MariaDB-backed web dashboard served by a C++ HTTP backend.

Both interfaces use the shared C++ domain classes and `HotelBackend` data layer. The application loads persisted data from MariaDB, holds a snapshot in memory for each running process, and commits changes through the database repositories.

Repository: [zaincsstudentdevops/Hotel-Managment-System-CPP](https://github.com/zaincsstudentdevops/Hotel-Managment-System-CPP)

## Features

### Completed functionality

- **Room management**
  - Add rooms from the console or web dashboard.
  - Use room numbers, categories (`Single`, `Double`, `Deluxe`), nightly prices, and statuses (`Available`, `Occupied`, `Maintenance`).
  - Filter the web room directory by status.
- **Smart Room Finder (AI Recommendation)**
  - Search the existing room inventory by guest count, maximum nightly budget, optional category, requested dates, and availability.
  - Apply the configured category capacities: Single = 1 guest, Double = 2 guests, Deluxe = 2 guests.
  - Rank suitable rooms using Uniform Cost Search (UCS), with total stay cost as the path cost.
  - Select a recommendation to continue in the existing reservation form; no separate booking workflow is created.
  - Room facilities are not considered because the existing schema has no facilities data.
- **Customer management**
  - Register, search, display, edit, and delete customers.
  - Reject duplicate customer IDs.
  - Prevent deletion of a customer that has reservations.
- **Reservations**
  - Create and display persisted reservations.
  - Require a valid customer and room.
  - Validate positive IDs, real `YYYY-MM-DD` dates, date order, and stay length.
  - Prevent overlapping room reservations while allowing a checkout date to equal the next check-in date.
- **Check-in and check-out**
  - Check in confirmed reservations.
  - Check out checked-in reservations.
  - Cancel confirmed reservations.
  - Update reservation and room statuses using database transactions.
- **Payments**
  - Record one full payment for a reservation.
  - Accept `Cash` or `Card`.
  - Require a positive amount that matches the reservation total.
  - Display payment history and dashboard totals.
- **Waiting list**
  - Add guest names to a persisted FIFO waiting queue.
  - Checkout removes and notifies the oldest waiting guest.
  - The notification does not automatically create a reservation.
- **Dashboard**
  - View room occupancy, customer totals, reservation totals and statuses, and payment totals.
  - View recent reservations, room inventory, customer records, payment records, and waiting entries.
- **Database persistence**
  - Store rooms, customers, reservations, payments, waiting-list entries, and action history in MariaDB.
  - Load all persisted application data before an application process starts.
- **Original console interface**
  - Use `main.cpp` and the console menu independently from the web server.
  - Preserve the original menu, customer search/update/delete options, and reservation sub-menu.

## Technologies Used

- C++17
- Microsoft Windows
- MariaDB
- XAMPP for local MariaDB/phpMyAdmin
- MSYS2 UCRT64 GCC (`g++`)
- MariaDB Connector/C
- cpp-httplib 0.18.3
- nlohmann/json 3.12.0
- HTML, CSS, and JavaScript
- PowerShell build commands and launch scripts

The header-only `cpp-httplib` and `nlohmann/json` dependencies are included in `third_party/`; no package download is needed for those libraries.

## Project Structure

```text
Hotel-Management-System-CPP/
├── main.cpp                         # Original console application entry point
├── include/                         # C++ headers and domain/repository interfaces
├── src/                             # C++ implementation files
├── database/
│   ├── schema.sql                   # hotel_management database and table schema
│   ├── connection_test.cpp
│   ├── database_query_test.cpp
│   ├── customer_crud_test.cpp
│   ├── customer_integration_test.cpp
│   ├── backend_integration_test.cpp
│   └── smart_room_finder_test.cpp
├── web/
│   ├── BUILD.md                     # Verified web build and launcher instructions
│   ├── server_main.cpp              # C++ HTTP server entry point
│   └── http_api.cpp                 # JSON API route implementation
├── frontend/
│   ├── index.html                   # Web dashboard markup
│   ├── css/styles.css
│   ├── js/app.js
│   └── assets/                      # Decorative hotel and room-category images
├── third_party/
│   ├── cpp-httplib/                 # cpp-httplib header and license
│   └── nlohmann/                    # nlohmann/json header and license
├── launch-hotel-dashboard.ps1       # Start/reuse the web server and Chrome app
├── stop-hotel-dashboard.ps1         # Stop the server started by the launcher
├── create-hotel-dashboard-shortcut.ps1
└── hotel_*_ucrt64.exe               # Generated local executables, where built
```

The source filenames `Reaervation.cpp` and `ReservationLinkList.cpp` are spelled as they currently exist in the project.

### Important C++ components

- `Database` manages the MariaDB connection.
- `HotelBackend` coordinates startup loading and database-backed operations.
- `CustomerLinkedList`, `ReservationLinkedList`, `ActionStack`, and `WaitingQueue` are manually implemented in-memory data structures.
- Repository classes keep SQL operations separated from the domain and console classes.
- `Room`, `Customer`, `Reservation`, and `Payment` contain the hotel domain models.
- `SmartRoomFinder` applies the customer requirements and orders valid room goals with a minimum-cost priority queue.

## Smart Room Finder

The web dashboard provides **Smart Room Finder** in its navigation. It posts
the guest count, nightly budget, optional category, and requested dates to
`POST /api/smart-room-finder`. The C++ API calls `HotelBackend`, which passes
its current room and reservation snapshots to `SmartRoomFinder`.

The data model has no individual room capacity field. The finder therefore
uses the explicitly configured category capacities: Single accommodates one
guest; Double and Deluxe accommodate up to two guests. A room that cannot fit
the requested party is excluded. The project also has no facilities data, so
facility requirements are not offered.

### UCS formulation

- **Initial state:** the search starts with the customer's request and an empty
  frontier.
- **States:** each candidate state represents one room that meets every
  mandatory requirement: category (when selected), guest capacity, budget,
  available room status, and no overlapping reservation for the requested
  dates.
- **Transitions:** one direct transition from the initial state to each
  suitable room.
- **Path cost:** that room's price per night multiplied by the number of nights.
- **Goal:** a suitable room. The finder removes goals from a min-priority queue,
  so the lowest total stay cost is recommended first. Room number breaks cost
  ties consistently.

All rooms are direct successors of the initial state, so this is a small,
beginner-friendly UCS search rather than a different algorithm disguised as
search. It returns all suitable rooms in UCS cost order.

### Demonstration

1. Start MariaDB and run the web application as described below.
2. Ensure the Rooms page contains rooms with status **Available**. The existing
   database schema needs no update for this feature.
3. Open **Smart Room Finder**, enter dates, guest count, a maximum nightly
   budget, and optionally choose a category.
4. Choose **Find rooms**. Review the room number, category, price, capacity,
   total stay price, and recommendation explanation.
5. Choose **Use this room**. The existing reservation form opens with the
   recommended room and dates selected. Select a customer and reservation ID,
   then submit through the existing booking workflow.
6. Try three guests to see that no room matches the configured category
   capacities.

### UCS module test

From the project root, build and run the standalone test with MSYS2 UCRT64:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude database\smart_room_finder_test.cpp src\SmartRoomFinder.cpp src\Reaervation.cpp src\Room.cpp src\Payment.cpp -o smart_room_finder_test.exe
.\smart_room_finder_test.exe
```

The test covers a matching room, UCS ordering, budget/category/date conflicts,
invalid date ranges, and guest-capacity matches and no-match cases.

### Short viva explanation

“Uniform Cost Search stores the candidate rooms in a priority queue ordered by
accumulated cost. In this project each valid room is one goal reachable from
the start state, and its path cost is the full stay price. UCS always removes
the cheapest goal first, so recommendations are shown from lowest total cost
to highest while mandatory requirements are checked before a room enters the
frontier.”

## Database Setup on a New Computer

The repository does not guarantee that the MariaDB database already exists, and it does not include customer or reservation records. Importing `database/schema.sql` creates the database structure, but it does not import personal or test data.

1. Install XAMPP.
2. Open the XAMPP Control Panel.
3. Start the **MySQL** module. It provides the local MariaDB service used by the project.
4. Open phpMyAdmin, normally at `http://localhost/phpmyadmin`.
5. In phpMyAdmin, select the **Import** tab.
6. Choose `database/schema.sql` from the cloned repository.
7. Run the import.

The schema creates and selects a database named `hotel_management`, and creates the following six tables:

- `rooms`
- `customers`
- `reservations`
- `payments`
- `waiting_list`
- `action_history`

The C++ connection is configured locally in `src/Database.cpp`:

- Host: `127.0.0.1`
- Port: `3306`
- User: `root`
- Password: empty
- Database: `hotel_management`

Keep these settings for the default XAMPP setup. If you change the MariaDB user, password, port, or database name, update the connection values in `src/Database.cpp`; there is no separate environment-file or configuration-file setting.

## Optional LUXORA Demo Data

`database/demo_seed.sql` inserts a small, fictional demonstration dataset into
the local `hotel_management` database. It does not create or alter tables,
delete records, or update existing rows. It uses reserved demo IDs and
insert-if-absent checks, so running it again does not add duplicate sample
records. Do not run it against a production database.

The seed adds 24 rooms, 16 fictional customers, 20 reservations, 6 full
payments, 6 waiting-list entries, and 12 marked action-history entries.
On the inspected local database, the resulting totals are 24 rooms, 17
customers (including the pre-existing customer), 20 reservations, 6 payments,
6 waiting-list entries, and 13 action-history entries (including the
pre-existing action).
Reservation dates are calculated relative to the date the seed is first run.
Existing demo rows are intentionally not shifted or overwritten on later runs.

Before importing, make sure XAMPP MySQL/MariaDB is running and confirm that the
reserved demo ID ranges are not already used by unrelated rows. In phpMyAdmin,
select `hotel_management`, choose **Import**, select
`database/demo_seed.sql`, and run it. The final query reports how many demo
rows were found in each module.

The web server and console load database data into memory at startup. Restart
the web server after importing so the dashboard sees the seeded rows. Rooms
have no facilities field, guest capacities come from the existing category
rules, check-in/out are reservation statuses, and the payment model supports
full payment or unpaid reservations but not partial payments.

## Development Environment Setup

The project configuration targets 64-bit MSYS2 UCRT64 on Windows.

1. Install MSYS2.
2. Install GCC in the UCRT64 environment and ensure this compiler exists:

   ```text
   C:\msys64\ucrt64\bin\g++.exe
   ```

3. Install and configure MariaDB Connector/C for the UCRT64 environment.
4. Ensure the Connector/C header and library are available at:

   ```text
   C:\msys64\ucrt64\include\mariadb
   C:\msys64\ucrt64\lib
   ```

The actual project build settings use C++17 and these compiler/linker requirements:

- `-std=c++17`
- warnings: `-Wall -Wextra -pedantic`
- include directories: `-Iinclude`
- web-only third-party include: `-Ithird_party`
- MariaDB headers: `-IC:\msys64\ucrt64\include\mariadb`
- library directory: `-LC:\msys64\ucrt64\lib`
- connector library: `-lmariadb`
- web Windows socket library: `-lws2_32`

The build commands in this README and [web/BUILD.md](web/BUILD.md) use these
paths and settings for the console and web dashboard.

## Running the Console Application

Start MariaDB first. Then build from a PowerShell prompt in the project root:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude -IC:\msys64\ucrt64\include\mariadb main.cpp src\*.cpp -LC:\msys64\ucrt64\lib -lmariadb -o hotel_app_ucrt64.exe
```

Run the console executable:

```powershell
.\hotel_app_ucrt64.exe
```

Build and run the console executable with the commands above.

The console loads rooms, customers, reservations, payments, waiting-list entries, and action history before showing the main menu. It cannot start without a working MariaDB connection and valid persisted data.

## Running the Web Application

Start MariaDB and make sure the `hotel_management` schema exists. From PowerShell in the project root, build the separate web executable:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude -Ithird_party -IC:\msys64\ucrt64\include\mariadb web\server_main.cpp web\http_api.cpp src\*.cpp -LC:\msys64\ucrt64\lib -lmariadb -lws2_32 -o hotel_web_ucrt64.exe
```

Run it from the project root so `frontend/` is mounted correctly:

```powershell
.\hotel_web_ucrt64.exe
```

Open [http://127.0.0.1:8080](http://127.0.0.1:8080/) in a browser.

The web server binds only to `127.0.0.1:8080`. The console executable and web executable are separate processes, so each process holds its own in-memory snapshot and needs a data refresh after changes made elsewhere.

## Desktop App-Style Launcher

The desktop experience is Chrome app mode, not a native Windows application. It starts or reuses the existing C++ web server and opens the dashboard at [http://127.0.0.1:8080](http://127.0.0.1:8080/). MariaDB must be running first.

Build `hotel_web_ucrt64.exe` before using the launcher, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch-hotel-dashboard.ps1
```

The launcher:

- checks for MariaDB on `127.0.0.1:3306`;
- reuses an existing responding dashboard server or starts `hotel_web_ucrt64.exe`;
- waits for `/api/health`;
- opens Chrome app mode when Chrome is found;
- does not stop MariaDB or unrelated processes.

Create a desktop shortcut:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create-hotel-dashboard-shortcut.ps1
```

The setup script asks before replacing an existing `Haven Hotel Dashboard` shortcut.

Stop only a server started by the launcher:

```powershell
.\stop-hotel-dashboard.ps1
```

If the server was started manually, stop it in that terminal with `Ctrl+C`. The stop script does not stop Chrome or MariaDB.

## Installation on Another Computer

1. Clone the repository:

   ```powershell
   git clone https://github.com/zaincsstudentdevops/Hotel-Managment-System-CPP.git
   cd Hotel-Managment-System-CPP
   ```

2. Install:
   - XAMPP;
   - MSYS2;
   - GCC for the MSYS2 UCRT64 environment;
   - MariaDB Connector/C for UCRT64.
3. Start the XAMPP MySQL/MariaDB module.
4. Open phpMyAdmin and import `database/schema.sql`.
5. Confirm that `hotel_management` and its six tables exist.
6. If this computer is not using the default XAMPP `root` user with no password on port `3306`, update the connection settings in `src/Database.cpp`.
7. Open the repository in VS Code.
8. Build and run the web dashboard using the commands in [web/BUILD.md](web/BUILD.md).
9. Open [http://127.0.0.1:8080](http://127.0.0.1:8080/) or create a desktop shortcut with `create-hotel-dashboard-shortcut.ps1`.

Each computer's XAMPP database is local unless the C++ connection is changed to a shared MariaDB server. Cloning the repository copies the schema and application files; it does not copy another computer's database records.

## Limitations and Future Improvements

### Current limitations

- The console `Hotel` room array supports at most 50 rooms. Startup stops instead of silently loading a larger database past this capacity.
- Waiting-list notifications are console/web process results only; the project does not send email or SMS.
- Reservation date validation checks real dates, prevents room conflicts, and requires check-in to be today or a future date.
- The web server is bound to loopback only and has no account or role system.
- The database schema has no users/roles tables, so the frontend is not an authenticated application.
- Room photographs in `frontend/assets/` are illustrative category images; the database does not store images.
- The web frontend refreshes data and each running process keeps an in-memory snapshot.
- The existing C++ model does not associate a waiting-list entry with a customer ID or a particular room.

### Possible future improvements

- Add authenticated roles and authorization.
- Replace or extend the fixed 50-room array for larger inventories.
- Support external notifications.
- Add automatic date/today status rules and a native desktop interface.
- Add more complete room update/delete operations and waiting-list removal workflows.
- Synchronize data across multiple running processes or use database-backed reads for every operation.

These items are suggestions only; they are not represented as completed functionality.

## Screenshots

No screenshot image files are currently included in the repository. The available visual assets are:

- `frontend/assets/hotel-pool.jpg`
- `frontend/assets/room-single.jpg`
- `frontend/assets/room-double.jpg`
- `frontend/assets/room-deluxe.jpg`

These are decorative, illustrative photographs used by the frontend, not application screenshots.
