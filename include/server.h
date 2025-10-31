// include/server.h
#ifndef SERVER_H
#define SERVER_H

#include "common.h" // Needs User struct etc.

// --- Core Server Functions ---
void *handle_client(void *client_socket_ptr); // Main thread function
User check_login(int userId, char *password); // Authentication logic
void run_server_recovery();                   // <-- recovery function

// --- Session Management ---
// (We'll keep the session variables global in server.c for simplicity now)

#endif // SERVER_H