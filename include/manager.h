// include/manager.h
#ifndef MANAGER_H
#define MANAGER_H

#include "common.h"

// --- Menu ---
void manager_menu(int client_socket, User user);

// --- Action Handlers ---
void handle_set_account_status(int client_socket, int admin_mode); // Shared with Admin
void handle_assign_loan(int client_socket);
void handle_review_feedback(int client_socket);
// handle_view_my_details is shared (defined in customer.h)
// handle_change_password is shared (defined in customer.h)

#endif // MANAGER_H