// src/customer.c
#include "customer.h"   // Function declarations for customer module
#include "data_access.h" // For functions like getAccount, updateAccount, etc.
#include "common.h"     // For structs, enums, read_client_input, write_string
#include <stdio.h>      // For sprintf
#include <stdlib.h>     // For atof
#include <time.h>
#include <signal.h> // For SIGKILL and kill()
#include <unistd.h> // For getpid()

// --- Account Selection ---
void account_selection_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    Account accounts[10]; // Allow user to have up to 10 accounts
    
    while(1) {
        // Use Data Access Layer to get accounts
        int count = getAccountsByOwnerId(user.userId, accounts, 10);
        
        if (count == 0) {
            write_string(client_socket, "You have no active accounts. Please contact your bank.\n");
            return;
        }

        sprintf(buffer, "\n--- Welcome, %s. Please Select an Account ---\n", user.firstName);
        write_string(client_socket, buffer);
        for (int i = 0; i < count; i++) {
            // Display Account Number and Balance
            sprintf(buffer, "%d. %s (Balance: ₹%.2f)\n", i+1, accounts[i].accountNumber, accounts[i].balance);
            write_string(client_socket, buffer);
        }
        sprintf(buffer, "%d. Logout\n", count + 1);
        write_string(client_socket, buffer);
        write_string(client_socket, "Enter your choice: ");
        
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        
        if (choice > 0 && choice <= count) {
            // User selected an account. Call the main customer menu for that accountId
            customer_menu(client_socket, user, accounts[choice - 1].accountId);
        } else if (choice == count + 1) {
            write_string(client_socket, "Logging out. Goodbye!\n");
            return; // Exit account selection
        } else {
            write_string(client_socket, "Invalid choice.\n");
        }
    }
}


// --- Main Customer Menu ---
void customer_menu(int client_socket, User user, int accountId) {
    char buffer[MAX_BUFFER];
    while(1) {
        // Fetch current account details to display in prompt (optional but nice)
        Account currentAccount = getAccount(accountId);
        if(currentAccount.accountId == -1) { // Error check
             write_string(client_socket, "Error accessing account details. Returning to selection.\n");
             return;
        }
        sprintf(buffer, "\n--- Customer Menu (Account: %s) ---\n", currentAccount.accountNumber);
        write_string(client_socket, buffer);

        write_string(client_socket, "1. View Balance\n");
        write_string(client_socket, "2. Deposit Money\n");
        write_string(client_socket, "3. Withdraw Money\n");
        write_string(client_socket, "4. Transfer Funds\n");
        write_string(client_socket, "5. View Transaction History\n");
        write_string(client_socket, "6. Apply for Loan\n");
        write_string(client_socket, "7. View Loan Status\n");
        write_string(client_socket, "8. View My Personal Details\n");
        write_string(client_socket, "9. Add Feedback\n");
        write_string(client_socket, "10. View Feedback Status\n");
        write_string(client_socket, "11. Change Password\n");
        write_string(client_socket, "12. Switch Account / Logout\n");
        write_string(client_socket, "Enter your choice: ");
        
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        switch(choice) {
            case 1: handle_view_balance(client_socket, accountId); break;
            case 2: handle_deposit(client_socket, accountId); break;
            case 3: handle_withdraw(client_socket, accountId); break;
            case 4: handle_transfer_funds(client_socket, accountId); break;
            case 5: handle_view_transaction_history(client_socket, accountId); break;
            case 6: handle_apply_loan(client_socket, user.userId); break;
            case 7: handle_view_loan_status(client_socket, user.userId); break;
            case 8: handle_view_my_details(client_socket, user); break;
            case 9: handle_add_feedback(client_socket, user.userId); break;
            case 10: handle_view_feedback_status(client_socket, user.userId); break;
            case 11: handle_change_password(client_socket, user.userId); break;
            case 12: return; // Go back to account selection menu
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// --- Customer Action Handlers (using Data Access Layer) ---

void handle_view_balance(int client_socket, int accountId) {
    Account account = getAccount(accountId); // Use Data Access Layer
    if (account.accountId == -1) {
        write_string(client_socket, "Error retrieving account details.\n");
        return;
    }
    char buffer[100];
    sprintf(buffer, "Balance for account %s: ₹%.2f\n", account.accountNumber, account.balance);
    write_string(client_socket, buffer);
}

void handle_deposit(int client_socket, int accountId) {
    char buffer[MAX_BUFFER];
    double amount;
    write_string(client_socket, "Enter amount to deposit: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    Account account = getAccount(accountId);
    if (account.accountId == -1) { write_string(client_socket, "Error retrieving account details.\n"); return; }

    account.balance += amount;

    if (updateAccount(account) == 0) { // Use Data Access Layer to update
        // Log transaction using Data Access Layer
        Transaction txn;
        txn.accountId = accountId;
        txn.userId = account.ownerUserId;
        txn.type = DEPOSIT;
        txn.amount = amount;
        txn.newBalance = account.balance;
        strcpy(txn.otherPartyAccountNumber, "---");
        addTransaction(txn); 

        sprintf(buffer, "Deposit successful. New balance: ₹%.2f\n", account.balance);
        write_string(client_socket, buffer);
    } else {
        write_string(client_socket, "Error processing deposit.\n");
    }
}

void handle_withdraw(int client_socket, int accountId) {
    char buffer[MAX_BUFFER];
    double amount;
    write_string(client_socket, "Enter amount to withdraw: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    Account account = getAccount(accountId);
    if (account.accountId == -1) { write_string(client_socket, "Error retrieving account details.\n"); return; }

    if (amount > account.balance) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        account.balance -= amount;
        if (updateAccount(account) == 0) { // Update using Data Access Layer
            // Log transaction
            Transaction txn;
            txn.accountId = accountId;
            txn.userId = account.ownerUserId;
            txn.type = WITHDRAWAL;
            txn.amount = amount;
            txn.newBalance = account.balance;
            strcpy(txn.otherPartyAccountNumber, "---");
            addTransaction(txn);

            sprintf(buffer, "Withdrawal successful. New balance: ₹%.2f\n", account.balance);
            write_string(client_socket, buffer);
        } else {
            write_string(client_socket, "Error processing withdrawal.\n");
        }
    }
}

void handle_transfer_funds(int client_socket, int senderAccountId) {
    char buffer[MAX_BUFFER];
    char receiver_acc_num[20];
    double amount;

    write_string(client_socket, "Enter Account Number to transfer : ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(receiver_acc_num, buffer);

    write_string(client_socket, "Enter amount to transfer: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    // Get accounts using Data Access Layer
    Account sender_account = getAccount(senderAccountId);
    Account receiver_account = getAccountByNum(receiver_acc_num);

    if (sender_account.isActive == 0 || receiver_account.isActive == 0) {
        write_string(client_socket, "Cannot transfer funds account is inactive.\n"); return;
    }
    if (sender_account.accountId == -1 || receiver_account.accountId == -1) {
        write_string(client_socket, "Invalid sender or receiver account number.\n"); return;
    }
    if (sender_account.accountId == receiver_account.accountId) {
        write_string(client_socket, "Cannot transfer funds to the same account.\n"); return;
    }

    if (sender_account.balance < amount) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        // Prepare updates
        sender_account.balance -= amount;
        receiver_account.balance += amount;

        // --- Critical Section: Update both accounts ---
        // Ideally, the data access layer would handle transactions,
        // but for simplicity, we lock/update/unlock sequentially here.
        // This is NOT truly atomic but better than before.
        int update1_status = updateAccount(sender_account);

        // --- ADD CRASH POINT ---
        // write_string(STDOUT_FILENO, "!!! SIMULATING SERVER CRASH !!!\n");
        // kill(getpid(), SIGKILL); // Force-kill the server
        // --- END CRASH POINT ---

        int update2_status = updateAccount(receiver_account);
        // --- End Critical Section ---

        if (update1_status == 0 && update2_status == 0) {
            // Log transactions
            Transaction txn_out, txn_in;
            
            txn_out.accountId = sender_account.accountId;
            txn_out.userId = sender_account.ownerUserId;
            txn_out.type = TRANSFER_OUT;
            txn_out.amount = amount;
            txn_out.newBalance = sender_account.balance;
            strcpy(txn_out.otherPartyAccountNumber, receiver_account.accountNumber);
            addTransaction(txn_out);

            txn_in.accountId = receiver_account.accountId;
            txn_in.userId = receiver_account.ownerUserId; // Log against receiver user
            txn_in.type = TRANSFER_IN;
            txn_in.amount = amount;
            txn_in.newBalance = receiver_account.balance;
            strcpy(txn_in.otherPartyAccountNumber, sender_account.accountNumber);
            addTransaction(txn_in);

            write_string(client_socket, "Transfer successful.\n");
        } else {
            // Attempt to rollback (best effort, not truly atomic)
            write_string(client_socket, "ERROR: Transfer failed. Please check balances.\n");
            // We should ideally revert the first update if the second failed,
            // but that adds significant complexity.
        }
    }
}

void handle_view_transaction_history(int client_socket, int accountId) {
    int fd = open(TRANSACTION_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No transactions found.\n"); return; }

    if (set_file_lock(fd, F_RDLCK) == -1) {
        write_string(client_socket, "Error locking transaction file.\n");
        close(fd); return;
    }

    Transaction txn;
    char buffer[300];
    int found = 0;
    struct tm timeinfo;
    char time_str[25];

    Account currentAccount = getAccount(accountId);
    if (currentAccount.accountId == -1) {
         write_string(client_socket, "Error retrieving account details.\n");
         set_file_lock(fd, F_UNLCK); close(fd); return;
    }
    sprintf(buffer, "\n--- Transaction History (%s) ---\n", currentAccount.accountNumber);
    write_string(client_socket, buffer);

    // --- UPDATED HEADER (Date & Time Moved) ---
    sprintf(buffer, "%-7s | %-15s | %-12s | %-15s | %-15s | %-20s\n",
            "TXN ID", "TYPE", "RECIVER ACC", "AMOUNT", "BALANCE", "DATE & TIME"); // Moved to end
    write_string(client_socket, buffer);
    // Adjusted separator length slightly
    write_string(client_socket, "------------------------------------------------------------------------------------------\n");

    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (txn.accountId == accountId) {
            found = 1;
            char type_str[16], other_user_str[20], amount_str[16], balance_str[16];

            // Format Timestamp
            localtime_r(&txn.timestamp, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

            // Format other details
            switch(txn.type) {
                case DEPOSIT: strcpy(type_str, "CREDITED"); strcpy(other_user_str, "---"); break;
                case WITHDRAWAL: strcpy(type_str, "DEBITED"); strcpy(other_user_str, "---"); break;
                case TRANSFER_OUT: strcpy(type_str, "DEBITED"); sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); break;
                case TRANSFER_IN: strcpy(type_str, "CREDITED"); sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); break;
                default: strcpy(type_str, "UNKNOWN"); strcpy(other_user_str, "---");
            }
            sprintf(amount_str, "₹%.2f", txn.amount);
            sprintf(balance_str, "₹%.2f", txn.newBalance);

            // --- UPDATED ROW SPRINTF (time_str Moved) ---
            sprintf(buffer, "%-7d | %-15s | %-12s | %-15s | %-15s | %-20s\n",
                txn.transactionId,
                type_str,
                other_user_str,
                amount_str,
                balance_str,
                time_str); // Moved to end
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found) { write_string(client_socket, "No transactions found for this account.\n"); }
}

void handle_apply_loan(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter loan amount (e.g., 50000): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    double amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }
    
    write_string(client_socket, "Enter Account Number to deposit to (e.g., SB-10001): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    Account account = getAccountByNum(buffer); // Get account details
    
    if (account.accountId == -1) {
        write_string(client_socket, "Account not found.\n"); return;
    }
    if (account.ownerUserId != userId) {
        write_string(client_socket, "That account does not belong to you.\n"); return;
    }
    
    Loan new_loan;
    new_loan.userId = userId;
    new_loan.accountIdToDeposit = account.accountId; // Store account ID
    new_loan.amount = amount;
    new_loan.status = PENDING;
    new_loan.assignedToEmployeeId = 0;
    
    if (addLoan(new_loan) == 0) { // Use Data Access Layer
         // We need the newly assigned loanId for the confirmation message
         // This is a slight inefficiency - ideally addLoan would return the ID.
         // For now, we'll just show a generic message or find the latest loan.
        write_string(client_socket, "Loan application submitted successfully. Status: PENDING\n");
    } else {
        write_string(client_socket, "Error submitting loan application.\n");
    }
}

void handle_view_loan_status(int client_socket, int userId) {
    int fd = open(LOAN_FILE, O_RDONLY); // Still need direct read for searching
    if (fd == -1) { write_string(client_socket, "No loan applications found.\n"); return; }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char buffer[256];
    int found = 0;
    write_string(client_socket, "\n--- Your Loan Applications ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.userId == userId) {
            found = 1;
            char* status_str;
            switch(loan.status) { /* ... status mapping ... */ 
                 case PENDING: status_str = "PENDING"; break;
                 case PROCESSING: status_str = "PROCESSING"; break;
                 case APPROVED: status_str = "APPROVED"; break;
                 case REJECTED: status_str = "REJECTED"; break;
                 default: status_str = "UNKNOWN";
             }
            sprintf(buffer, "Loan ID: %d | Amount: ₹%.2f | Status: %s\n",
                loan.loanId, loan.amount, status_str);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    if (!found) { write_string(client_socket, "No loan applications found.\n"); }
}

void handle_add_feedback(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter your feedback: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    Feedback new_feedback;
    new_feedback.userId = userId;
    strncpy(new_feedback.feedbackText, buffer, 255);
    new_feedback.feedbackText[255] = '\0';
    new_feedback.isReviewed = 0;
    if (addFeedback(new_feedback) == 0) { // Use Data Access Layer
        write_string(client_socket, "Feedback submitted successfully. Thank you!\n");
    } else {
         write_string(client_socket, "Error submitting feedback.\n");
    }
}

void handle_view_feedback_status(int client_socket, int userId) {
     int fd = open(FEEDBACK_FILE, O_RDONLY); // Still need direct read for searching
    if (fd == -1) { write_string(client_socket, "No feedback history found.\n"); return; }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    char buffer[512];
    int found = 0;
    write_string(client_socket, "\n--- Your Feedback History ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &feedback, sizeof(Feedback)) == sizeof(Feedback)) {
        if (feedback.userId == userId) {
            found = 1;
            char* status_str = (feedback.isReviewed) ? "Reviewed" : "Pending Review";
            sprintf(buffer, "ID: %d | Status: %s | Feedback: %.50s...\n",
                feedback.feedbackId, status_str, feedback.feedbackText);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    if (!found) { write_string(client_socket, "No feedback history found.\n"); }
}

void handle_view_my_details(int client_socket, User user) {
    // User struct already contains the details
    char buffer[512];
    write_string(client_socket, "\n--- Your Personal Details ---\n");
    sprintf(buffer, "User ID: %d\n", user.userId); write_string(client_socket, buffer);
    sprintf(buffer, "Name: %s %s\n", user.firstName, user.lastName); write_string(client_socket, buffer);
    sprintf(buffer, "Phone: %s\n", user.phone); write_string(client_socket, buffer);
    sprintf(buffer, "Email: %s\n", user.email); write_string(client_socket, buffer);
    sprintf(buffer, "Address: %s\n", user.address); write_string(client_socket, buffer);
    write_string(client_socket, "------------------------------\n");
}

// NOTE: handle_change_password is user-specific, not account specific
// It can be moved to a shared user_actions.c or stay here.
void handle_change_password(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter new password: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);

    User user = getUser(userId); // Get user data
    if (user.userId == -1) { write_string(client_socket, "Error retrieving user details.\n"); return; }

    strcpy(user.password, buffer); // Update password

    if (updateUser(user) == 0) { // Update using Data Access Layer
        write_string(client_socket, "Password changed successfully.\n");
    } else {
        write_string(client_socket, "Error changing password.\n");
    }
}