// include/employee.h
#ifndef EMPLOYEE_H
#define EMPLOYEE_H

#include "common.h"

// --- Menu ---
void employee_menu(int client_socket, User user);

// --- Action Handlers ---
void handle_add_user(int client_socket, UserRole role_to_add); // Shared with Admin
void handle_add_new_account(int client_socket);
void handle_modify_user_details(int client_socket, int admin_mode); // Shared with Admin
void handle_view_customer_transactions(int client_socket);
void handle_view_assigned_loans(int client_socket, int employeeId);
void handle_process_loan(int client_socket, int employeeId);
// handle_view_my_details is shared (defined in customer.h)
// handle_change_password is shared (defined in customer.h)

#endif // EMPLOYEE_H