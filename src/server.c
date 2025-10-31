// src/server.c
#include "server.h"     // Includes common.h and declares handle_client, check_login
#include "data_access.h" // Needed for check_login potentially using data funcs
#include "customer.h"   // For customer_menu, account_selection_menu
#include "employee.h"   // For employee_menu
#include "manager.h"    // For manager_menu
#include "admin.h"      // For admin_menu
#include <pthread.h>    // For threading
#include <stdlib.h>     // For malloc, free, atoi
#include <unistd.h>     // For close
#include <stdio.h>      // For perror

// --- Session Management Globals ---
#define MAX_SESSIONS 100 // Maximum concurrent logged-in users
int activeUserIds[MAX_SESSIONS];
int activeUserCount = 0;
pthread_mutex_t sessionMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect the list

// --- Core Login Function ---
// (Remains here as it's central authentication logic)
User check_login(int userId, char* password) {
    User user_to_find;
    user_to_find.userId = 0; // Default: not found

    // Use Data Access Layer functions (could be implemented here or called)
    int record_num = find_user_record(userId);
    if (record_num == -1) {
        return user_to_find; // Not found
    }

    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) {
        user_to_find.userId = -1; // Error reading file
        perror("check_login: open user file");
        return user_to_find;
    }

    // Lock the record for reading
    if (set_record_lock(fd, record_num, sizeof(User), F_RDLCK) == -1) {
        close(fd);
        user_to_find.userId = -1; // Locking error
        return user_to_find;
    }

    lseek(fd, record_num * sizeof(User), SEEK_SET);

    User user_from_file;
    if (read(fd, &user_from_file, sizeof(User)) == sizeof(User)) {
        // Verify ID, Password, and Active status
        if (user_from_file.userId == userId && my_strcmp(user_from_file.password, password) == 0) {
            if (user_from_file.isActive) {
                user_to_find = user_from_file; // Success! Copy details
            } else {
                user_to_find.userId = -2; // Deactivated
            }
        }
        // If password doesn't match, userId remains 0 (not found)
    } else {
         user_to_find.userId = -1; // Read error
         perror("check_login: read user record");
    }

    // Unlock the record
    set_record_lock(fd, record_num, sizeof(User), F_UNLCK);
    close(fd);

    return user_to_find;
}

/*
--- Main Client Handler (Thread Function) ---

-> The handle_client function is the "brain" for a single, isolated client connection.
-> When a client connects, the main function in server.c creates a new thread, and this one function is the 
   entire life's work of that thread. Its job is to:
-> Authenticate and verify the user (Role, ID, Password).
-> Check if that user is already logged in (Session Management).
-> Dispatch the user to their correct "department" (the role-specific menus like customer_menu).
-> Wait for them to log out.
-> Clean up their session and close the connection.

*/
void* handle_client(void* client_socket_ptr){
    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);

    // user: Will hold the user's details (name, role, etc.) once they log in.
    // roleChoice: Will store the user's menu selection (1-4).
    // expectedRole: Will store the actual enum value (ADMINISTRATOR, CUSTOMER, etc.) 
    // loginSuccess: This is a critical flag. We set this to 1 only if the user passes every single check.

    char buffer[MAX_BUFFER];
    User user;
    user.userId = -1;
    int roleChoice = 0;
    UserRole expectedRole;
    int loginSuccess = 0;

    // write_string / read_client_input: These are our custom utility functions.
    // write_string is a wrapper around the write system call, which sends bytes of data to the client's socket 
    // (their screen).
    // read_client_input is a wrapper around the read system call, which blocks (pauses the thread) and waits 
    // for the client to send data from their keyboard.

    write_string(client_socket,"Welcome to the Bank!\n");

    // --- Role Selection Loop ---
    while(1){
        write_string(client_socket,"Please select your role to log in:\n");
        write_string(client_socket," 1. Administrator\n 2. Manager\n 3. Employee\n 4. Customer\n");
        write_string(client_socket,"Enter choice (1-4): ");
        if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return NULL; // Client disconnected
        roleChoice = atoi(buffer);

        // --- Correct Mapping Logic ---
        if(roleChoice == 1){
            expectedRole = ADMINISTRATOR;
            break;
        }
        else if(roleChoice == 2){
            expectedRole = MANAGER;
            break;
        }
        else if(roleChoice == 3){
            expectedRole = EMPLOYEE;
            break;
        }
        else if(roleChoice == 4){
            expectedRole = CUSTOMER;
            break;
        }
        else{
            write_string(client_socket,"Invalid choice. Please try again.\n");
        }
    }

    // --- Get Credentials ---
    int userIdInput;
    char password[50];
    write_string(client_socket,"Enter User ID: ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return NULL; // Client disconnected
    userIdInput = atoi(buffer);
    write_string(client_socket,"Enter Password: ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return NULL; // Client disconnected
    strcpy(password, buffer);

    // --- Authentication ---
    user = check_login(userIdInput,password);

    // --- Verification and Session Check ---
    if(user.userId <= 0){
        if(user.userId == -2){
            write_string(client_socket, "Login failed: Your account is deactivated. Contact support.\n");
        }
        else{
            write_string(client_socket, "Login failed: Invalid User ID or Password.\n");
        }
    }
    else if(user.role != expectedRole){ // Role mismatch
        write_string(STDOUT_FILENO, "Login failed: Role mismatch.\n");
        write_string(client_socket, "Login failed: Your User ID does not match the selected role.\n");
    }
    else{
        // Key Concept: pthread_mutex_t (Mutex)

        // sessionMutex, activeUserIds, and activeUserCount are global variables. This means all threads 
        // can see and change them.

        // A race condition could happen if two threads try to add a user to the activeUserIds array at 
        // the exact same time.

        // A mutex (Mutual Exclusion) is like a " key" Only the thread holding the key can enter 
        // the "critical section" (the code that modifies the global variables).

        // pthread_mutex_lock(&sessionMutex); acquires the key. If another thread has it, this thread will
        // wait until it's released.

        // pthread_mutex_unlock(&sessionMutex); releases the key so another waiting thread can proceed.

        pthread_mutex_lock(&sessionMutex);
        int alreadyLoggedIn = 0;
        for(int i = 0; i < activeUserCount; i++){
            if (activeUserIds[i] == user.userId) {
                alreadyLoggedIn = 1;
                break;
            }
        }
        if(alreadyLoggedIn){
            pthread_mutex_unlock(&sessionMutex);
            write_string(STDOUT_FILENO, "Login failed: User already logged in.\n");
            write_string(client_socket, "ERROR: This user is already logged in elsewhere.\n");
            sleep(1); // <-- ADD THIS LINE to allow message to send
        }
        else if(activeUserCount >= MAX_SESSIONS){
            pthread_mutex_unlock(&sessionMutex);
            write_string(STDOUT_FILENO, "Login failed: Server full.\n");
            write_string(client_socket, "ERROR: Server is currently full. Please try again later.\n");
            sleep(1); // <-- ADD THIS LINE to allow message to send
        } 
        else{
            // *** SUCCESS CASE ***
            activeUserIds[activeUserCount] = user.userId;
            activeUserCount++;
            pthread_mutex_unlock(&sessionMutex);
            loginSuccess = 1; // Set the success flag ONLY HERE

            write_string(STDOUT_FILENO, "Login success, session added.\n");
            write_string(client_socket, "Login Successful!\n");

            // --- Menu Dispatch (Calls functions from other modules) ---
            switch(user.role){
                case CUSTOMER: account_selection_menu(client_socket,user);
                break;
                case EMPLOYEE: employee_menu(client_socket,user);
                break;
                case MANAGER: manager_menu(client_socket,user);
                break;
                case ADMINISTRATOR: admin_menu(client_socket,user);
                break;
            }
        }
    }

    // --- Session Cleanup ---
    if (loginSuccess == 1) {
        pthread_mutex_lock(&sessionMutex);
        for (int i = 0; i < activeUserCount; i++) {
            if(activeUserIds[i] == user.userId){
                // Swap-and-pop
                activeUserIds[i] = activeUserIds[activeUserCount-1];
                activeUserCount--;
                write_string(STDOUT_FILENO,"Session removed.\n");
                break;
            }
        }
        pthread_mutex_unlock(&sessionMutex);
    }

    close(client_socket);
    write_string(STDOUT_FILENO,"Client session ended.\n");
    return NULL;
}

// --- Main Server Setup (Threaded) ---
int main() {
    int server_fd; // File descriptor for the server socket (used to listen for connections).
    int new_socket; // File descriptor for a client’s socket (used to communicate with one client).
    struct sockaddr_in address; // Structure containing the IP address and port details.
    int addrlen = sizeof(address);
    pthread_t thread_id; // Stores the thread handle when a new thread is created.

    // --- Socket Setup (socket, bind, listen - same as before) ---
    // The kernel allocates a socket descriptor (like a file handle).
    // AF_INET → IPv4 domain.
    // SOCK_STREAM → TCP connection (reliable, connection-oriented).
    // Returns a file descriptor (like 3, 4, etc.).
    // (Linux treats sockets just like files — read/write works the same way.)


    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // INADDR_ANY → Listen on all available network interfaces (localhost + LAN IP).
    // htons() ensures correct endianness (big-endian order, required by TCP/IP).

    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any local IP (0.0.0.0)
    address.sin_port = htons(PORT);  // Host to Network Short: converts port to network byte order


    // The kernel associates this socket with the IP + Port (e.g., 0.0.0.0:8080).
    // It ensures no other process is using the same port.
    // Without bind(), your server wouldn’t have an address for clients to connect to.

    if(bind(server_fd,(struct sockaddr *)&address,sizeof(address)) < 0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Tells the OS: “I’m ready to accept incoming TCP connection requests.”
    // The 10 is the backlog — number of clients that can wait in queue while the server is busy.
    // At this point:
    // The server socket is passive, waiting for connection requests.

    if(listen(server_fd,10) < 0){
        perror("listen"); exit(EXIT_FAILURE);
    }

    write_string(STDOUT_FILENO,"Server listening on port 8080 (Threaded & Modular)...\n");

    // --- Accept Loop (Creates threads) ---
    while(1){
        // What accept() does internally:
        // It blocks (waits) until a client connects.
        // When a client (e.g., from telnet or another program) connects, the OS:
        // Establishes a TCP 3-way handshake.
        // Creates a new socket specifically for this client (different from server_fd).
        // Returns that new socket descriptor as new_socket.
        // Now, server_fd still listens for new clients, while new_socket is used for communication with one client.

        if((new_socket = accept(server_fd,(struct sockaddr *)&address,(socklen_t*)&addrlen)) < 0){
            perror("accept"); continue; // Continue listening even if accept fails
        }


        // Allocate memory for client socket pointer to pass to thread
        int* client_sock_ptr = (int*)malloc(sizeof(int));
        if(client_sock_ptr == NULL){
            perror("malloc for client socket ptr");
            close(new_socket); // Clean up the accepted socket
            continue; // Skip creating thread
        }
        *client_sock_ptr = new_socket;

        // Create the thread to handle the client
        if(pthread_create(&thread_id,NULL,handle_client,(void*)client_sock_ptr) != 0){
            perror("pthread_create failed");
            close(new_socket);       // Clean up socket
            free(client_sock_ptr); // Clean up allocated memory
        }
        else{
            pthread_detach(thread_id); // Detach thread for automatic cleanup
            write_string(STDOUT_FILENO, "New client connected, thread created.\n");
        }
    }
    close(server_fd); // Should technically be reached on server shutdown signal
    return 0;
}

/*
---- pthread_create(&thread_id,NULL,handle_client,(void*)client_sock_ptr) ----

🔹 What it does
This line creates a new thread in your program — a separate flow of execution that will run the function 
handle_client() to serve one client connection.

🔹 How it runs (execution flow)
Main thread calls pthread_create.
The OS allocates resources for a new thread.
The new thread starts executing immediately, beginning at handle_client(client_sock_ptr).
The main thread continues running — it doesn’t wait for the new one (they run concurrently).

🔹 Why (void*)client_sock_ptr ?
1. Because pthread_create expects the argument in a generic form
That means:
You can pass any type of data (int, struct, array, etc.).
But it must be converted (casted) to a void * before passing.
So, even though your data is an int * (pointer to socket descriptor),
you must explicitly cast it


*/