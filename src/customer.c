// src/customer.c
#include "customer.h"   // Function declarations for customer module
#include "data_access.h" // For functions like getAccount, updateAccount, etc.
#include "common.h"     // For structs, enums, read_client_input, write_string
#include <stdio.h>      // For sprintf
#include <stdlib.h>     // For atof
#include <time.h>       // For time/timestamp
#include <signal.h>     // For SIGKILL and kill() (for testing)
#include <unistd.h>     // For getpid()
#include <string.h>     // For strlen, strncpy, etc.

// --- Account Selection ---
void account_selection_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    Account accounts[10]; // Allow user to have up to 10 accounts
    
    while(1) {
        int count = getAccountsByOwnerId(user.userId, accounts, 10);
        
        if (count == 0) {
            write_string(client_socket, "You have no active accounts. Please contact your bank.\n");
            return;
        }

        sprintf(buffer, "\n--- Welcome, %s. Please Select an Account ---\n", user.firstName);
        write_string(client_socket, buffer);
        for (int i = 0; i < count; i++) {
            sprintf(buffer, "%d. %s (Balance: ₹%.2f)\n", i+1, accounts[i].accountNumber, accounts[i].balance);
            write_string(client_socket, buffer);
        }
        sprintf(buffer, "%d. Logout\n", count + 1);
        write_string(client_socket, buffer);
        write_string(client_socket, "Enter your choice: ");
        
        // FIX: Orphaned Session
        if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1) {
            return; // Client disconnected
        }
        int choice = atoi(buffer);
        
        if (choice > 0 && choice <= count) {
            customer_menu(client_socket, user, accounts[choice - 1].accountId);
        } else if (choice == count + 1) {
            write_string(client_socket, "Logging out. Goodbye!\n");
            return;
        } else {
            write_string(client_socket, "Invalid choice.\n");
        }
    }
}


// --- Main Customer Menu ---
void customer_menu(int client_socket, User user, int accountId) {
    char buffer[MAX_BUFFER];
    while(1) {
        Account currentAccount = getAccount(accountId);
        
        // FIX: read() Failure
        if(currentAccount.accountId == -1) {
             write_string(client_socket, "Error accessing account details. Returning to selection.\n");
             return;
        }
        sprintf(buffer, "\n--- Customer Menu (Account: %s) ---\n", currentAccount.accountNumber);
        write_string(client_socket, buffer);

        // ... (Menu options 1-12) ...
        write_string(client_socket, "1. View Balance\n2. Deposit Money\n3. Withdraw Money\n4. Transfer Funds\n5. View Transaction History\n6. Apply for Loan\n7. View Loan Status\n8. View My Personal Details\n9. Add Feedback\n10. View Feedback Status\n11. Change Password\n12. Switch Account / Logout\n");
        write_string(client_socket, "Enter your choice: ");
        
        // FIX: Orphaned Session
        if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1) {
            return; // Client disconnected
        }
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
            case 12: return; // Go back to account selection
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// --- Customer Action Handlers ---

void handle_view_balance(int client_socket, int accountId) {
    Account account = getAccount(accountId);
    
    // FIX: read() Failure
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
    write_string(client_socket, "Enter amount to deposit (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;
    
    // FIX: Invalid Data Type
    if (!is_valid_number(buffer)) {
        write_string(client_socket, "Invalid amount. Must be a positive number.\n");
        return;
    }
    amount = atof(buffer);
    
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    Account account = getAccount(accountId);
    if (account.accountId == -1) { write_string(client_socket, "Error retrieving account details.\n"); return; }

    account.balance += amount;

    // FIX: write() Failure
    if (updateAccount(account) == 0) {
        Transaction txn;
        txn.accountId = accountId;
        txn.userId = account.ownerUserId;
        txn.type = DEPOSIT;
        txn.amount = amount;
        txn.newBalance = account.balance;
        strcpy(txn.otherPartyAccountNumber, "---");
        
        // FIX: write() Failure
        if (addTransaction(txn) != 0) {
            write_string(client_socket, "CRITICAL ERROR: Deposit Succeeded but FAILED to Log Transaction.\n");
        }
        sprintf(buffer, "Deposit successful. New balance: ₹%.2f\n", account.balance);
        write_string(client_socket, buffer);
    } else {
        write_string(client_socket, "Error processing deposit. (Write Failure)\n");
    }
}

void handle_withdraw(int client_socket, int accountId) {
    char buffer[MAX_BUFFER];
    double amount;

    write_string(client_socket, "Enter amount to withdraw (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;

    // FIX: Invalid Data Type
    if (!is_valid_number(buffer)) {
        write_string(client_socket, "Invalid amount. Must be a positive number.\n");
        return;
    }
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    Account account = getAccount(accountId);
    if (account.accountId == -1) { write_string(client_socket, "Error retrieving account details.\n"); return; }

    if (amount > account.balance) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        account.balance -= amount;
        
        // FIX: write() Failure
        if (updateAccount(account) == 0) {
            Transaction txn;
            txn.accountId = accountId;
            txn.userId = account.ownerUserId;
            txn.type = WITHDRAWAL;
            txn.amount = amount;
            txn.newBalance = account.balance;
            strcpy(txn.otherPartyAccountNumber, "---");
            
            // FIX: write() Failure
            if (addTransaction(txn) != 0) {
                 write_string(client_socket, "CRITICAL ERROR: Withdrawal Succeeded but FAILED to Log Transaction.\n");
            }

            sprintf(buffer, "Withdrawal successful. New balance: ₹%.2f\n", account.balance);
            write_string(client_socket, buffer);
        } else {
            write_string(client_socket, "Error processing withdrawal. (Write Failure)\n");
        }
    }
}

void handle_transfer_funds(int client_socket, int senderAccountId) {
    char buffer[MAX_BUFFER];
    char receiver_acc_num[20];
    double amount;

    write_string(client_socket, "Enter Account Number to transfer to (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;
    
    // FIX: Buffer Overflow / Empty Input
    if (strlen(buffer) == 0 || strlen(buffer) >= 20) {
        write_string(client_socket, "Invalid account number format. Aborting.\n");
        return;
    }
    strcpy(receiver_acc_num, buffer);

    write_string(client_socket, "Enter amount to transfer: ");
    if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1) return; // Orphaned Session
    
    // FIX: Invalid Data Type
    if (!is_valid_number(buffer)) {
        write_string(client_socket, "Invalid amount. Must be a positive number.\n");
        return;
    }
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    Account sender_account = getAccount(senderAccountId);
    Account receiver_account = getAccountByNum(receiver_acc_num);

    // FIX: read() Failure
    if (sender_account.accountId == -1 || receiver_account.accountId == -1) {
        write_string(client_socket, "Invalid sender or receiver account number.\n"); return;
    }
    
    // FIX: State Validation
    if (sender_account.isActive == 0 || receiver_account.isActive == 0) {
        write_string(client_socket, "Cannot transfer funds: one or both accounts are inactive.\n"); 
        return;
    }
    
    if (sender_account.accountId == receiver_account.accountId) {
        write_string(client_socket, "Cannot transfer funds to the same account.\n"); return;
    }

    if (sender_account.balance < amount) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        sender_account.balance -= amount;
        receiver_account.balance += amount;

        int update1_status = updateAccount(sender_account);
        // Crash point for testing Atomicity (Journaling) would go here
        int update2_status = updateAccount(receiver_account);

        // FIX: write() Failure
        if (update1_status == 0 && update2_status == 0) {
            Transaction txn_out, txn_in;
            // ... (setup txn_out) ...
            txn_out.accountId = sender_account.accountId;
            txn_out.userId = sender_account.ownerUserId;
            txn_out.type = TRANSFER_OUT;
            txn_out.amount = amount;
            txn_out.newBalance = sender_account.balance;
            strcpy(txn_out.otherPartyAccountNumber, receiver_account.accountNumber);
            
            // ... (setup txn_in) ...
            txn_in.accountId = receiver_account.accountId;
            txn_in.userId = receiver_account.ownerUserId; 
            txn_in.type = TRANSFER_IN;
            txn_in.amount = amount;
            txn_in.newBalance = receiver_account.balance;
            strcpy(txn_in.otherPartyAccountNumber, sender_account.accountNumber);

            // FIX: write() Failure
            if (addTransaction(txn_out) != 0 || addTransaction(txn_in) != 0) {
                write_string(client_socket, "CRITICAL ERROR: Transfer Succeeded but FAILED to Log Transaction.\n");
            }
            write_string(client_socket, "Transfer successful.\n");
        } else {
            // This is the Atomicity failure case
            write_string(client_socket, "ERROR: Transfer failed. Balances may be inconsistent. Please contact support.\n");
            // We should roll back, but since we didn't implement journaling, we just warn.
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
    // FIX: read() Failure
    if (currentAccount.accountId == -1) {
         write_string(client_socket, "Error retrieving account details.\n");
         set_file_lock(fd, F_UNLCK); close(fd); return;
    }
    sprintf(buffer, "\n--- Transaction History (%s) ---\n", currentAccount.accountNumber);
    write_string(client_socket, buffer);

    // ... (sprintf header) ...
    sprintf(buffer, "%-7s | %-20s | %-15s | %-12s | %-15s | %-15s\n", "TXN ID", "DATE & TIME", "TYPE", "RECIVER ACC", "AMOUNT", "BALANCE");
    write_string(client_socket, buffer);
    write_string(client_socket, "------------------------------------------------------------------------------------------\n");

    lseek(fd, 0, SEEK_SET);
    
    // FIX: read() Failure
    while (read(fd, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (txn.accountId == accountId) {
            found = 1;
            char type_str[16], other_user_str[20], amount_str[16], balance_str[16];
            localtime_r(&txn.timestamp, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            
            switch(txn.type) {
                // ... (case DEPOSIT, WITHDRAWAL, TRANSFER_OUT, TRANSFER_IN, default) ...
                case DEPOSIT: strcpy(type_str, "CREDITED"); strcpy(other_user_str, "---"); break;
                case WITHDRAWAL: strcpy(type_str, "DEBITED"); strcpy(other_user_str, "---"); break;
                case TRANSFER_OUT: strcpy(type_str, "DEBITED"); sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); break;
                case TRANSFER_IN: strcpy(type_str, "CREDITED"); sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); break;
                default: strcpy(type_str, "UNKNOWN"); strcpy(other_user_str, "---");
            }
            sprintf(amount_str, "₹%.2f", txn.amount);
            sprintf(balance_str, "₹%.2f", txn.newBalance);

            sprintf(buffer, "%-7d | %-20s | %-15s | %-12s | %-15s | %-15s\n",
                txn.transactionId, time_str, type_str, other_user_str, amount_str, balance_str);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    if (!found) { write_string(client_socket, "No transactions found for this account.\n"); }
}

void handle_apply_loan(int client_socket, int userId) {
    char buffer[MAX_BUFFER];

    write_string(client_socket, "Enter loan amount (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;
    
    // FIX: Invalid Data Type
    if (!is_valid_number(buffer)) {
        write_string(client_socket, "Invalid amount. Must be a positive number.\n");
        return;
    }
    double amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }
    
    write_string(client_socket, "Enter Account Number to deposit to (e.g., SB-10001): ");
    if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1) return; // Orphaned Session
    
    // FIX: Buffer Overflow / Empty Input
    if (strlen(buffer) == 0 || strlen(buffer) >= 20) {
        write_string(client_socket, "Invalid account number format. Aborting.\n");
        return;
    }
    
    Account account = getAccountByNum(buffer);
    if (account.accountId == -1) { write_string(client_socket, "Account not found.\n"); return; }
    if (account.ownerUserId != userId) {
        write_string(client_socket, "That account does not belong to you.\n"); return;
    }
    
    Loan new_loan;
    new_loan.userId = userId;
    new_loan.accountIdToDeposit = account.accountId;
    new_loan.amount = amount;
    new_loan.status = PENDING;
    new_loan.assignedToEmployeeId = 0;
    
    // FIX: write() Failure
    if (addLoan(new_loan) == 0) {
        write_string(client_socket, "Loan application submitted successfully. Status: PENDING\n");
    } else {
        write_string(client_socket, "Error submitting loan application. (Write Failure)\n");
    }
}

void handle_view_loan_status(int client_socket, int userId) {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No loan applications found.\n"); return; }
    if (set_file_lock(fd, F_RDLCK) == -1) { close(fd); return; }
    
    Loan loan;
    char buffer[256];
    int found = 0;
    write_string(client_socket, "\n--- Your Loan Applications ---\n");
    lseek(fd, 0, SEEK_SET);
    
    // FIX: read() Failure
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.userId == userId) {
            found = 1;
            char* status_str;
            switch(loan.status) {
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

    write_string(client_socket, "Enter your feedback (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;
    
    // FIX: Empty Input
    if (strlen(buffer) == 0) {
        write_string(client_socket, "Empty feedback cannot be submitted.\n");
        return;
    }
    // FIX: Buffer Overflow (strncpy handles it, but good to check)
    if (strlen(buffer) >= 256) {
        write_string(client_socket, "Feedback is too long, it will be truncated.\n");
        // We let strncpy handle the truncation
    }

    Feedback new_feedback;
    new_feedback.userId = userId;
    strncpy(new_feedback.feedbackText, buffer, 255);
    new_feedback.feedbackText[255] = '\0'; // Ensure null termination
    new_feedback.isReviewed = 0;
    
    // FIX: write() Failure
    if (addFeedback(new_feedback) == 0) {
        write_string(client_socket, "Feedback submitted successfully. Thank you!\n");
    } else {
         write_string(client_socket, "Error submitting feedback. (Write Failure)\n");
    }
}

void handle_view_feedback_status(int client_socket, int userId) {
     int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No feedback history found.\n"); return; }
    if (set_file_lock(fd, F_RDLCK) == -1) { close(fd); return; }
    
    Feedback feedback;
    char buffer[512];
    int found = 0;
    write_string(client_socket, "\n--- Your Feedback History ---\n");
    lseek(fd, 0, SEEK_SET);
    
    // FIX: read() Failure
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
    // This function is safe as it just reads from the passed 'user' struct
    char buffer[512];
    write_string(client_socket, "\n--- Your Personal Details ---\n");
    sprintf(buffer, "User ID: %d\n", user.userId); write_string(client_socket, buffer);
    sprintf(buffer, "Name: %s %s\n", user.firstName, user.lastName); write_string(client_socket, buffer);
    sprintf(buffer, "Phone: %s\n", user.phone); write_string(client_socket, buffer);
    sprintf(buffer, "Email: %s\n", user.email); write_string(client_socket, buffer);
    sprintf(buffer, "Address: %s\n", user.address); write_string(client_socket, buffer);
    write_string(client_socket, "------------------------------\n");
}

void handle_change_password(int client_socket, int userId) {
    char buffer[MAX_BUFFER];

    write_string(client_socket, "Enter new password (or '0' to cancel): ");
    if(read_client_input(client_socket,buffer,MAX_BUFFER) == -1)return;
    if(my_strcmp(buffer,"0") == 0)return;
    
    // FIX: Empty Input / Buffer Overflow
    if (strlen(buffer) == 0) {
        write_string(client_socket, "Password cannot be empty.\n");
        return;
    }
    if (strlen(buffer) >= 50) { // Max length of password field is 50
        write_string(client_socket, "Password is too long. Max 49 characters.\n");
        return;
    }

    User user = getUser(userId);
    if (user.userId == -1) { write_string(client_socket, "Error retrieving user details.\n"); return; }

    strcpy(user.password, buffer);

    // FIX: write() Failure
    if (updateUser(user) == 0) {
        write_string(client_socket, "Password changed successfully.\n");
    } else {
        write_string(client_socket, "Error changing password. (Write Failure)\n");
    }
}