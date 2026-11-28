#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>

#include "http_api.h"
#include "ActionStack.h"
#include "CustomerLinkedList.h"
#include "Database.h"
#include "Hotel.h"
#include "HotelBackend.h"
#include "ReservationLinkedList.h"
#include "WaitingQueue.h"

#include <exception>
#include <filesystem>
#include <iostream>

int main() {
    Database database;
    if (!database.connect()) {
        std::cerr << "Web server startup aborted because MariaDB is unavailable.\n";
        return 1;
    }

    Hotel hotel;
    CustomerLinkedList customers;
    ReservationLinkedList reservations;
    WaitingQueue waitingList;
    ActionStack actionHistory;
    HotelBackend backend(
        database, hotel, customers, reservations, waitingList, actionHistory
    );
    std::string startupError;
    if (!backend.loadAll(startupError)) {
        std::cerr << "Web server startup aborted. " << startupError << '\n';
        return 1;
    }

    const std::filesystem::path frontendDirectory =
        std::filesystem::current_path() / "frontend";
    if (!std::filesystem::is_directory(frontendDirectory)) {
        std::cerr << "Frontend directory was not found. Start the server from "
                     "the project root.\n";
        return 1;
    }

    httplib::Server server;
    server.new_task_queue = [] {
        return new httplib::ThreadPool(1);
    };
    server.set_payload_max_length(1024 * 1024);
    registerHotelApi(server, backend);
    if (!server.set_mount_point("/", frontendDirectory.string())) {
        std::cerr << "Could not mount the frontend static files.\n";
        return 1;
    }
    server.set_error_handler(
        [](const httplib::Request& request, httplib::Response& response) {
            if (request.path.compare(0, 5, "/api/") == 0 &&
                response.status == 404 && response.body.empty()) {
                response.set_content(
                    nlohmann::json({
                        {"error", "API endpoint was not found."},
                        {"code", "not_found"}
                    }).dump(),
                    "application/json; charset=utf-8"
                );
            }
        }
    );
    server.set_exception_handler(
        [](const httplib::Request&, httplib::Response& response,
           std::exception_ptr exception) {
            try {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            } catch (const std::exception& error) {
                std::cerr << "Unhandled HTTP handler exception: "
                          << error.what() << '\n';
            } catch (...) {
                std::cerr << "Unhandled non-standard HTTP handler exception.\n";
            }
            response.status = 500;
            response.set_content(
                R"({"error":"The server could not complete the request.","code":"server_error"})",
                "application/json; charset=utf-8"
            );
        }
    );
    std::cout << "Hotel Management web dashboard: http://127.0.0.1:8080\n"
              << "Serving MariaDB-backed data. Press Ctrl+C to stop.\n";
    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Could not start the HTTP server on 127.0.0.1:8080.\n";
        return 1;
    }
    return 0;
}
