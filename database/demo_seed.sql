-- LUXORA demo data for a local demonstration database.
-- This script only inserts missing rows; it never updates or deletes records.
USE hotel_management;

START TRANSACTION;

INSERT INTO rooms (room_number, category, price_per_night, status)
SELECT demo.room_number, demo.category, demo.price_per_night, demo.status
FROM (
    SELECT 901 AS room_number, 'Single' AS category, 4500.00 AS price_per_night, 'Available' AS status
    UNION ALL SELECT 902, 'Single', 5200.00, 'Available'
    UNION ALL SELECT 903, 'Single', 4800.00, 'Maintenance'
    UNION ALL SELECT 904, 'Single', 5500.00, 'Available'
    UNION ALL SELECT 905, 'Double', 6800.00, 'Available'
    UNION ALL SELECT 906, 'Double', 7200.00, 'Available'
    UNION ALL SELECT 907, 'Double', 7600.00, 'Occupied'
    UNION ALL SELECT 908, 'Double', 6500.00, 'Maintenance'
    UNION ALL SELECT 909, 'Double', 8000.00, 'Available'
    UNION ALL SELECT 910, 'Deluxe', 11800.00, 'Available'
    UNION ALL SELECT 911, 'Deluxe', 14500.00, 'Occupied'
    UNION ALL SELECT 912, 'Deluxe', 13500.00, 'Available'
    UNION ALL SELECT 913, 'Single', 4900.00, 'Available'
    UNION ALL SELECT 914, 'Single', 5600.00, 'Available'
    UNION ALL SELECT 915, 'Single', 5300.00, 'Occupied'
    UNION ALL SELECT 916, 'Single', 5100.00, 'Maintenance'
    UNION ALL SELECT 917, 'Double', 6900.00, 'Available'
    UNION ALL SELECT 918, 'Double', 7400.00, 'Available'
    UNION ALL SELECT 919, 'Double', 8200.00, 'Occupied'
    UNION ALL SELECT 920, 'Double', 7800.00, 'Available'
    UNION ALL SELECT 921, 'Deluxe', 11200.00, 'Available'
    UNION ALL SELECT 922, 'Deluxe', 12600.00, 'Available'
    UNION ALL SELECT 923, 'Deluxe', 13900.00, 'Maintenance'
    UNION ALL SELECT 924, 'Deluxe', 15200.00, 'Available'
) AS demo
WHERE NOT EXISTS (
    SELECT 1
    FROM rooms existing
    WHERE existing.room_number = demo.room_number
);

INSERT INTO customers (
    customer_id, full_name, phone_number, email, identity_document
)
SELECT demo.customer_id, demo.full_name, demo.phone_number,
       demo.email, demo.identity_document
FROM (
    SELECT 950001 AS customer_id, 'Amina Yusuf' AS full_name,
           '+1-202-555-0101' AS phone_number,
           'amina.yusuf@example.invalid' AS email,
           'DEMO-ID-001' AS identity_document
    UNION ALL SELECT 950002, 'Rayan Malik', '+1-202-555-0102',
           'rayan.malik@example.invalid', 'DEMO-ID-002'
    UNION ALL SELECT 950003, 'Leena Farooq', '+1-202-555-0103',
           'leena.farooq@example.invalid', 'DEMO-ID-003'
    UNION ALL SELECT 950004, 'Samir Rahman', '+1-202-555-0104',
           'samir.rahman@example.invalid', 'DEMO-ID-004'
    UNION ALL SELECT 950005, 'Mira Haddad', '+1-202-555-0105',
           'mira.haddad@example.invalid', 'DEMO-ID-005'
    UNION ALL SELECT 950006, 'Nadia Karim', '+1-202-555-0106',
           'nadia.karim@example.invalid', 'DEMO-ID-006'
    UNION ALL SELECT 950007, 'Omar Siddiqui', '+1-202-555-0107',
           'omar.siddiqui@example.invalid', 'DEMO-ID-007'
    UNION ALL SELECT 950008, 'Talia Noor', '+1-202-555-0108',
           'talia.noor@example.invalid', 'DEMO-ID-008'
    UNION ALL SELECT 950009, 'Farah Qureshi', '+1-202-555-0109',
           'farah.qureshi@example.invalid', 'DEMO-ID-009'
    UNION ALL SELECT 950010, 'Imran Sayeed', '+1-202-555-0110',
           'imran.sayeed@example.invalid', 'DEMO-ID-010'
    UNION ALL SELECT 950011, 'Layla Abbas', '+1-202-555-0111',
           'layla.abbas@example.invalid', 'DEMO-ID-011'
    UNION ALL SELECT 950012, 'Zain Ashraf', '+1-202-555-0112',
           'zain.ashraf@example.invalid', 'DEMO-ID-012'
    UNION ALL SELECT 950013, 'Sara Nadeem', '+1-202-555-0113',
           'sara.nadeem@example.invalid', 'DEMO-ID-013'
    UNION ALL SELECT 950014, 'Khalid Mansour', '+1-202-555-0114',
           'khalid.mansour@example.invalid', 'DEMO-ID-014'
    UNION ALL SELECT 950015, 'Yara Khalil', '+1-202-555-0115',
           'yara.khalil@example.invalid', 'DEMO-ID-015'
    UNION ALL SELECT 950016, 'Adil Hussain', '+1-202-555-0116',
           'adil.hussain@example.invalid', 'DEMO-ID-016'
) AS demo
WHERE NOT EXISTS (
    SELECT 1
    FROM customers existing
    WHERE existing.customer_id = demo.customer_id
);

INSERT INTO reservations (
    reservation_id, customer_id, room_number, check_in_date,
    check_out_date, total_cost, status
)
SELECT demo.reservation_id, demo.customer_id, demo.room_number,
       DATE_ADD(CURDATE(), INTERVAL demo.check_in_offset DAY),
       DATE_ADD(CURDATE(), INTERVAL demo.check_out_offset DAY),
       demo.nightly_rate * (demo.check_out_offset - demo.check_in_offset),
       demo.reservation_status
FROM (
    SELECT 960001 AS reservation_id, 950001 AS customer_id,
           907 AS room_number, -1 AS check_in_offset, 2 AS check_out_offset,
           7600.00 AS nightly_rate, 'Double' AS category,
           'Occupied' AS room_status, 'CheckedIn' AS reservation_status,
           'DEMO-ID-001' AS identity_document
    UNION ALL SELECT 960002, 950002, 911, -1, 1, 14500.00, 'Deluxe',
           'Occupied', 'CheckedIn', 'DEMO-ID-002'
    UNION ALL SELECT 960003, 950003, 905, 10, 13, 6800.00, 'Double',
           'Available', 'Confirmed', 'DEMO-ID-003'
    UNION ALL SELECT 960004, 950004, 906, 5, 7, 7200.00, 'Double',
           'Available', 'Confirmed', 'DEMO-ID-004'
    UNION ALL SELECT 960005, 950005, 910, 15, 18, 11800.00, 'Deluxe',
           'Available', 'Confirmed', 'DEMO-ID-005'
    UNION ALL SELECT 960006, 950006, 909, -8, -5, 8000.00, 'Double',
           'Available', 'CheckedOut', 'DEMO-ID-006'
    UNION ALL SELECT 960007, 950007, 904, -20, -17, 5500.00, 'Single',
           'Available', 'CheckedOut', 'DEMO-ID-007'
    UNION ALL SELECT 960008, 950008, 912, 4, 6, 13500.00, 'Deluxe',
           'Available', 'Cancelled', 'DEMO-ID-008'
    UNION ALL SELECT 960009, 950009, 915, -1, 2, 5300.00, 'Single',
           'Occupied', 'CheckedIn', 'DEMO-ID-009'
    UNION ALL SELECT 960010, 950010, 919, -1, 1, 8200.00, 'Double',
           'Occupied', 'CheckedIn', 'DEMO-ID-010'
    UNION ALL SELECT 960011, 950011, 917, 9, 12, 6900.00, 'Double',
           'Available', 'Confirmed', 'DEMO-ID-011'
    UNION ALL SELECT 960012, 950012, 918, 3, 5, 7400.00, 'Double',
           'Available', 'Confirmed', 'DEMO-ID-012'
    UNION ALL SELECT 960013, 950013, 921, 18, 20, 11200.00, 'Deluxe',
           'Available', 'Confirmed', 'DEMO-ID-013'
    UNION ALL SELECT 960014, 950014, 922, 25, 28, 12600.00, 'Deluxe',
           'Available', 'Confirmed', 'DEMO-ID-014'
    UNION ALL SELECT 960015, 950015, 920, 7, 9, 7800.00, 'Double',
           'Available', 'Confirmed', 'DEMO-ID-015'
    UNION ALL SELECT 960016, 950016, 913, -10, -8, 4900.00, 'Single',
           'Available', 'CheckedOut', 'DEMO-ID-016'
    UNION ALL SELECT 960017, 950009, 914, -15, -12, 5600.00, 'Single',
           'Available', 'CheckedOut', 'DEMO-ID-009'
    UNION ALL SELECT 960018, 950010, 924, -20, -18, 15200.00, 'Deluxe',
           'Available', 'CheckedOut', 'DEMO-ID-010'
    UNION ALL SELECT 960019, 950012, 917, -30, -28, 6900.00, 'Double',
           'Available', 'CheckedOut', 'DEMO-ID-012'
    UNION ALL SELECT 960020, 950013, 924, 6, 8, 15200.00, 'Deluxe',
           'Available', 'Cancelled', 'DEMO-ID-013'
) AS demo
JOIN rooms room_row
  ON room_row.room_number = demo.room_number
 AND room_row.category = demo.category
 AND room_row.price_per_night = demo.nightly_rate
 AND room_row.status = demo.room_status
JOIN customers customer_row
  ON customer_row.customer_id = demo.customer_id
 AND customer_row.identity_document = demo.identity_document
WHERE NOT EXISTS (
    SELECT 1
    FROM reservations existing
    WHERE existing.reservation_id = demo.reservation_id
);

INSERT INTO payments (
    reservation_id, amount, payment_method, payment_date
)
SELECT reservation_row.reservation_id, reservation_row.total_cost,
       demo.payment_method, demo.payment_date
FROM (
    SELECT 960001 AS reservation_id, 950001 AS customer_id,
           907 AS room_number, 22800.00 AS expected_total,
           'DEMO-ID-001' AS identity_document,
           'Cash' AS payment_method, CURDATE() AS payment_date
    UNION ALL SELECT 960003, 950003, 905, 20400.00, 'DEMO-ID-003',
           'Card', CURDATE()
    UNION ALL SELECT 960006, 950006, 909, 24000.00, 'DEMO-ID-006',
           'Card', DATE_SUB(CURDATE(), INTERVAL 5 DAY)
    UNION ALL SELECT 960009, 950009, 915, 15900.00, 'DEMO-ID-009',
           'Cash', CURDATE()
    UNION ALL SELECT 960011, 950011, 917, 20700.00, 'DEMO-ID-011',
           'Card', CURDATE()
    UNION ALL SELECT 960016, 950016, 913, 9800.00, 'DEMO-ID-016',
           'Cash', DATE_SUB(CURDATE(), INTERVAL 8 DAY)
) AS demo
JOIN reservations reservation_row
  ON reservation_row.reservation_id = demo.reservation_id
 AND reservation_row.customer_id = demo.customer_id
 AND reservation_row.room_number = demo.room_number
 AND reservation_row.total_cost = demo.expected_total
JOIN customers customer_row
  ON customer_row.customer_id = demo.customer_id
 AND customer_row.identity_document = demo.identity_document
WHERE NOT EXISTS (
    SELECT 1
    FROM payments existing
    WHERE existing.reservation_id = demo.reservation_id
);

INSERT INTO waiting_list (customer_name)
SELECT demo.customer_name
FROM (
    SELECT 'DEMO - Hana Saleh' AS customer_name
    UNION ALL SELECT 'DEMO - Idris Bennett'
    UNION ALL SELECT 'DEMO - Sofia Rahim'
    UNION ALL SELECT 'DEMO - Maya Iqbal'
    UNION ALL SELECT 'DEMO - Rafi Noor'
    UNION ALL SELECT 'DEMO - Lina Tariq'
) AS demo
WHERE NOT EXISTS (
    SELECT 1
    FROM waiting_list existing
    WHERE existing.customer_name = demo.customer_name
);

INSERT INTO action_history (action_text)
SELECT demo.action_text
FROM (
    SELECT 'DEMO DATA: sample room inventory loaded' AS action_text
    UNION ALL SELECT 'DEMO DATA: fictional guest profiles registered'
    UNION ALL SELECT 'DEMO DATA: upcoming stays confirmed'
    UNION ALL SELECT 'DEMO DATA: current guests checked in'
    UNION ALL SELECT 'DEMO DATA: completed stays checked out'
    UNION ALL SELECT 'DEMO DATA: full sample payments recorded'
    UNION ALL SELECT 'DEMO DATA: additional room inventory added'
    UNION ALL SELECT 'DEMO DATA: additional fictional guests registered'
    UNION ALL SELECT 'DEMO DATA: date-range booking examples added'
    UNION ALL SELECT 'DEMO DATA: checked-in and completed stays added'
    UNION ALL SELECT 'DEMO DATA: additional full payments recorded'
    UNION ALL SELECT 'DEMO DATA: Smart Room Finder scenarios expanded'
) AS demo
WHERE NOT EXISTS (
    SELECT 1
    FROM action_history existing
    WHERE existing.action_text = demo.action_text
);

COMMIT;

SELECT 'Demo rooms' AS demo_data, COUNT(*) AS row_count
FROM rooms
WHERE room_number BETWEEN 901 AND 924
UNION ALL
SELECT 'Demo customers', COUNT(*)
FROM customers
WHERE customer_id BETWEEN 950001 AND 950016
UNION ALL
SELECT 'Demo reservations', COUNT(*)
FROM reservations
WHERE reservation_id BETWEEN 960001 AND 960020
UNION ALL
SELECT 'Demo payments', COUNT(*)
FROM payments
WHERE reservation_id IN (
    960001, 960003, 960006, 960009, 960011, 960016
)
UNION ALL
SELECT 'Demo waiting-list entries', COUNT(*)
FROM waiting_list
WHERE customer_name IN (
    'DEMO - Hana Saleh',
    'DEMO - Idris Bennett',
    'DEMO - Sofia Rahim',
    'DEMO - Maya Iqbal',
    'DEMO - Rafi Noor',
    'DEMO - Lina Tariq'
)
UNION ALL
SELECT 'Demo action-history entries', COUNT(*)
FROM action_history
WHERE action_text LIKE 'DEMO DATA:%';
