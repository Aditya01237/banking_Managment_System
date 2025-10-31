#ifndef EMPLOYEE_H
#define EMPLOYEE_H

#include "common.h"

// Menu
void employee_menu(int client_socket, User user);

// Action Handlers
void handle_add_user(int client_socket, UserRole role_to_add);
void handle_add_new_account(int client_socket);
void handle_modify_user_details(int client_socket, int admin_mode);
void handle_view_customer_transactions(int client_socket);
void handle_view_assigned_loans(int client_socket, int employeeId);
void handle_process_loan(int client_socket, int employeeId);

#endif