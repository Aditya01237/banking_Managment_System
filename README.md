# 🏦 Banking Management System

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
    * View detailed, timestamped transaction history per account.
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

## 🗃️ Data Integrity & ACID Properties

This project implements features to ensure data integrity, modeled after the **ACID** properties.

* **A - Atomicity (All or Nothing):**
    * Implemented for critical transactions (like `handle_transfer_funds`) using a **Write-Ahead Log (WAL) / Journal** (`journal.log`).
    * **Flow:**
        1.  `[TXN_START]` entries (containing "undo" information) are written to the journal and forced to disk with `fsync()`.
        2.  The `accounts.dat` file is modified.
        3.  A `[TXN_COMMIT]` entry is written to the journal and `fsync()`ed.
    * **Recovery:** On startup, `run_server_recovery()` reads the journal. If it finds `_START` entries without a `_COMMIT` (indicating a crash), it uses the "undo" info to **roll back** the changes, ensuring the database is always in a consistent state.

* **C - Consistency:**
    * Enforced by **application-level logic** (e.g., `is_valid_number`, checking for sufficient funds) and guaranteed by **Atomicity**.

* **I - Isolation:**
    * Implemented using the **`fcntl`** system call for file locking.
    * **Record-Level Locking:** `updateAccount` and other modification functions lock *only* the specific record (byte range) being changed. This allows two users to modify *different* accounts at the same time.
    * **File-Level Locking:** A shared read lock (`F_RDLCK`) is used when searching files (like `find_user_record`) to allow concurrent reads but prevent a concurrent write.

* **D - Durability:**
    * Implemented using the **`fsync()`** system call.
    * After every critical `write()` to the data files or journal, `fsync()` is called to force the OS to flush the data from the memory cache to the physical disk. This ensures that completed transactions are permanent.

## 🛡️ Robust Error Handling

The system is hardened against common errors and bad user input.

* **Input Validation:** All user input is validated at the source.
    * **Data Type:** `atof()` is protected by `is_valid_number()` to prevent bugs like `"100abc"`.
    * **Format:** `is_valid_phone()` (10 digits) and `is_valid_email()` (contains `@` and `.`) are used.
    * **Buffer Overflow:** `strlen` is checked against struct field sizes (e.g., password < 50) before `strcpy`.
    * **Empty Input:** `strlen(buffer) == 0` is checked to prevent empty names or passwords.
* **User Experience:**
    * **Re-prompting:** Invalid input (e.g., bad phone number) re-prompts the user for *just that field* instead of aborting the entire operation.
    * **Cancel Function:** Users can type `"0"` at (almost) any prompt to safely return to the previous menu.
* **Concurrency Safety:**
    * **Race Conditions:** User/Account creation is protected by a dedicated mutex (`create_user_mutex`) to prevent two users from being created with the same ID.
    * **Uniqueness:** The system checks for duplicate phone numbers or emails before creating a new user.
    * **Orphaned Sessions:** The server robustly handles unexpected client disconnects (`Ctrl+C`) by detecting the `read()` failure, which causes the thread to exit and trigger the session cleanup logic.
* **System Call Robustness:**
    * The return values of `read()` and `write()` are checked to prevent data corruption from partial writes (e.g., disk full) or the use of garbage data from failed reads.

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
├── src/                  # Source files implementing the logic
│   ├── admin.c
│   ├── admin_util.c      # Utility to create initial users/accounts
│   ├── client.c          # Client program
│   ├── common_utils.c    # Generic helper functions
│   ├── customer.c
│   ├── data_access.c     # Data storage and retrieval logic
│   ├── employee.c
│   ├── manager.c
│   └── server.c          # Main server logic (connection handling, threads)
├── data/                 # Data files
├── obj/                  # Compiled object files 
└── Makefile              # Optional: For automating compilation

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

## 🚀 How to Compile and Run

### 1. Compile
You must compile all the modules and link them together.

1.  **Open Terminal:** Navigate to the `BankingManagementSystem` root directory.
2.  **Create Directories:**
    ```bash
    mkdir -p obj data
    ```
3.  **Compile Object Files:**
    ```bash
    gcc -Iinclude -Wall -Wextra -g -c src/common_utils.c -o obj/common_utils.o
    gcc -Iinclude -Wall -Wextra -g -c src/data_access.c -o obj/data_access.o
    gcc -Iinclude -Wall -Wextra -g -c src/customer.c -o obj/customer.o
    gcc -Iinclude -Wall -Wextra -g -c src/employee.c -o obj/employee.o
    gcc -Iinclude -Wall -Wextra -g -c src/manager.c -o obj/manager.o
    gcc -Iinclude -Wall -Wextra -g -c src/admin.c -o obj/admin.o
    gcc -Iinclude -Wall -Wextra -g -c src/server.c -o obj/server.o
    ```
4.  **Link Server Executable:**
    ```bash
    gcc obj/*.o -o server -lpthread
    ```
5.  **Compile Client Executable:**
    ```bash
    gcc -Iinclude -Wall -Wextra -g src/client.c obj/common_utils.o -o client
    ```
6.  **Compile Admin Utility Executable:**
    ```bash
    gcc -Iinclude -Wall -Wextra -g src/admin_util.c obj/data_access.o obj/common_utils.o -o admin_util
    ```

### 2. Run
You will need at least two terminals.

1.  **Initialize Data (Run Once):**
    *First, clean any old data (if necessary) and run the admin utility to create default users.*
    ```bash
    rm -f data/*.dat data/*.log
    ./admin_util
    ```

2.  **Terminal 1: Start the Server**
    ```bash
    ./server
    ```
    *(The server will start, check the journal, and begin listening).*

3.  **Terminal 2 (and 3, 4...): Run the Client**
    ```bash
    ./client
    ```
    *(Follow the prompts to log in and use the system).*


