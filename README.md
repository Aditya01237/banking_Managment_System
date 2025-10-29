# Banking Management System 🏦

## 📜 Description

This project simulates the core functionalities of a banking system using C. It focuses on demonstrating fundamental system software concepts, particularly **concurrency**, **process/thread synchronization**, **file management using system calls**, and **socket programming**. The system features role-based access control and ensures data consistency through locking mechanisms.

## ✨ Features

* **Role-Based Access Control:**
    * **Customer:** Manages personal accounts.
    * **Employee:** Manages customer accounts and processes loans.
    * **Manager:** Oversees employees, manages account statuses, and reviews feedback.
    * **Administrator:** Manages all user accounts (customers and employees).
* **Account Management:**
    * Customers can have **multiple accounts**.
    * View balance.
    * Deposit and withdraw funds.
    * Transfer funds between accounts (using Account Numbers).
    * View detailed transaction history per account.
* **Loan System:**
    * Customers can apply for loans linked to a specific account.
    * Managers can assign pending loans to employees.
    * Employees can view assigned loans and approve/reject them.
    * Approved loans automatically credit the specified customer account.
* **Feedback System:**
    * Customers can submit feedback.
    * Managers can review feedback.
* **User Management:**
    * Employees can add new customers and create their first account.
    * Employees can add additional accounts for existing customers.
    * Employees/Admins can modify user details (KYC info, password).
    * Managers/Admins can activate/deactivate users and their associated accounts.
    * Admins can add new employees/managers and change user roles.
* **Concurrency & Security:**
    * **Multithreaded Server:** Handles multiple client connections simultaneously using POSIX threads (`pthread`).
    * **Session Management:** Prevents multiple logins by the same user ID using a mutex-protected session list.
    * **File Locking:** Uses `fcntl` for both record-level (for specific accounts/users) and whole-file locking (for searching/appending) to prevent race conditions and ensure data integrity.
    * **System Calls:** Prioritizes direct system calls (`open`, `read`, `write`, `lseek`, `fcntl`) over standard library functions (`fopen`, `fread`, etc.) for file I/O.

## ⚙️ Technical Requirements Met

* **Socket Programming:** Implements a client-server architecture.
* **System Calls:** Uses system calls for file management, process/thread management, and synchronization.
* **File Management:** Uses binary files as a database.
* **File Locking:** Implements both shared (read) and exclusive (write) locks at file and record levels.
* **Multithreading:** Server uses `pthread_create` for concurrency.
* **Synchronization:** Uses `pthread_mutex_t` for session management and `fcntl` locks for file data consistency.

## 📁 Project Structure

```
BankingManagementSystem/
├── include/              # Header files (.h) defining interfaces and structures
│   ├── admin.h
│   ├── common.h
│   ├── customer.h
│   ├── data_access.h
│   ├── employee.h
│   ├── manager.h
│   └── server.h
├── src/                  # Source files (.c) implementing the logic
│   ├── admin.c
│   ├── admin_util.c      # Utility to create initial users/accounts
│   ├── client.c          # Client program
│   ├── common_utils.c    # Generic helper functions
│   ├── customer.c
│   ├── data_access.c     # Data storage and retrieval logic
│   ├── employee.c
│   ├── manager.c
│   └── server.c          # Main server logic (connection handling, threads)
├── data/                 # Data files (.dat) - created automatically
└── obj/                  # Compiled object files (.o) - created during build
```           

## 🧩 Modules

The project is structured into logical modules:

* **`common`:** Core data structures, enums, constants, and basic utilities.
* **`data_access`:** Handles all direct file I/O, locking, and data retrieval/storage operations.
* **`customer`:** Implements customer-specific menus and actions.
* **`employee`:** Implements employee-specific menus and actions.
* **`manager`:** Implements manager-specific menus and actions.
* **`admin`:** Implements admin-specific menus and actions.
* **`server`:** Handles client connections, threading, login, session management, and dispatches requests to the appropriate role module.
* **`client`:** The user-facing program to connect to the server.
* **`admin_util`:** A command-line tool to initialize the database files and create default users.

##  Compile Instructions

1.  **Open Terminal:** Navigate to the `BankingManagementSystem` root directory.
2.  **Create Directories:**
    ```bash
    mkdir -p obj data
    ```
3.  **Compile Object Files:**
    ```bash
    gcc -Iinclude -Wall -Wextra -c src/common_utils.c -o obj/common_utils.o
    gcc -Iinclude -Wall -Wextra -c src/data_access.c -o obj/data_access.o
    gcc -Iinclude -Wall -Wextra -c src/customer.c -o obj/customer.o
    gcc -Iinclude -Wall -Wextra -c src/employee.c -o obj/employee.o
    gcc -Iinclude -Wall -Wextra -c src/manager.c -o obj/manager.o
    gcc -Iinclude -Wall -Wextra -c src/admin.c -o obj/admin.o
    gcc -Iinclude -Wall -Wextra -c src/server.c -o obj/server.o
    ```
4.  **Link Server Executable:**
    ```bash
    gcc obj/*.o -o server -lpthread
    ```
5.  **Compile Client Executable:**
    ```bash
    gcc -Iinclude -Wall -Wextra src/client.c obj/common_utils.o -o client
    ```
6.  **Compile Admin Utility Executable:**
    ```bash
    gcc -Iinclude -Wall -Wextra src/admin_util.c obj/data_access.o obj/common_utils.o -o admin_util
    ```
    *(**Note:** Using a `Makefile` is recommended for easier compilation).*

## ▶️ How to Run

1.  **Clean Data (Optional but Recommended on First Run or After Struct Changes):**
    ```bash
    rm -f data/*.dat
    ```
2.  **Run Admin Utility (Run Once):** This creates the initial data files and default users.
    ```bash
    ./admin_util
    ```
3.  **Start the Server:** Open a terminal window and run:
    ```bash
    ./server
    ```
    *(Keep this terminal running).*
4.  **Run the Client:** Open one or more **new** terminal windows and run:
    ```bash
    ./client
    ```
    Follow the prompts to log in and use the system.

## 💡 Key Concepts Demonstrated

* **Concurrency:** Handling multiple clients simultaneously using multithreading.
* **Synchronization:** Using mutexes (`pthread_mutex_t`) for shared memory (session list) and file locks (`fcntl`) for shared file resources to prevent race conditions.
* **System Calls:** Direct use of low-level OS primitives for I/O and process/thread control.
* **Client-Server Architecture:** Separation of concerns between the client interface and the server logic/data management.
* **Modular Programming:** Organizing code into logical units with clear interfaces (header files).
* **Data Persistence:** Storing application state in binary files.
