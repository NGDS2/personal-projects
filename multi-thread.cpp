#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <pthread.h>
#include <iomanip> // For setting decimal precision

// Structure to represent inventory items
struct InventoryItem {
    int productID;
    double price;
    int quantity;
    std::string description;
};

// Structure to represent customer orders
struct Order {
    int customerID;
    int productID;
    int quantityOrdered;
};

// Shared data structures
std::vector<InventoryItem> inventory;
std::vector<Order> orders;

// Mutex for synchronizing access to the inventory and orders list
pthread_mutex_t inventoryMutex;
pthread_mutex_t ordersMutex;

// Function to read the inventory file
std::vector<InventoryItem> readInventory(const std::string& filename) {
    std::vector<InventoryItem> inventory;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return inventory;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        InventoryItem item;

        iss >> item.productID >> item.price >> item.quantity;
        std::getline(iss, item.description); // Read the remaining line as the description

        // Remove leading whitespace from the description
        item.description = item.description.substr(1);

        inventory.push_back(item);
    }

    file.close();
    return inventory;
}

// Function to read the orders file (Producer Thread)
void* producerThread(void* arg) {
    std::string ordersFile = "orders";
    std::ifstream file(ordersFile);

    if (!file.is_open()) {
        std::cerr << "Error opening orders file: " << ordersFile << std::endl;
        pthread_exit(nullptr);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Order order;
        iss >> order.customerID >> order.productID >> order.quantityOrdered;

        // Lock the mutex before modifying the shared orders list
        pthread_mutex_lock(&ordersMutex);
        orders.push_back(order);
        pthread_mutex_unlock(&ordersMutex);
    }

    file.close();
    pthread_exit(nullptr);
}

// Function to process orders (Consumer Thread)
void* consumerThread(void* arg) {
    std::ofstream logFile("log");
    if (!logFile.is_open()) {
        std::cerr << "Error opening log file" << std::endl;
        pthread_exit(nullptr);
    }

    // Set up column headers in the log file
    logFile << std::left << std::setw(12) << "CustomerID"
            << std::setw(12) << "ProductID"
            << std::setw(30) << "Description"
            << std::setw(8) << "Ordered"
            << std::setw(12) << "Amount"
            << "Result" << std::endl;

    // Process each order
    pthread_mutex_lock(&ordersMutex);
    for (const auto& order : orders) {
        pthread_mutex_unlock(&ordersMutex);

        pthread_mutex_lock(&inventoryMutex);
        bool orderFilled = false;
        double transactionAmount = 0.0;
        std::string description;

        // Search for the product in the inventory
        for (auto& item : inventory) {
            if (item.productID == order.productID) {
                description = item.description;
                if (item.quantity >= order.quantityOrdered) {
                    // Update inventory
                    item.quantity -= order.quantityOrdered;
                    transactionAmount = order.quantityOrdered * item.price;
                    orderFilled = true;
                }
                break;
            }
        }
        pthread_mutex_unlock(&inventoryMutex);

        // Log the result of processing the order
        logFile << std::left << std::setw(12) << order.customerID
                << std::setw(12) << order.productID
                << std::setw(30) << description
                << std::setw(8) << order.quantityOrdered
                << std::setw(12) << std::fixed << std::setprecision(2) << transactionAmount
                << (orderFilled ? "Filled" : "Rejected") << std::endl;

        pthread_mutex_lock(&ordersMutex);
    }
    pthread_mutex_unlock(&ordersMutex);

    logFile.close();
    pthread_exit(nullptr);
}

// Function to display the inventory (for testing purposes)
void displayInventory(const std::vector<InventoryItem>& inventory) {
    for (const auto& item : inventory) {
        std::cout << "ID: " << item.productID << ", Price: " << item.price
                  << ", Quantity: " << item.quantity << ", Description: " << item.description << std::endl;
    }
}

// Function to display the orders (for testing purposes)
void displayOrders() {
    pthread_mutex_lock(&ordersMutex);  // Locking to ensure thread-safe access
    for (const auto& order : orders) {
        std::cout << "Customer ID: " << order.customerID << ", Product ID: " << order.productID
                  << ", Quantity Ordered: " << order.quantityOrdered << std::endl;
    }
    pthread_mutex_unlock(&ordersMutex);
}

int main() {
    // Initialize the mutexes
    pthread_mutex_init(&inventoryMutex, nullptr);
    pthread_mutex_init(&ordersMutex, nullptr);

    // Read inventory from file
    std::string inventoryFile = "inventory.old";
    inventory = readInventory(inventoryFile);

    // Display inventory (for testing purposes)
    std::cout << "Initial Inventory:\n";
    displayInventory(inventory);

    // Create the producer thread
    pthread_t producer;
    if (pthread_create(&producer, nullptr, producerThread, nullptr)) {
        std::cerr << "Error creating producer thread!" << std::endl;
        return 1;
    }

    // Wait for the producer thread to finish
    pthread_join(producer, nullptr);

    // Display orders (for testing purposes)
    std::cout << "\nOrders Received:\n";
    displayOrders();

    // Create the consumer thread
    pthread_t consumer;
    if (pthread_create(&consumer, nullptr, consumerThread, nullptr)) {
        std::cerr << "Error creating consumer thread!" << std::endl;
        return 1;
    }

    // Wait for the consumer thread to finish
    pthread_join(consumer, nullptr);

    // Display updated inventory
    std::cout << "\nUpdated Inventory:\n";
    displayInventory(inventory);

    // Destroy the mutexes
    pthread_mutex_destroy(&inventoryMutex);
    pthread_mutex_destroy(&ordersMutex);

    return 0;
}