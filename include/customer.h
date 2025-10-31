// include/customer.h
#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "common.h"

// --- Menu ---
void account_selection_menu(int client_socket, User user);
void customer_menu(int client_socket, User user, int accountId);

// --- Action Handlers ---
void handle_view_balance(int client_socket, int accountId);
void handle_deposit(int client_socket, int accountId);
void handle_withdraw(int client_socket, int accountId);
void handle_transfer_funds(int client_socket, int senderAccountId);
void handle_view_transaction_history(int client_socket, int accountId);
void handle_apply_loan(int client_socket, int userId);
void handle_view_loan_status(int client_socket, int userId);
void handle_add_feedback(int client_socket, int userId);
void handle_view_feedback_status(int client_socket, int userId);
void handle_view_my_details(int client_socket, User user);
void handle_change_password(int client_socket, int userId);


#endif // CUSTOMER_H