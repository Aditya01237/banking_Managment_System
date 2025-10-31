#ifndef SERVER_H
#define SERVER_H

#include "common.h"

// Core Server Functions
void *handle_client(void *client_socket_ptr); // Main thread function
User check_login(int userId, char *password); // Authentication logic
void run_server_recovery(); // ecovery function

#endif