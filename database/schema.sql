-- Hotel Management System schema for MySQL/MariaDB.
-- The current C++ application stores all data in memory; this file is a
-- proposed relational representation and has not been executed.

CREATE DATABASE IF NOT EXISTS hotel_management
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE hotel_management;

CREATE TABLE IF NOT EXISTS rooms (
    room_number INT NOT NULL,
    category ENUM('Single', 'Double', 'Deluxe') NOT NULL,
    price_per_night DECIMAL(10, 2) NOT NULL,
    status ENUM('Available', 'Occupied', 'Maintenance') NOT NULL
        DEFAULT 'Available',
    PRIMARY KEY (room_number),
    CONSTRAINT chk_rooms_number_positive CHECK (room_number > 0),
    CONSTRAINT chk_rooms_price_positive CHECK (price_per_night > 0)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS customers (
    customer_id INT NOT NULL,
    full_name VARCHAR(150) NOT NULL,
    phone_number VARCHAR(30) NOT NULL,
    email VARCHAR(254) NOT NULL,
    identity_document VARCHAR(50) NOT NULL,
    PRIMARY KEY (customer_id),
    CONSTRAINT chk_customers_id_positive CHECK (customer_id > 0)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS reservations (
    reservation_id INT NOT NULL,
    customer_id INT NOT NULL,
    room_number INT NOT NULL,
    check_in_date DATE NOT NULL,
    check_out_date DATE NOT NULL,
    number_of_nights INT UNSIGNED
        GENERATED ALWAYS AS (DATEDIFF(check_out_date, check_in_date)) STORED,
    total_cost DECIMAL(12, 2) NOT NULL,
    status ENUM('Confirmed', 'CheckedIn', 'CheckedOut', 'Cancelled') NOT NULL
        DEFAULT 'Confirmed',
    PRIMARY KEY (reservation_id),
    INDEX idx_reservations_customer (customer_id),
    INDEX idx_reservations_room_dates (
        room_number,
        check_in_date,
        check_out_date
    ),
    CONSTRAINT chk_reservations_date_order
        CHECK (check_out_date > check_in_date),
    CONSTRAINT chk_reservations_id_positive
        CHECK (reservation_id > 0),
    CONSTRAINT chk_reservations_total_positive
        CHECK (total_cost > 0),
    CONSTRAINT fk_reservations_customer
        FOREIGN KEY (customer_id)
        REFERENCES customers (customer_id)
        ON DELETE RESTRICT
        ON UPDATE RESTRICT,
    CONSTRAINT fk_reservations_room
        FOREIGN KEY (room_number)
        REFERENCES rooms (room_number)
        ON DELETE RESTRICT
        ON UPDATE RESTRICT
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS payments (
    payment_id INT NOT NULL AUTO_INCREMENT,
    reservation_id INT NOT NULL,
    amount DECIMAL(12, 2) NOT NULL,
    payment_method ENUM('Cash', 'Card') NOT NULL,
    payment_date DATE NOT NULL,
    PRIMARY KEY (payment_id),
    UNIQUE KEY uq_payments_reservation (reservation_id),
    CONSTRAINT chk_payments_amount_positive CHECK (amount > 0),
    CONSTRAINT fk_payments_reservation
        FOREIGN KEY (reservation_id)
        REFERENCES reservations (reservation_id)
        ON DELETE RESTRICT
        ON UPDATE RESTRICT
) ENGINE = InnoDB;

-- Each row is an active waiting-queue entry. Ordering by waiting_entry_id
-- preserves FIFO; the generated ID is a database key not present in the C++ model.
CREATE TABLE IF NOT EXISTS waiting_list (
    waiting_entry_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    customer_name VARCHAR(150) NOT NULL,
    PRIMARY KEY (waiting_entry_id)
) ENGINE = InnoDB;

-- action_id supplies insertion order for the C++ ActionStack's action strings.
-- Display newest rows first to retain the current LIFO presentation.
CREATE TABLE IF NOT EXISTS action_history (
    action_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    action_text TEXT NOT NULL,
    PRIMARY KEY (action_id)
) ENGINE = InnoDB;

-- A room/date overlap spans multiple reservation rows and cannot be expressed
-- as a row-level CHECK. The application must check and insert within a
-- transaction that serializes bookings for the same room. Cancelled reservations
-- are excluded from that check, matching the current C++ behavior.
--
-- The C++ model stores the booked total but not a per-night rate snapshot on a
-- reservation. total_cost is therefore stored as the booking-time amount and
-- must not be recalculated from the room's current price_per_night.
--
-- The payment amount must equal its reservation's total_cost. That comparison
-- spans tables, so the application must validate it when recording a payment.
--
-- There are no users/roles tables because the existing C++ project has no
-- account or authorization model. The current waiting queue stores names only;
-- it does not associate an entry with a customer ID or a particular room.
