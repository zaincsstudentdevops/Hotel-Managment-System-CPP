
#include <iostream>
#include "Hotel.h"
#include "CustomerLinkedList.h"

using namespace std;

int main() {

    cout << "\n===== STAGE 2: ROOM ARRAY TEST =====\n";

    Hotel myHotel;

    Room room1(101, "Single", 4000.0, "Available");
    Room room2(102, "Double", 6000.0, "Available");
    Room room3(103, "Deluxe", 8500.0, "Available");

    myHotel.addRoom(room1);
    myHotel.addRoom(room2);
    myHotel.addRoom(room3);

    myHotel.displayAllRooms();

    cout << "\n===== STAGE 3: CUSTOMER LINKED LIST =====\n";

    CustomerLinkedList customers;

    Customer c1(1, "Ali Ahmed", "03001234567",
                "ali@example.com", "12345-1234567-1");

    Customer c2(2, "Zain ul Abedin", "03007654321",
                "zain@example.com", "12345-7654321-1");

    customers.insertCustomer(c1);
    customers.insertCustomer(c2);

    cout << "\nAll Customers:\n";
    customers.displayAllCustomers();

    cout << "\nSearching Customer ID 2:\n";

    Customer* found = customers.searchById(2);

    if (found != nullptr) {
        found->displayCustomer();
    } else {
        cout << "Customer not found.\n";
    }

    cout << "\nSearching by Name:\n";

    found = customers.searchByName("Ali Ahmed");

    if (found != nullptr) {
        found->displayCustomer();
    } else {
        cout << "Customer not found.\n";
    }

    cout << "\nDeleting Customer ID 1:\n";

    if (customers.deleteCustomer(1)) {
        cout << "Customer deleted successfully.\n";
    } else {
        cout << "Customer not found.\n";
    }

    cout << "\nCustomers After Deletion:\n";
    customers.displayAllCustomers();

    return 0;
}
