// src/manager.c
#include "manager.h"    // Function declarations for manager module
#include "customer.h"   // For shared handle_view_my_details, handle_change_password
#include "data_access.h" // For data access functions
#include "common.h"     // For structs, enums, read_client_input, write_string
#include <stdio.h>      // For sprintf
#include <stdlib.h>     // For atoi

// --- Manager Menu ---
void manager_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    while(1) {
        sprintf(buffer, "\n--- Manager Menu (User: %s %s) ---\n", user.firstName, user.lastName);
        write_string(client_socket, buffer);
        write_string(client_socket, "1. Activate/Deactivate Customer & Accounts\n");
        write_string(client_socket, "2. Assign Loan to Employee\n");
        write_string(client_socket, "3. Review Customer Feedback\n");
        write_string(client_socket, "4. View My Personal Details\n");
        write_string(client_socket, "5. Change My Password\n");
        write_string(client_socket, "6. Logout\n");
        write_string(client_socket, "Enter your choice: ");

        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        switch(choice) {
            case 1: handle_set_account_status(client_socket, 0); break; // 0 = Not admin
            case 2: handle_assign_loan(client_socket); break;
            case 3: handle_review_feedback(client_socket); break;
            case 4: handle_view_my_details(client_socket, user); break; // Shared function
            case 5: handle_change_password(client_socket, user.userId); break; // Shared function
            case 6: write_string(client_socket, "Logging out. Goodbye!\n"); return;
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// --- Manager Action Handlers (using Data Access Layer) ---

// Shared with Admin, implemented here
// In src/manager.c (and src/admin.c if applicable)

void handle_set_account_status(int client_socket, int admin_mode) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter User ID to modify status: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int target_user_id = atoi(buffer);

    write_string(client_socket, "Enter status (1=Active, 0=Deactivated): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int new_status = atoi(buffer);
    if (new_status != 0 && new_status != 1) {
        write_string(client_socket, "Invalid status. Use 1 for Active, 0 for Deactivated.\n");
        return;
    }

    // --- Update User Status (Same as before) ---
    User user = getUser(target_user_id);
    if (user.userId == -1) { write_string(client_socket, "User not found.\n"); return; }

    if (!admin_mode && user.role != CUSTOMER) {
        write_string(client_socket, "Permission denied. Managers can only modify customers.\n");
        return;
    }

    user.isActive = new_status;
    if (updateUser(user) != 0) {
        write_string(client_socket, "Error updating user status record.\n");
        // We might still try to update accounts
    } else {
         write_string(client_socket, "User status updated successfully.\n");
    }


    // --- CORRECTED: Update ALL accounts owned by this user ---
    int fd_acct = open(ACCOUNT_FILE, O_RDWR); // Open Read/Write
    if (fd_acct == -1) {
        write_string(client_socket, "Could not open account file to update account statuses.\n");
        return;
    }

    if (set_file_lock(fd_acct, F_WRLCK) == -1) { // Lock whole file for modification loop
         write_string(client_socket, "Could not lock account file.\n");
         close(fd_acct);
         return;
    }

    Account account;
    int record_num = 0;
    int update_count = 0;
    int error_count = 0;
    lseek(fd_acct, 0, SEEK_SET); // Rewind to start

    // Loop through all account records
    while (read(fd_acct, &account, sizeof(Account)) == sizeof(Account)) {
        if (account.ownerUserId == target_user_id) {
            // Found an account belonging to the user
            if (account.isActive != new_status) { // Only update if needed
                 account.isActive = new_status;
                 // Seek back to the beginning of this record to overwrite it
                 if (lseek(fd_acct, record_num * sizeof(Account), SEEK_SET) == -1) {
                     perror("lseek back failed"); error_count++;
                 } else {
                     // Write the updated record
                     if (write(fd_acct, &account, sizeof(Account)) != sizeof(Account)) {
                         perror("write update failed"); error_count++;
                     } else {
                         update_count++;
                     }
                 }
                 // Seek forward to the start of the *next* record to continue reading
                 if (lseek(fd_acct, (record_num + 1) * sizeof(Account), SEEK_SET) == -1 && errno != 0) {
                      // Check errno, lseek returns -1 at EOF which is okay unless read fails next
                      perror("lseek forward failed");
                      // This might indicate a bigger problem, maybe break?
                 }
             }
        }
        record_num++;
    } // End while loop reading accounts

    set_file_lock(fd_acct, F_UNLCK); // Unlock the file
    close(fd_acct);

    if (error_count > 0) {
         sprintf(buffer, "Updated %d account(s) with %d errors.\n", update_count, error_count);
         write_string(client_socket, buffer);
    } else if (update_count > 0){
         sprintf(buffer, "Successfully updated status for %d account(s).\n", update_count);
         write_string(client_socket, buffer);
    } else {
         write_string(client_socket, "No account statuses needed updating.\n");
    }
    // --- End Corrected Account Update ---
}

void handle_assign_loan(int client_socket) {
    char buffer[MAX_BUFFER];
    int found = 0;

    // --- Display unassigned loans ---
    int fd_loan = open(LOAN_FILE, O_RDONLY); // Need direct read for searching
    if (fd_loan == -1) { write_string(client_socket, "No loans found or error opening file.\n"); return; }

    set_file_lock(fd_loan, F_RDLCK);
    Loan loan;
    write_string(client_socket, "\n--- Unassigned Loans (Status: PENDING) ---\n");
    lseek(fd_loan, 0, SEEK_SET); // Rewind
    while (read(fd_loan, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.assignedToEmployeeId == 0 && loan.status == PENDING) {
            found = 1;
            sprintf(buffer, "Loan ID: %d | Customer ID: %d | Amount: ₹%.2f\n",
                    loan.loanId, loan.userId, loan.amount);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd_loan, F_UNLCK);
    close(fd_loan); // Close after reading

    if (!found) {
        write_string(client_socket, "No unassigned loans found.\n");
        return;
    }
    // --- End display ---

    // Get input for assignment
    write_string(client_socket, "Enter Loan ID to assign: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int loanId = atoi(buffer);

    write_string(client_socket, "Enter Employee ID to assign to: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int employeeId = atoi(buffer);

    // Validate Employee
    User employee = getUser(employeeId);
    if (employee.userId == -1 || employee.role != EMPLOYEE) {
        write_string(client_socket, "Invalid Employee ID.\n");
        return;
    }

    // Get Loan and update
    Loan loan_to_update = getLoan(loanId);
    if (loan_to_update.loanId == -1) {
        write_string(client_socket, "Loan not found.\n");
        return;
    }

    if (loan_to_update.assignedToEmployeeId != 0 || loan_to_update.status != PENDING) {
        write_string(client_socket, "Loan cannot be assigned (already assigned or processed).\n");
    } else {
        loan_to_update.assignedToEmployeeId = employeeId;
        loan_to_update.status = PROCESSING; // Mark as processing
        if (updateLoan(loan_to_update) == 0) { // Update using Data Access Layer
            write_string(client_socket, "Loan assigned successfully.\n");
        } else {
            write_string(client_socket, "Error assigning loan.\n");
        }
    }
}


void handle_review_feedback(int client_socket) {
    char buffer[MAX_BUFFER];
    int found = 0;

    // --- Display unreviewed feedback ---
    int fd_feedback = open(FEEDBACK_FILE, O_RDONLY); // Need direct read for searching
    if (fd_feedback == -1) { write_string(client_socket, "No feedback found or error opening file.\n"); return; }

    set_file_lock(fd_feedback, F_RDLCK);
    Feedback feedback;
    write_string(client_socket, "\n--- Unreviewed Feedback ---\n");
    lseek(fd_feedback, 0, SEEK_SET); // Rewind
    while (read(fd_feedback, &feedback, sizeof(Feedback)) == sizeof(Feedback)) {
        if (feedback.isReviewed == 0) {
            found = 1;
            sprintf(buffer, "ID: %d | User: %d | Feedback: %.100s...\n",
                    feedback.feedbackId, feedback.userId, feedback.feedbackText);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd_feedback, F_UNLCK);
    close(fd_feedback); // Close after reading

    if (!found) {
        write_string(client_socket, "No unreviewed feedback found.\n");
        return;
    }
    // --- End display ---

    write_string(client_socket, "Enter Feedback ID to mark as reviewed: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int feedbackId = atoi(buffer);

    Feedback feedback_to_update = getFeedback(feedbackId); // Get data
    if (feedback_to_update.feedbackId == -1) {
        write_string(client_socket, "Feedback ID not found.\n");
        return;
    }

    if (feedback_to_update.isReviewed == 1) {
        write_string(client_socket, "Feedback already marked as reviewed.\n");
    } else {
        feedback_to_update.isReviewed = 1; // Mark as reviewed
        if (updateFeedback(feedback_to_update) == 0) { // Update using Data Access Layer
            write_string(client_socket, "Feedback marked as reviewed.\n");
        } else {
            write_string(client_socket, "Error marking feedback as reviewed.\n");
        }
    }
}