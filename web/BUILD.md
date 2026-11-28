# Local web dashboard build and run

The web server and dashboard use C++17, MSYS2 UCRT64 GCC, MariaDB Connector/C,
cpp-httplib 0.18.3, and nlohmann/json 3.12.0. The two header-only web
dependencies and their MIT license files are in `third_party/`.

Start the XAMPP MariaDB service and make sure the existing
`hotel_management` database and six tables are available. The C++ server uses
the same database connection configuration and `HotelBackend::loadAll()` as
the console application.

From PowerShell in the project root, build the separate web executable:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra -pedantic -Iinclude -Ithird_party -IC:\msys64\ucrt64\include\mariadb web\server_main.cpp web\http_api.cpp src\*.cpp -LC:\msys64\ucrt64\lib -lmariadb -lws2_32 -o hotel_web_ucrt64.exe
```

Run it from the project root so it can find `frontend/`:

```powershell
.\hotel_web_ucrt64.exe
```

Open `http://127.0.0.1:8080` in a browser. The server binds to loopback only;
the original console executable and build task remain separate. Use one
interface at a time because each process holds its own in-memory snapshot.

## Windows desktop launch

The desktop experience is Chrome app mode, not a native Windows application.
It uses the same local C++ server and keeps ordinary browser access at
`http://127.0.0.1:8080`.

1. Start MySQL/MariaDB from the XAMPP Control Panel.
2. Build `hotel_web_ucrt64.exe` using the command above.
3. Run the launcher from PowerShell in the project root:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch-hotel-dashboard.ps1
   ```

   The execution-policy override applies only to this PowerShell process; it
   does not change the computer's saved policy. The launcher checks TCP
   access to MariaDB on `127.0.0.1:3306`, reuses a responding dashboard server
   or starts the existing executable from the project root, waits for its
   health endpoint, adds MSYS2 UCRT64's `bin` folder to the child process
   search path when present, then opens Chrome with
   `--app=http://127.0.0.1:8080`. It does not install software or stop Chrome,
   MariaDB, or unrelated processes. If port 8080 is occupied by another
   service, it reports the conflict instead of terminating it.
4. To create a Windows desktop shortcut, run
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create-hotel-dashboard-shortcut.ps1`
   yourself. It asks before replacing an existing shortcut. No desktop changes
   occur until this setup script is run.
5. To stop a server started by the launcher, run
   `stop-hotel-dashboard.ps1`. It verifies the recorded PID, executable path,
   and process start time before stopping that one server process. If the
   server was started manually or by VS Code, stop it in its terminal with
   Ctrl+C. Chrome and MariaDB remain running.

Launcher logs and its process record are kept under
`%LOCALAPPDATA%\HavenHotelDashboard`. If Chrome is not installed in a standard
Windows location, open the URL above in a browser manually.

## Local visual assets

The room-category and hotel-pool photographs are stored under
`frontend/assets/`; no external image host is needed at runtime. Room photos
are illustrative because the existing schema does not associate an image with
a room. Sources and license reference are listed in
`frontend/assets/IMAGE-CREDITS.txt`.
