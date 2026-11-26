#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>

#include "http_api.h"
#include "HotelBackend.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using Json = nlohmann::json;

void sendJson(httplib::Response& response, int status, const Json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

void sendError(
    httplib::Response& response,
    int status,
    const std::string& message,
    const std::string& code = "request_error"
) {
    sendJson(response, status, {{"error", message}, {"code", code}});
}

bool parseObject(
    const httplib::Request& request,
    httplib::Response& response,
    Json& body
) {
    body = Json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        sendError(response, 400, "Request body must be a JSON object.",
                  "invalid_json");
        return false;
    }
    return true;
}

bool requiredString(
    const Json& body,
    const char* field,
    std::string& value,
    httplib::Response& response
) {
    const auto item = body.find(field);
    if (item == body.end() || !item->is_string()) {
        sendError(response, 400,
                  std::string("Field '") + field + "' must be a string.",
                  "invalid_field");
        return false;
    }
    value = item->get<std::string>();
    if (value.empty()) {
        sendError(response, 400,
                  std::string("Field '") + field + "' cannot be empty.",
                  "invalid_field");
        return false;
    }
    return true;
}

bool requiredInt(
    const Json& body,
    const char* field,
    int& value,
    httplib::Response& response
) {
    const auto item = body.find(field);
    if (item == body.end() ||
        (!item->is_number_integer() && !item->is_number_unsigned())) {
        sendError(response, 400,
                  std::string("Field '") + field +
                      "' must be an integer.",
                  "invalid_field");
        return false;
    }
    if (item->is_number_unsigned()) {
        const auto parsed = item->get<std::uint64_t>();
        if (parsed > static_cast<std::uint64_t>(
                         std::numeric_limits<int>::max())) {
            sendError(response, 400,
                      std::string("Field '") + field + "' is out of range.",
                      "invalid_field");
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }
    const auto parsed = item->get<std::int64_t>();
    if (parsed < static_cast<std::int64_t>(
                     std::numeric_limits<int>::min()) ||
        parsed > static_cast<std::int64_t>(
                     std::numeric_limits<int>::max())) {
        sendError(response, 400,
                  std::string("Field '") + field + "' is out of range.",
                  "invalid_field");
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool requiredNumber(
    const Json& body,
    const char* field,
    double& value,
    httplib::Response& response
) {
    const auto item = body.find(field);
    if (item == body.end() || !item->is_number()) {
        sendError(response, 400,
                  std::string("Field '") + field +
                      "' must be a number.",
                  "invalid_field");
        return false;
    }
    value = item->get<double>();
    if (!std::isfinite(value)) {
        sendError(response, 400,
                  std::string("Field '") + field +
                      "' must be finite.",
                  "invalid_field");
        return false;
    }
    return true;
}

bool parsePathId(
    const std::string& text,
    int& value,
    httplib::Response& response
) {
    int parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed <= 0) {
        sendError(response, 400, "Path ID must be a positive integer.",
                  "invalid_id");
        return false;
    }
    value = parsed;
    return true;
}

bool isInfrastructureError(const std::string& error) {
    std::string normalized = error;
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    const char* terms[] = {
        "mariadb", "database connection", "transaction", "statement",
        "mysql", "query", "commit", "rollback", "connection"
    };
    for (const char* term : terms) {
        if (normalized.find(term) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void sendBackendFailure(
    httplib::Response& response,
    const std::string& message
) {
    std::string normalized = message;
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    if (normalized.find("already exists") != std::string::npos ||
        normalized.find("already recorded") != std::string::npos ||
        normalized.find("overlapping") != std::string::npos ||
        normalized.find("duplicate entry") != std::string::npos) {
        sendError(response, 409, message, "conflict");
    } else if (isInfrastructureError(message)) {
        std::cerr << "HTTP backend operation failed: " << message << '\n';
        sendError(response, 500, "The database operation could not be completed.",
                  "backend_error");
    } else {
        sendError(response, 422, message, "operation_rejected");
    }
}

Json customerJson(const Customer& customer) {
    return {
        {"customer_id", customer.getCustomerId()},
        {"full_name", customer.getFullName()},
        {"phone_number", customer.getPhoneNumber()},
        {"email", customer.getEmail()},
        {"identity_document", customer.getIdentityDocument()}
    };
}

Json roomJson(const Room& room) {
    return {
        {"room_number", room.getRoomNumber()},
        {"category", room.getCategory()},
        {"price_per_night", room.getPricePerNight()},
        {"status", room.getStatus()}
    };
}

Json paymentJson(const Payment& payment) {
    return {
        {"payment_id", payment.getPaymentId()},
        {"reservation_id", payment.getReservationId()},
        {"amount", payment.getAmount()},
        {"payment_method", payment.getPaymentMethod()},
        {"payment_date", payment.getPaymentDate()},
        {"payment_status", payment.getPaymentStatus()}
    };
}

Json reservationJson(const Reservation& reservation) {
    Json payment = nullptr;
    if (reservation.hasPayment()) {
        payment = paymentJson(reservation.getPayment());
    }
    return {
        {"reservation_id", reservation.getReservationId()},
        {"customer_id", reservation.getCustomerId()},
        {"room_number", reservation.getRoomNumber()},
        {"check_in_date", reservation.getCheckInDate()},
        {"check_out_date", reservation.getCheckOutDate()},
        {"number_of_nights", reservation.getNumberOfNights()},
        {"total_cost", reservation.getTotalCost()},
        {"status", reservation.getStatus()},
        {"payment", payment}
    };
}

Json waitingEntryJson(const WaitingListEntrySnapshot& entry) {
    return {
        {"waiting_entry_id", entry.entryId},
        {"customer_name", entry.customerName}
    };
}

template <typename Items, typename Serializer>
Json collectionJson(const Items& items, Serializer serializer) {
    Json serialized = Json::array();
    for (const auto& item : items) {
        serialized.push_back(serializer(item));
    }
    return {{"items", std::move(serialized)}, {"count", items.size()}};
}

void registerCustomerRoutes(httplib::Server& server, HotelBackend& backend) {
    server.Get("/api/customers", [&backend](const auto&, auto& response) {
        const auto customers = backend.getCustomersSnapshot();
        sendJson(response, 200, collectionJson(
            customers, [](const Customer& customer) {
                return customerJson(customer);
            }
        ));
    });

    server.Get(R"(/api/customers/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int customerId = 0;
            if (!parsePathId(request.matches[1].str(), customerId, response)) {
                return;
            }
            Customer customer;
            std::string error;
            const CustomerLookupStatus status =
                backend.findCustomerById(customerId, customer, error);
            if (status == CustomerLookupStatus::found) {
                sendJson(response, 200, customerJson(customer));
            } else if (status == CustomerLookupStatus::notFound) {
                sendError(response, 404, "Customer was not found.", "not_found");
            } else {
                sendBackendFailure(response, error);
            }
        });

    server.Post("/api/customers",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            int customerId = 0;
            std::string fullName;
            std::string phone;
            std::string email;
            std::string identityDocument;
            if (!requiredInt(body, "customer_id", customerId, response) ||
                !requiredString(body, "full_name", fullName, response) ||
                !requiredString(body, "phone_number", phone, response) ||
                !requiredString(body, "email", email, response) ||
                !requiredString(body, "identity_document", identityDocument,
                                response)) {
                return;
            }
            const auto customers = backend.getCustomersSnapshot();
            if (std::any_of(customers.begin(), customers.end(),
                    [customerId](const Customer& customer) {
                        return customer.getCustomerId() == customerId;
                    })) {
                sendError(response, 409, "Customer ID already exists.",
                          "conflict");
                return;
            }
            const Customer customer(
                customerId, fullName, phone, email, identityDocument
            );
            std::string error;
            if (!backend.registerCustomer(customer, error)) {
                sendBackendFailure(response, error);
                return;
            }
            const auto savedCustomers = backend.getCustomersSnapshot();
            const auto saved = std::find_if(
                savedCustomers.begin(), savedCustomers.end(),
                [customerId](const Customer& item) {
                    return item.getCustomerId() == customerId;
                }
            );
            if (saved == savedCustomers.end()) {
                sendError(response, 500,
                          "Customer was created but is missing from memory.",
                          "backend_state_inconsistent");
                return;
            }
            sendJson(response, 201, customerJson(*saved));
        });

    server.Put(R"(/api/customers/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int customerId = 0;
            if (!parsePathId(request.matches[1].str(), customerId, response)) {
                return;
            }
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            if (body.contains("customer_id")) {
                int bodyId = 0;
                if (!requiredInt(body, "customer_id", bodyId, response)) {
                    return;
                }
                if (bodyId != customerId) {
                    sendError(response, 400,
                              "Body customer_id must match the path ID.",
                              "id_mismatch");
                    return;
                }
            }
            std::string fullName;
            std::string phone;
            std::string email;
            std::string identityDocument;
            if (!requiredString(body, "full_name", fullName, response) ||
                !requiredString(body, "phone_number", phone, response) ||
                !requiredString(body, "email", email, response) ||
                !requiredString(body, "identity_document", identityDocument,
                                response)) {
                return;
            }
            const Customer customer(
                customerId, fullName, phone, email, identityDocument
            );
            std::string error;
            const CustomerUpdateStatus status =
                backend.updateCustomer(customer, error);
            if (status == CustomerUpdateStatus::updated ||
                status == CustomerUpdateStatus::unchanged) {
                sendJson(response, 200, customerJson(customer));
            } else if (status == CustomerUpdateStatus::notFound) {
                sendError(response, 404, "Customer was not found.", "not_found");
            } else {
                sendBackendFailure(response, error);
            }
        });

    server.Delete(R"(/api/customers/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int customerId = 0;
            if (!parsePathId(request.matches[1].str(), customerId, response)) {
                return;
            }
            std::string error;
            const CustomerDeleteStatus status =
                backend.deleteCustomer(customerId, error);
            if (status == CustomerDeleteStatus::deleted) {
                sendJson(response, 200, {{"deleted", true},
                                         {"customer_id", customerId}});
            } else if (status == CustomerDeleteStatus::notFound) {
                sendError(response, 404, "Customer was not found.", "not_found");
            } else if (status == CustomerDeleteStatus::hasReservations) {
                sendError(response, 409, error, "customer_has_reservations");
            } else {
                sendBackendFailure(response, error);
            }
        });
}

void registerRoomRoutes(httplib::Server& server, HotelBackend& backend) {
    server.Get("/api/rooms", [&backend](const auto&, auto& response) {
        const auto rooms = backend.getRoomsSnapshot();
        sendJson(response, 200, collectionJson(
            rooms, [](const Room& room) { return roomJson(room); }
        ));
    });

    server.Get(R"(/api/rooms/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int roomNumber = 0;
            if (!parsePathId(request.matches[1].str(), roomNumber, response)) {
                return;
            }
            const auto rooms = backend.getRoomsSnapshot();
            const auto found = std::find_if(
                rooms.begin(), rooms.end(),
                [roomNumber](const Room& room) {
                    return room.getRoomNumber() == roomNumber;
                }
            );
            if (found == rooms.end()) {
                sendError(response, 404, "Room was not found.", "not_found");
                return;
            }
            sendJson(response, 200, roomJson(*found));
        });

    server.Post("/api/rooms",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            int roomNumber = 0;
            double price = 0.0;
            std::string category;
            if (!requiredInt(body, "room_number", roomNumber, response) ||
                !requiredString(body, "category", category, response) ||
                !requiredNumber(body, "price_per_night", price, response)) {
                return;
            }
            if (body.contains("status") &&
                (!body["status"].is_string() ||
                 body["status"].get<std::string>() != "Available")) {
                sendError(response, 422, "New rooms must start as Available.",
                          "invalid_room_status");
                return;
            }
            const auto rooms = backend.getRoomsSnapshot();
            if (std::any_of(rooms.begin(), rooms.end(),
                    [roomNumber](const Room& room) {
                        return room.getRoomNumber() == roomNumber;
                    })) {
                sendError(response, 409, "Room number already exists.",
                          "conflict");
                return;
            }
            const Room room(roomNumber, category, price, "Available");
            std::string error;
            if (!backend.addRoom(room, error)) {
                sendBackendFailure(response, error);
                return;
            }
            const auto savedRooms = backend.getRoomsSnapshot();
            const auto saved = std::find_if(
                savedRooms.begin(), savedRooms.end(),
                [roomNumber](const Room& item) {
                    return item.getRoomNumber() == roomNumber;
                }
            );
            if (saved == savedRooms.end()) {
                sendError(response, 500,
                          "Room was created but is missing from memory.",
                          "backend_state_inconsistent");
                return;
            }
            sendJson(response, 201, roomJson(*saved));
        });
}

void registerReservationRoutes(
    httplib::Server& server,
    HotelBackend& backend
) {
    server.Get("/api/reservations",
        [&backend](const auto&, auto& response) {
            const auto reservations = backend.getReservationsSnapshot();
            sendJson(response, 200, collectionJson(
                reservations, [](const Reservation& reservation) {
                    return reservationJson(reservation);
                }
            ));
        });

    server.Get(R"(/api/reservations/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int reservationId = 0;
            if (!parsePathId(
                    request.matches[1].str(), reservationId, response
                )) {
                return;
            }
            const auto reservations = backend.getReservationsSnapshot();
            const auto found = std::find_if(
                reservations.begin(), reservations.end(),
                [reservationId](const Reservation& reservation) {
                    return reservation.getReservationId() == reservationId;
                }
            );
            if (found == reservations.end()) {
                sendError(response, 404, "Reservation was not found.",
                          "not_found");
                return;
            }
            sendJson(response, 200, reservationJson(*found));
        });

    server.Post("/api/reservations",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            int reservationId = 0;
            int customerId = 0;
            int roomNumber = 0;
            int nights = 0;
            double totalCost = 0.0;
            std::string checkIn;
            std::string checkOut;
            if (!requiredInt(body, "reservation_id", reservationId, response) ||
                !requiredInt(body, "customer_id", customerId, response) ||
                !requiredInt(body, "room_number", roomNumber, response) ||
                !requiredString(body, "check_in_date", checkIn, response) ||
                !requiredString(body, "check_out_date", checkOut, response) ||
                !requiredInt(body, "number_of_nights", nights, response) ||
                !requiredNumber(body, "total_cost", totalCost, response)) {
                return;
            }
            const auto reservations = backend.getReservationsSnapshot();
            if (std::any_of(reservations.begin(), reservations.end(),
                    [reservationId](const Reservation& reservation) {
                        return reservation.getReservationId() == reservationId;
                    })) {
                sendError(response, 409, "Reservation ID already exists.",
                          "conflict");
                return;
            }
            const Reservation reservation = Reservation::fromStoredData(
                reservationId, customerId, roomNumber, checkIn, checkOut,
                nights, totalCost, "Confirmed"
            );
            std::string error;
            const ReservationCreateResult result =
                backend.createReservation(reservation, error);
            if (result == ReservationCreateResult::created) {
                const auto savedReservations =
                    backend.getReservationsSnapshot();
                const auto saved = std::find_if(
                    savedReservations.begin(), savedReservations.end(),
                    [reservationId](const Reservation& item) {
                        return item.getReservationId() == reservationId;
                    }
                );
                if (saved == savedReservations.end()) {
                    sendError(
                        response, 500,
                        "Reservation was created but is missing from memory.",
                        "backend_state_inconsistent"
                    );
                    return;
                }
                sendJson(response, 201, reservationJson(*saved));
            } else if (result == ReservationCreateResult::duplicateId ||
                       result == ReservationCreateResult::overlappingDates) {
                sendError(response, 409, error, "reservation_conflict");
            } else if (result == ReservationCreateResult::roomNotFound) {
                sendError(response, 404, error, "room_not_found");
            } else {
                sendBackendFailure(response, error);
            }
        });

    server.Post(R"(/api/reservations/([0-9]+)/cancel)",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int reservationId = 0;
            if (!parsePathId(
                    request.matches[1].str(), reservationId, response
                )) {
                return;
            }
            const auto reservations = backend.getReservationsSnapshot();
            if (std::none_of(reservations.begin(), reservations.end(),
                    [reservationId](const Reservation& reservation) {
                        return reservation.getReservationId() == reservationId;
                    })) {
                sendError(response, 404, "Reservation was not found.",
                          "not_found");
                return;
            }
            std::string error;
            if (!backend.cancelReservation(reservationId, error)) {
                if (error.find("not found") != std::string::npos) {
                    sendError(response, 404, error, "not_found");
                } else {
                    sendBackendFailure(response, error);
                }
                return;
            }
            sendJson(response, 200, {{"reservation_id", reservationId},
                                     {"status", "Cancelled"}});
        });

    server.Post(R"(/api/reservations/([0-9]+)/check-in)",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int reservationId = 0;
            if (!parsePathId(
                    request.matches[1].str(), reservationId, response
                )) {
                return;
            }
            const auto reservations = backend.getReservationsSnapshot();
            if (std::none_of(reservations.begin(), reservations.end(),
                    [reservationId](const Reservation& reservation) {
                        return reservation.getReservationId() == reservationId;
                    })) {
                sendError(response, 404, "Reservation was not found.",
                          "not_found");
                return;
            }
            std::string error;
            if (!backend.checkIn(reservationId, error)) {
                if (error.find("not found") != std::string::npos) {
                    sendError(response, 404, error, "not_found");
                } else {
                    sendBackendFailure(response, error);
                }
                return;
            }
            sendJson(response, 200, {{"reservation_id", reservationId},
                                     {"status", "CheckedIn"}});
        });

    server.Post(R"(/api/reservations/([0-9]+)/check-out)",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int reservationId = 0;
            if (!parsePathId(
                    request.matches[1].str(), reservationId, response
                )) {
                return;
            }
            const auto reservations = backend.getReservationsSnapshot();
            if (std::none_of(reservations.begin(), reservations.end(),
                    [reservationId](const Reservation& reservation) {
                        return reservation.getReservationId() == reservationId;
                    })) {
                sendError(response, 404, "Reservation was not found.",
                          "not_found");
                return;
            }
            std::string notifiedCustomer;
            std::string error;
            if (!backend.checkOut(
                    reservationId, notifiedCustomer, error
                )) {
                if (error.find("not found") != std::string::npos) {
                    sendError(response, 404, error, "not_found");
                } else {
                    sendBackendFailure(response, error);
                }
                return;
            }
            Json notified = nullptr;
            if (!notifiedCustomer.empty()) {
                notified = notifiedCustomer;
            }
            sendJson(response, 200, {
                {"reservation_id", reservationId},
                {"status", "CheckedOut"},
                {"notified_customer", notified}
            });
        });
}

void registerPaymentRoutes(httplib::Server& server, HotelBackend& backend) {
    server.Get("/api/payments", [&backend](const auto&, auto& response) {
        const auto payments = backend.getPaymentsSnapshot();
        sendJson(response, 200, collectionJson(
            payments, [](const Payment& payment) {
                return paymentJson(payment);
            }
        ));
    });

    server.Get(R"(/api/payments/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int paymentId = 0;
            if (!parsePathId(request.matches[1].str(), paymentId, response)) {
                return;
            }
            const auto payments = backend.getPaymentsSnapshot();
            const auto found = std::find_if(
                payments.begin(), payments.end(),
                [paymentId](const Payment& payment) {
                    return payment.getPaymentId() == paymentId;
                }
            );
            if (found == payments.end()) {
                sendError(response, 404, "Payment was not found.", "not_found");
                return;
            }
            sendJson(response, 200, paymentJson(*found));
        });

    server.Post(R"(/api/reservations/([0-9]+)/payments)",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            int reservationId = 0;
            if (!parsePathId(
                    request.matches[1].str(), reservationId, response
                )) {
                return;
            }
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            double amount = 0.0;
            std::string method;
            std::string date;
            if (!requiredNumber(body, "amount", amount, response) ||
                !requiredString(body, "payment_method", method, response) ||
                !requiredString(body, "payment_date", date, response)) {
                return;
            }
            Payment payment;
            std::string error;
            if (!backend.recordPayment(
                    reservationId, amount, method, date, payment, error
                )) {
                if (error.find("not found") != std::string::npos) {
                    sendError(response, 404, error, "reservation_not_found");
                } else {
                    sendBackendFailure(response, error);
                }
                return;
            }
            sendJson(response, 201, paymentJson(payment));
        });
}

void registerWaitingListRoutes(
    httplib::Server& server,
    HotelBackend& backend
) {
    server.Get("/api/waiting-list",
        [&backend](const auto&, auto& response) {
            std::vector<WaitingListEntrySnapshot> entries;
            std::string error;
            if (!backend.getWaitingListSnapshot(entries, error)) {
                sendBackendFailure(response, error);
                return;
            }
            sendJson(response, 200, collectionJson(
                entries, [](const WaitingListEntrySnapshot& entry) {
                    return waitingEntryJson(entry);
                }
            ));
        });

    server.Get(R"(/api/waiting-list/([0-9]+))",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            unsigned long long entryId = 0;
            const std::string text = request.matches[1].str();
            const char* begin = text.data();
            const char* end = begin + text.size();
            const auto parsed = std::from_chars(begin, end, entryId);
            if (parsed.ec != std::errc() || parsed.ptr != end || entryId == 0) {
                sendError(response, 400,
                          "Waiting-list ID must be a positive integer.",
                          "invalid_id");
                return;
            }
            std::vector<WaitingListEntrySnapshot> entries;
            std::string error;
            if (!backend.getWaitingListSnapshot(entries, error)) {
                sendBackendFailure(response, error);
                return;
            }
            const auto found = std::find_if(
                entries.begin(), entries.end(),
                [entryId](const WaitingListEntrySnapshot& entry) {
                    return entry.entryId == entryId;
                }
            );
            if (found == entries.end()) {
                sendError(response, 404, "Waiting-list entry was not found.",
                          "not_found");
                return;
            }
            sendJson(response, 200, waitingEntryJson(*found));
        });

    server.Post("/api/waiting-list",
        [&backend](const httplib::Request& request,
                   httplib::Response& response) {
            Json body;
            if (!parseObject(request, response, body)) {
                return;
            }
            std::string customerName;
            if (!requiredString(
                    body, "customer_name", customerName, response
                )) {
                return;
            }
            unsigned long long entryId = 0;
            std::string error;
            if (!backend.enqueueWaitingCustomer(
                    customerName, entryId, error
                )) {
                sendBackendFailure(response, error);
                return;
            }
            sendJson(response, 201, {
                {"waiting_entry_id", entryId},
                {"customer_name", customerName}
            });
        });
}

void registerDashboardRoutes(httplib::Server& server, HotelBackend& backend) {
    server.Get("/api/health", [](const auto&, auto& response) {
        sendJson(response, 200, {{"status", "ok"}});
    });

    server.Get("/api/dashboard",
        [&backend](const auto&, auto& response) {
            const DashboardSnapshot stats = backend.getDashboardSnapshot();
            auto recent = backend.getReservationsSnapshot();
            std::sort(recent.begin(), recent.end(),
                [](const Reservation& left, const Reservation& right) {
                    return left.getReservationId() >
                           right.getReservationId();
                });
            if (recent.size() > 5) {
                recent.resize(5);
            }
            Json recentJson = Json::array();
            for (const Reservation& reservation : recent) {
                recentJson.push_back(reservationJson(reservation));
            }
            const double occupancyRate = stats.totalRooms == 0
                ? 0.0
                : 100.0 * static_cast<double>(stats.occupiedRooms) /
                      static_cast<double>(stats.totalRooms);
            sendJson(response, 200, {
                {"customers", stats.totalCustomers},
                {"rooms", {
                    {"total", stats.totalRooms},
                    {"available", stats.availableRooms},
                    {"occupied", stats.occupiedRooms},
                    {"maintenance", stats.maintenanceRooms},
                    {"occupancy_rate", occupancyRate}
                }},
                {"reservations", {
                    {"total", stats.totalReservations},
                    {"confirmed", stats.confirmedReservations},
                    {"checked_in", stats.checkedInReservations},
                    {"checked_out", stats.checkedOutReservations},
                    {"cancelled", stats.cancelledReservations}
                }},
                {"payments", {
                    {"count", stats.totalPayments},
                    {"total_amount", stats.totalPaymentsAmount}
                }},
                {"waiting_list_count", stats.waitingListSize},
                {"recent_reservations", recentJson},
                {"reservation_order_note",
                 "Recent reservations are ordered by reservation ID because "
                 "the existing schema has no creation timestamp."}
            });
        });
}

}

void registerHotelApi(httplib::Server& server, HotelBackend& backend) {
    registerDashboardRoutes(server, backend);
    registerCustomerRoutes(server, backend);
    registerRoomRoutes(server, backend);
    registerReservationRoutes(server, backend);
    registerPaymentRoutes(server, backend);
    registerWaitingListRoutes(server, backend);
}
