#ifndef MANAGER_H
#define MANAGER_H

#include "common.h"

// Menu
void manager_menu(int client_socket, User user);

// Action Handlers
void handle_set_account_status(int client_socket, int admin_mode);
void handle_assign_loan(int client_socket);
void handle_review_feedback(int client_socket);

#endif // MANAGER_H