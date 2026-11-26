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
- VS Code tasks for building

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
│   └── backend_integration_test.cpp
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
├── .vscode/
│   ├── tasks.json                   # Console, test, and web build tasks
│   └── c_cpp_properties.json        # UCRT64 compiler/include configuration
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

The VS Code build tasks in `.vscode/tasks.json` use these paths and settings for the console, web dashboard, and database tests.

## Running the Console Application

Start MariaDB first. Then build from a PowerShell prompt in the project root:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude -IC:\msys64\ucrt64\include\mariadb main.cpp src\*.cpp -LC:\msys64\ucrt64\lib -lmariadb -o hotel_app_ucrt64.exe
```

Run the console executable:

```powershell
.\hotel_app_ucrt64.exe
```

You can also run the **Build Hotel Management System** task in VS Code. The task builds `hotel_app_ucrt64.exe`.

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

VS Code also provides the **Build Hotel Web Dashboard** and **Run Hotel Web Dashboard** tasks. `Run Hotel Web Dashboard` builds the executable and then runs it.

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

If the server was started manually or through VS Code, stop it in that terminal with `Ctrl+C`. The stop script does not stop Chrome or MariaDB.

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
8. Build and run the web dashboard using the VS Code tasks or the commands in [web/BUILD.md](web/BUILD.md).
9. Open [http://127.0.0.1:8080](http://127.0.0.1:8080/) or create a desktop shortcut with `create-hotel-dashboard-shortcut.ps1`.

Each computer's XAMPP database is local unless the C++ connection is changed to a shared MariaDB server. Cloning the repository copies the schema and application files; it does not copy another computer's database records.

## Limitations and Future Improvements

### Current limitations

- The console `Hotel` room array supports at most 50 rooms. Startup stops instead of silently loading a larger database past this capacity.
- Waiting-list notifications are console/web process results only; the project does not send email or SMS.
- Reservation date validation checks real dates and room conflicts, but does not automatically compare bookings with today's date.
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
