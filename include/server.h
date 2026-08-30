#ifndef SERVER_H
#define SERVER_H

#include "common.h"

void *handle_client(void *client_socket_ptr);
User check_login(int userId, const char *password);
void run_server_recovery(void);

#endif
