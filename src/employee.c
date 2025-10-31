// src/employee.c
#include "employee.h"   // Function declarations for employee module
#include "customer.h"   // For shared handle_view_my_details, handle_change_password
#include "data_access.h" // For data access functions
#include "common.h"     // For structs, enums, read_client_input, write_string
#include <stdio.h>      // For sprintf
#include <stdlib.h>     // For atoi

// --- Employee Menu ---
void employee_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    while(1) {
        sprintf(buffer, "\n--- Employee Menu (User: %s %s) ---\n", user.firstName, user.lastName);
        write_string(client_socket, buffer);
        write_string(client_socket, "1. Add New Customer\n");
        write_string(client_socket, "2. Add New Account for Existing Customer\n");
        write_string(client_socket, "3. Modify Customer Details\n");
        write_string(client_socket, "4. View Customer Transactions\n");
        write_string(client_socket, "5. View Assigned Loans\n");
        write_string(client_socket, "6. Process Loan Application\n");
        write_string(client_socket, "7. View My Personal Details\n");
        write_string(client_socket, "8. Change My Password\n");
        write_string(client_socket, "9. Logout\n");
        write_string(client_socket, "Enter your choice: ");

        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        switch(choice) {
            case 1: handle_add_user(client_socket, CUSTOMER); break;
            case 2: handle_add_new_account(client_socket); break;
            case 3: handle_modify_user_details(client_socket, 0); break; // 0 = Not admin
            case 4: handle_view_customer_transactions(client_socket); break;
            case 5: handle_view_assigned_loans(client_socket, user.userId); break;
            case 6: handle_process_loan(client_socket, user.userId); break;
            case 7: handle_view_my_details(client_socket, user); break; // Shared function
            case 8: handle_change_password(client_socket, user.userId); break; // Shared function
            case 9: write_string(client_socket, "Logging out. Goodbye!\n"); return;
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// --- Employee Action Handlers (using Data Access Layer) ---

// Shared with Admin, implemented here for now
void handle_add_user(int client_socket, UserRole role_to_add) {
    char buffer[MAX_BUFFER];
    User new_user;
    // new_user.userId will be set by addUser
    new_user.role = role_to_add;
    new_user.isActive = 1;

    // Get details from client
    write_string(client_socket, "Enter new user's password: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.password, buffer);
    
    write_string(client_socket, "Enter user's First Name: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.firstName, buffer);
    
    write_string(client_socket, "Enter user's Last Name: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.lastName, buffer);
    
    write_string(client_socket, "Enter user's Phone: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.phone, buffer);
    
    write_string(client_socket, "Enter user's Email: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.email, buffer);
    
    write_string(client_socket, "Enter user's Address: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(new_user.address, buffer);

    // Add user using Data Access Layer
    if (addUser(new_user) != 0) {
         write_string(client_socket, "Error adding user.\n");
         return;
    }
    // Need the new ID for the message and account creation
    // (Ideally addUser would return the new ID)
    // Find the user we just added (inefficient but works for now)
    int latest_id = get_next_user_id() - 1; 
    new_user.userId = latest_id; 


    // If it's a customer, create their first bank account
    if (role_to_add == CUSTOMER) {
        Account new_account;
        // accountId will be set by addAccount
        new_account.ownerUserId = new_user.userId;
        new_account.balance = 0.0;
        new_account.isActive = 1;
        generate_new_account_number(new_account.accountNumber); // Generate "SB-10003" etc.

        if (addAccount(new_account) == 0) {
             sprintf(buffer, "User created successfully. New User ID: %d, New Account Number: %s\n", new_user.userId, new_account.accountNumber);
             write_string(client_socket, buffer);
        } else {
             write_string(client_socket, "User created, but failed to create account.\n");
        }
    } else {
        sprintf(buffer, "User created successfully. New User ID: %d\n", new_user.userId);
        write_string(client_socket, buffer);
    }
}

void handle_add_new_account(int client_socket) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Customer User ID to add account to: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int cust_id = atoi(buffer);
    
    // Check if user exists using Data Access Layer
    User customer = getUser(cust_id);
    if (customer.userId == -1) { 
        write_string(client_socket, "User not found.\n"); 
        return; 
    }
    if (customer.role != CUSTOMER) {
         write_string(client_socket, "User is not a customer.\n"); 
        return; 
    }
    
    Account new_account;
    // accountId set by addAccount
    new_account.ownerUserId = cust_id;
    new_account.balance = 0.0;
    new_account.isActive = 1;
    generate_new_account_number(new_account.accountNumber); 

    if (addAccount(new_account) == 0) {
        sprintf(buffer, "New account %s created successfully for User ID %d.\n", new_account.accountNumber, cust_id);
        write_string(client_socket, buffer);
    } else {
        write_string(client_socket, "Error adding new account.\n");
    }
}

// Shared with Admin, implemented here
void handle_modify_user_details(int client_socket, int admin_mode) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter User ID to modify: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int target_user_id = atoi(buffer);

    User user = getUser(target_user_id); // Get user data
    if (user.userId == -1) { write_string(client_socket, "User not found.\n"); return; }

    // Security check
    if (!admin_mode && user.role != CUSTOMER) {
        write_string(client_socket, "Permission denied. Employees can only modify customers.\n");
        return;
    }

    // Get modifications (same logic as before, asking 'skip')
    write_string(client_socket, "Enter new password (or 'skip'): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    if (my_strcmp(buffer, "skip") != 0) { strcpy(user.password, buffer); }
    
    write_string(client_socket, "Enter new First Name (or 'skip'): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    if (my_strcmp(buffer, "skip") != 0) { strcpy(user.firstName, buffer); }
    
    // ... (repeat for lastName, phone, email, address) ...
     write_string(client_socket, "Enter new Last Name (or 'skip'): ");
     read_client_input(client_socket, buffer, MAX_BUFFER);
     if (my_strcmp(buffer, "skip") != 0) { strcpy(user.lastName, buffer); }
     
     write_string(client_socket, "Enter new Phone (or 'skip'): ");
     read_client_input(client_socket, buffer, MAX_BUFFER);
     if (my_strcmp(buffer, "skip") != 0) { strcpy(user.phone, buffer); }
     
     write_string(client_socket, "Enter new Email (or 'skip'): ");
     read_client_input(client_socket, buffer, MAX_BUFFER);
     if (my_strcmp(buffer, "skip") != 0) { strcpy(user.email, buffer); }
     
     write_string(client_socket, "Enter new Address (or 'skip'): ");
     read_client_input(client_socket, buffer, MAX_BUFFER);
     if (my_strcmp(buffer, "skip") != 0) { strcpy(user.address, buffer); }

    // Only Admin can change role
    if (admin_mode) {
        write_string(client_socket, "Enter new role (0=CUST, 1=EMP, 2=MAN, 3=ADMIN) (or 'skip'): ");
        read_client_input(client_socket, buffer, MAX_BUFFER);
        if (my_strcmp(buffer, "skip") != 0) { 
            int role_val = atoi(buffer);
            if (role_val >= 0 && role_val <= 3) {
                 user.role = (UserRole)role_val; 
            } else {
                 write_string(client_socket, "Invalid role value skipped.\n");
            }
        }
    }

    // Update user using Data Access Layer
    if (updateUser(user) == 0) {
        write_string(client_socket, "User details modified successfully.\n");
    } else {
        write_string(client_socket, "Error modifying user details.\n");
    }
}

void handle_view_customer_transactions(int client_socket) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Customer Account Number (e.g., SB-10001): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    
    // Get account details using Data Access Layer
    Account account = getAccountByNum(buffer);
    if (account.accountId == -1) {
        write_string(client_socket, "Account not found.\n"); return;
    }
    
    // Call the shared history function (needs accountId)
    handle_view_transaction_history(client_socket, account.accountId);
}

void handle_view_assigned_loans(int client_socket, int employeeId) {
    int fd = open(LOAN_FILE, O_RDONLY); // Need direct access for searching
    if (fd == -1) { write_string(client_socket, "No loans found.\n"); return; }

    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char buffer[256];
    int found = 0;
    
    write_string(client_socket, "\n--- Your Assigned Loans ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.assignedToEmployeeId == employeeId && (loan.status == PENDING || loan.status == PROCESSING)) {
            found = 1;
            char* status_str = (loan.status == PENDING) ? "PENDING" : "PROCESSING";
            sprintf(buffer, "Loan ID: %d | Customer ID: %d | Amount: ₹%.2f | Status: %s\n",
                loan.loanId, loan.userId, loan.amount, status_str);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);

    if (!found) { write_string(client_socket, "No assigned loans found.\n"); }
}

void handle_process_loan(int client_socket, int employeeId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Loan ID to process: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int loanId = atoi(buffer);

    Loan loan = getLoan(loanId); // Use Data Access Layer
    if (loan.loanId == -1) { write_string(client_socket, "Loan ID not found.\n"); return; }

    if (loan.assignedToEmployeeId != employeeId) {
        write_string(client_socket, "This loan is not assigned to you.\n");
    } else if (loan.status != PENDING && loan.status != PROCESSING) {
        write_string(client_socket, "This loan has already been processed.\n");
    } else {
        write_string(client_socket, "Choose action: 1 = Approve, 2 = Reject: ");
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        int status_updated = 0;

        if (choice == 1) {
            loan.status = APPROVED;
            // Get the account to deposit into
            Account account = getAccount(loan.accountIdToDeposit);
            if (account.accountId == -1) {
                write_string(client_socket, "Loan approved, but customer account not found!\n");
                // Should we revert loan status? For now, no.
            } else {
                account.balance += loan.amount;
                if (updateAccount(account) == 0) { // Update account
                    // Log transaction
                     Transaction txn;
                     txn.accountId = account.accountId;
                     txn.userId = account.ownerUserId;
                     txn.type = DEPOSIT;
                     txn.amount = loan.amount;
                     txn.newBalance = account.balance;
                     strcpy(txn.otherPartyAccountNumber, "LOAN_CREDIT"); // Indicate source
                     addTransaction(txn);
                    
                    write_string(client_socket, "Loan approved. Amount credited to customer account.\n");
                    status_updated = 1; // Mark success
                } else {
                     write_string(client_socket, "Loan approved, but failed to credit account.\n");
                     // Revert? For now, no.
                }
            }
        } else if (choice == 2) {
            loan.status = REJECTED;
            write_string(client_socket, "Loan rejected.\n");
            status_updated = 1; // Mark success
        } else {
            write_string(client_socket, "Invalid choice. No action taken.\n");
        }
        
        // Update loan status if an action was taken
        if (status_updated) {
            if (updateLoan(loan) != 0) { // Update loan using Data Access Layer
                write_string(client_socket, "Error updating loan status in file.\n");
            }
        }
    }
}