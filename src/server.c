// src/server.c
#include "common.h"
#include <pthread.h> // For threading

// --- NEW: Session Management Globals ---
#define MAX_SESSIONS 10000 // Maximum concurrent logged-in users
int activeUserIds[MAX_SESSIONS];
int activeUserCount = 0;
pthread_mutex_t sessionMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect the list



// --- HELPER: Read line from client ---
void read_client_input(int client_socket, char* buffer, int size) {
    int read_size = read(client_socket, buffer, size - 1);
    if (read_size > 0) {
        buffer[read_size] = '\0';
        if (buffer[read_size - 1] == '\n') {
            buffer[read_size - 1] = '\0';
        }
    }
}

// --- HELPER: Generate a new account number ---
void generate_new_account_number(char* new_acc_num) {
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1) {
        strcpy(new_acc_num, "SB-10001"); // Start from 10001
        return;
    }
    set_file_lock(fd, F_RDLCK);

    if (lseek(fd, -sizeof(Account), SEEK_END) == -1) {
        // File is empty
        strcpy(new_acc_num, "SB-10001");
    } else {
        Account last_account;
        read(fd, &last_account, sizeof(Account));
        // Simple increment: extract number, add 1, format back
        int last_num = atoi(last_account.accountNumber + 3); // Skip "SB-"
        sprintf(new_acc_num, "SB-%d", last_num + 1);
    }
    
    set_file_lock(fd, F_UNLCK);
    close(fd);
}

// --- Record-Finding Functions ---

int find_user_record(int userId) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) { perror("open user file"); return -1; }
    set_file_lock(fd, F_RDLCK);
    User user;
    int record_num = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (user.userId == userId) {
            set_file_lock(fd, F_UNLCK); close(fd); return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return -1;
}

// Finds account by its unique PRIMARY KEY (accountId)
int find_account_record_by_id(int accountId) {
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1) { perror("open account file"); return -1; }
    set_file_lock(fd, F_RDLCK);
    Account account;
    int record_num = 0;
    while (read(fd, &account, sizeof(Account)) == sizeof(Account)) {
        if (account.accountId == accountId) {
            set_file_lock(fd, F_UNLCK); close(fd); return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return -1;
}

// NEW: Find account by string "SB-10001"
int find_account_record_by_number(char* acc_num) {
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1) { return -1; }
    set_file_lock(fd, F_RDLCK);
    Account account;
    int record_num = 0;
    while (read(fd, &account, sizeof(Account)) == sizeof(Account)) {
        if (my_strcmp(account.accountNumber, acc_num) == 0) {
            set_file_lock(fd, F_UNLCK); close(fd); return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return -1;
}

int find_loan_record(int loanId) {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1) { perror("open loan file"); return -1; }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    int record_num = 0;
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.loanId == loanId) {
            set_file_lock(fd, F_UNLCK); close(fd); return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return -1;
}

int find_feedback_record(int feedbackId) {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) { perror("open feedback file"); return -1; }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    int record_num = 0;
    while (read(fd, &feedback, sizeof(Feedback)) == sizeof(Feedback)) {
        if (feedback.feedbackId == feedbackId) {
            set_file_lock(fd, F_UNLCK); close(fd); return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return -1;
}

// --- Login Function ---
User check_login(int userId, char* password) {
    User user_to_find;
    user_to_find.userId = 0; 
    int record_num = find_user_record(userId);
    if (record_num == -1) { return user_to_find; }

    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) { user_to_find.userId = -1; return user_to_find; }

    // Use fixed 4-argument function
    set_record_lock(fd, record_num, sizeof(User), F_RDLCK);
    lseek(fd, record_num * sizeof(User), SEEK_SET);
    
    User user;
    if (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (user.userId == userId && my_strcmp(user.password, password) == 0) {
            if (user.isActive) {
                user_to_find = user;
            } else {
                user_to_find.userId = -2;
            }
        }
    }
    set_record_lock(fd, record_num, sizeof(User), F_UNLCK);
    close(fd);
    return user_to_find;
}

// --- Transaction Functions ---

// UPDATED: Now takes accountId and a string for other party
void log_transaction(int accountId, int userId, TransactionType type, double amount, double newBalance, const char* otherPartyAccount) {
    int fd = open(TRANSACTION_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("Could not open transaction file"); return; }

    set_file_lock(fd, F_WRLCK);

    Transaction txn;
    txn.transactionId = get_next_transaction_id();
    txn.accountId = accountId;
    txn.userId = userId;
    txn.type = type;
    txn.amount = amount;
    txn.newBalance = newBalance;
    strcpy(txn.otherPartyAccountNumber, otherPartyAccount); // <-- UPDATED

    write(fd, &txn, sizeof(Transaction));
    set_file_lock(fd, F_UNLCK);
    close(fd);
}

// UPDATED: Now takes accountId and formats to your spec
void handle_view_transaction_history(int client_socket, int accountId) {
    int fd = open(TRANSACTION_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No transactions found.\n"); return; }

    set_file_lock(fd, F_RDLCK);
    Transaction txn;
    char buffer[256];
    int found = 0;
    
    write_string(client_socket, "\n--- Transaction History ---\n");
    sprintf(buffer, "%-7s | %-15s | %-12s | %-15s | %-15s\n", 
            "TXN ID", "TYPE", "RECIVER ACC", "AMOUNT", "BALANCE");
    write_string(client_socket, buffer);
    write_string(client_socket, "--------------------------------------------------------------------------\n");

    while (read(fd, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (txn.accountId == accountId) { // Check by accountId
            found = 1;
            char type_str[16], other_user_str[20], amount_str[16], balance_str[16];
            switch(txn.type) {
                case DEPOSIT: 
                    strcpy(type_str, "CREDITED"); 
                    strcpy(other_user_str, "---");
                    sprintf(amount_str, "₹%.2f", txn.amount);
                    break;
                case WITHDRAWAL: 
                    strcpy(type_str, "DEBITED");
                    strcpy(other_user_str, "---");
                    sprintf(amount_str, "₹%.2f", txn.amount);
                    break;
                case TRANSFER_OUT: 
                    strcpy(type_str, "DEBITED");
                    sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); // <-- UPDATED
                    sprintf(amount_str, "₹%.2f", txn.amount);
                    break;
                case TRANSFER_IN: 
                    strcpy(type_str, "CREDITED"); 
                    sprintf(other_user_str, "%s", txn.otherPartyAccountNumber); // <-- UPDATED
                    sprintf(amount_str, "₹%.2f", txn.amount);
                    break;
                default: 
                    strcpy(type_str, "UNKNOWN");
                    strcpy(other_user_str, "---");
                    sprintf(amount_str, "₹%.2f", txn.amount);
            }
            sprintf(balance_str, "₹%.2f", txn.newBalance);
            sprintf(buffer, "%-7d | %-15s | %-12s | %-15s | %-15s\n",
                txn.transactionId, type_str, other_user_str, amount_str, balance_str);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    if (!found) { write_string(client_socket, "No transactions found for this account.\n"); }
}

// --- Customer Handlers ---

// UPDATED: Now takes accountId
void handle_view_balance(int client_socket, int accountId) {
    int record_num = find_account_record_by_id(accountId);
    if (record_num == -1) { write_string(client_socket, "Error: Account not found.\n"); return; }
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "Error: Could not access account data.\n"); return; }

    set_record_lock(fd, record_num, sizeof(Account), F_RDLCK);
    lseek(fd, record_num * sizeof(Account), SEEK_SET);
    Account account;
    read(fd, &account, sizeof(Account));
    set_record_lock(fd, record_num, sizeof(Account), F_UNLCK);
    close(fd);

    char buffer[100];
    sprintf(buffer, "Balance for account %s: ₹%.2f\n", account.accountNumber, account.balance);
    write_string(client_socket, buffer);
}

// UPDATED: Now takes accountId
void handle_deposit(int client_socket, int accountId) {
    char buffer[MAX_BUFFER];
    double amount;
    write_string(client_socket, "Enter amount to deposit: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    int record_num = find_account_record_by_id(accountId);
    if (record_num == -1) { write_string(client_socket, "Error: Account not found.\n"); return; }
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd == -1) { write_string(client_socket, "Error: Could not access account data.\n"); return; }

    set_record_lock(fd, record_num, sizeof(Account), F_WRLCK);
    Account account;
    lseek(fd, record_num * sizeof(Account), SEEK_SET);
    read(fd, &account, sizeof(Account));
    account.balance += amount;
    lseek(fd, record_num * sizeof(Account), SEEK_SET);
    write(fd, &account, sizeof(Account));
    set_record_lock(fd, record_num, sizeof(Account), F_UNLCK);
    close(fd);
    
    log_transaction(accountId, account.ownerUserId, DEPOSIT, amount, account.balance, "---");
    sprintf(buffer, "Deposit successful. New balance: ₹%.2f\n", account.balance);
    write_string(client_socket, buffer);
}

// UPDATED: Now takes accountId
void handle_withdraw(int client_socket, int accountId) {
    char buffer[MAX_BUFFER];
    double amount;
    write_string(client_socket, "Enter amount to withdraw: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    int record_num = find_account_record_by_id(accountId);
    if (record_num == -1) { write_string(client_socket, "Error: Account not found.\n"); return; }
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd == -1) { write_string(client_socket, "Error: Could not access account data.\n"); return; }

    set_record_lock(fd, record_num, sizeof(Account), F_WRLCK);
    Account account;
    lseek(fd, record_num * sizeof(Account), SEEK_SET);
    read(fd, &account, sizeof(Account));
    if (amount > account.balance) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        account.balance -= amount;
        lseek(fd, record_num * sizeof(Account), SEEK_SET);
        write(fd, &account, sizeof(Account));
        log_transaction(accountId, account.ownerUserId, WITHDRAWAL, amount, account.balance, "---");
        sprintf(buffer, "Withdrawal successful. New balance: ₹%.2f\n", account.balance);
        write_string(client_socket, buffer);
    }
    set_record_lock(fd, record_num, sizeof(Account), F_UNLCK);
    close(fd);
}

// UPDATED: Now takes senderAccountId and asks for receiver Account Number
void handle_transfer_funds(int client_socket, int senderAccountId) {
    char buffer[MAX_BUFFER];
    char receiver_acc_num[20];
    double amount;

    write_string(client_socket, "Enter Account Number to transfer to (e.g., SB-10001): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(receiver_acc_num, buffer);

    write_string(client_socket, "Enter amount to transfer: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }

    int sender_rec_num = find_account_record_by_id(senderAccountId);
    int receiver_rec_num = find_account_record_by_number(receiver_acc_num);

    if (sender_rec_num == -1 || receiver_rec_num == -1) {
        write_string(client_socket, "Invalid sender or receiver account number.\n"); return;
    }
    if (sender_rec_num == receiver_rec_num) {
        write_string(client_socket, "Cannot transfer funds to the same account.\n"); return;
    }

    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd == -1) { write_string(client_socket, "Error accessing account data.\n"); return; }

    // Deadlock Prevention: Lock records in consistent order
    int rec1 = (sender_rec_num < receiver_rec_num) ? sender_rec_num : receiver_rec_num;
    int rec2 = (sender_rec_num > receiver_rec_num) ? sender_rec_num : receiver_rec_num;
    set_record_lock(fd, rec1, sizeof(Account), F_WRLCK);
    set_record_lock(fd, rec2, sizeof(Account), F_WRLCK);
    
    Account sender_account, receiver_account;
    lseek(fd, sender_rec_num * sizeof(Account), SEEK_SET);
    read(fd, &sender_account, sizeof(Account));
    lseek(fd, receiver_rec_num * sizeof(Account), SEEK_SET);
    read(fd, &receiver_account, sizeof(Account));

    if (sender_account.balance < amount) {
        write_string(client_socket, "Insufficient funds.\n");
    } else {
        sender_account.balance -= amount;
        receiver_account.balance += amount;

        lseek(fd, sender_rec_num * sizeof(Account), SEEK_SET);
        write(fd, &sender_account, sizeof(Account));
        lseek(fd, receiver_rec_num * sizeof(Account), SEEK_SET);
        write(fd, &receiver_account, sizeof(Account));
        
        // Log with account numbers
        log_transaction(receiver_account.accountId, receiver_account.ownerUserId, TRANSFER_IN, amount, receiver_account.balance, sender_account.accountNumber);
        log_transaction(sender_account.accountId, sender_account.ownerUserId, TRANSFER_OUT, amount, sender_account.balance, receiver_account.accountNumber);
        
        write_string(client_socket, "Transfer successful.\n");
    }
    
    set_record_lock(fd, rec1, sizeof(Account), F_UNLCK);
    set_record_lock(fd, rec2, sizeof(Account), F_UNLCK);
    close(fd);
}

// NEW: Show user their own details
void handle_view_my_details(int client_socket, User user) {
    char buffer[512];
    write_string(client_socket, "\n--- Your Personal Details ---\n");
    sprintf(buffer, "User ID: %d\n", user.userId); write_string(client_socket, buffer);
    sprintf(buffer, "Name: %s %s\n", user.firstName, user.lastName); write_string(client_socket, buffer);
    sprintf(buffer, "Phone: %s\n", user.phone); write_string(client_socket, buffer);
    sprintf(buffer, "Email: %s\n", user.email); write_string(client_socket, buffer);
    sprintf(buffer, "Address: %s\n", user.address); write_string(client_socket, buffer);
    write_string(client_socket, "------------------------------\n");
}

// Unchanged (password is not account-specific)
void handle_change_password(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter new password: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int record_num = find_user_record(userId);
    if (record_num == -1) { write_string(client_socket, "Error: User not found.\n"); return; }
    int fd = open(USER_FILE, O_RDWR);
    if (fd == -1) { write_string(client_socket, "Error: Could not access user data.\n"); return; }
    set_record_lock(fd, record_num, sizeof(User), F_WRLCK);
    User user;
    lseek(fd, record_num * sizeof(User), SEEK_SET);
    read(fd, &user, sizeof(User));
    strcpy(user.password, buffer);
    lseek(fd, record_num * sizeof(User), SEEK_SET);
    write(fd, &user, sizeof(User));
    set_record_lock(fd, record_num, sizeof(User), F_UNLCK);
    close(fd);
    write_string(client_socket, "Password changed successfully.\n");
}

// --- Loan/Feedback Handlers (Slightly Updated) ---

// UPDATED: Now asks which account to deposit loan into
void handle_apply_loan(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter loan amount (e.g., 50000): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    double amount = atof(buffer);
    if (amount <= 0) { write_string(client_socket, "Invalid amount.\n"); return; }
    
    // Ask for account
    write_string(client_socket, "Enter Account Number to deposit to (e.g., SB-10001): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int rec_num = find_account_record_by_number(buffer);
    if (rec_num == -1) {
        write_string(client_socket, "Account not found.\n"); return;
    }
    
    // Get the accountId
    int fd_acct = open(ACCOUNT_FILE, O_RDONLY);
    set_record_lock(fd_acct, rec_num, sizeof(Account), F_RDLCK);
    lseek(fd_acct, rec_num * sizeof(Account), SEEK_SET);
    Account account;
    read(fd_acct, &account, sizeof(Account));
    set_record_lock(fd_acct, rec_num, sizeof(Account), F_UNLCK);
    close(fd_acct);
    
    if (account.ownerUserId != userId) {
        write_string(client_socket, "That account does not belong to you.\n"); return;
    }
    
    Loan new_loan;
    new_loan.loanId = get_next_loan_id();
    new_loan.userId = userId;
    new_loan.accountIdToDeposit = account.accountId; // <-- STORED
    new_loan.amount = amount;
    new_loan.status = PENDING;
    new_loan.assignedToEmployeeId = 0;
    
    int fd_loan = open(LOAN_FILE, O_WRONLY | O_APPEND);
    if (fd_loan == -1) { write_string(client_socket, "Error submitting loan application.\n"); return; }
    set_file_lock(fd_loan, F_WRLCK);
    write(fd_loan, &new_loan, sizeof(Loan));
    set_file_lock(fd_loan, F_UNLCK);
    close(fd_loan);

    sprintf(buffer, "Loan application (ID: %d) submitted. Status: PENDING\n", new_loan.loanId);
    write_string(client_socket, buffer);
}

// Unchanged (shows all loans for a user)
void handle_view_loan_status(int client_socket, int userId) {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No loan applications found.\n"); return; }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char buffer[256];
    int found = 0;
    write_string(client_socket, "\n--- Your Loan Applications ---\n");
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

// Unchanged (feedback is user-level)
void handle_add_feedback(int client_socket, int userId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter your feedback: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    Feedback new_feedback;
    new_feedback.feedbackId = get_next_feedback_id();
    new_feedback.userId = userId;
    strncpy(new_feedback.feedbackText, buffer, 255);
    new_feedback.feedbackText[255] = '\0';
    new_feedback.isReviewed = 0;
    int fd = open(FEEDBACK_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) { write_string(client_socket, "Error submitting feedback.\n"); return; }
    set_file_lock(fd, F_WRLCK);
    write(fd, &new_feedback, sizeof(Feedback));
    set_file_lock(fd, F_UNLCK); close(fd);
    write_string(client_socket, "Feedback submitted successfully. Thank you!\n");
}

// Unchanged
void handle_view_feedback_status(int client_socket, int userId) {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) { write_string(client_socket, "No feedback history found.\n"); return; }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    char buffer[512];
    int found = 0;
    write_string(client_socket, "\n--- Your Feedback History ---\n");
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

// --- Admin/Employee/Manager Handlers ---

// UPDATED: Now takes all the new user fields
void handle_add_user(int client_socket, UserRole role_to_add) {
    char buffer[MAX_BUFFER];
    User new_user;
    new_user.userId = get_next_user_id();
    new_user.role = role_to_add;
    new_user.isActive = 1;

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

    int fd_user = open(USER_FILE, O_WRONLY | O_APPEND);
    if (fd_user == -1) { write_string(client_socket, "Error opening user file.\n"); return; }
    set_file_lock(fd_user, F_WRLCK);
    write(fd_user, &new_user, sizeof(User));
    set_file_lock(fd_user, F_UNLCK);
    close(fd_user);

    // If it's a customer, create their first bank account
    if (role_to_add == CUSTOMER) {
        Account new_account;
        new_account.accountId = get_next_account_id(); // Use account ID generator
        new_account.ownerUserId = new_user.userId;
        new_account.balance = 0.0;
        new_account.isActive = 1;
        generate_new_account_number(new_account.accountNumber); // Generate "SB-10003" etc.

        int fd_acct = open(ACCOUNT_FILE, O_WRONLY | O_APPEND);
        if (fd_acct == -1) { write_string(client_socket, "Error opening account file.\n"); return; }
        set_file_lock(fd_acct, F_WRLCK);
        write(fd_acct, &new_account, sizeof(Account));
        set_file_lock(fd_acct, F_UNLCK);
        close(fd_acct);
        
        sprintf(buffer, "User created successfully. New User ID: %d, New Account Number: %s\n", new_user.userId, new_account.accountNumber);
        write_string(client_socket, buffer);
    } else {
        sprintf(buffer, "User created successfully. New User ID: %d\n", new_user.userId);
        write_string(client_socket, buffer);
    }
}

// NEW: For employees to add more accounts to existing customers
void handle_add_new_account(int client_socket) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Customer User ID to add account to: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int cust_id = atoi(buffer);
    
    int rec_num = find_user_record(cust_id);
    if (rec_num == -1) { write_string(client_socket, "User not found.\n"); return; }

    // (Could check if user is a CUSTOMER, skipping for brevity)
    
    Account new_account;
    new_account.accountId = get_next_account_id();
    new_account.ownerUserId = cust_id;
    new_account.balance = 0.0;
    new_account.isActive = 1;
    generate_new_account_number(new_account.accountNumber); // Generate "SB-10004" etc.

    int fd_acct = open(ACCOUNT_FILE, O_WRONLY | O_APPEND);
    if (fd_acct == -1) { write_string(client_socket, "Error opening account file.\n"); return; }
    set_file_lock(fd_acct, F_WRLCK);
    write(fd_acct, &new_account, sizeof(Account));
    set_file_lock(fd_acct, F_UNLCK);
    close(fd_acct);
    
    sprintf(buffer, "New account %s created successfully for User ID %d.\n", new_account.accountNumber, cust_id);
    write_string(client_socket, buffer);
}

// UPDATED: To allow changing new KYC fields
void handle_modify_user_details(int client_socket, int admin_mode) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter User ID to modify: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int target_user_id = atoi(buffer);
    int record_num = find_user_record(target_user_id);
    if (record_num == -1) { write_string(client_socket, "User not found.\n"); return; }

    int fd = open(USER_FILE, O_RDWR);
    set_record_lock(fd, record_num, sizeof(User), F_WRLCK);
    lseek(fd, record_num * sizeof(User), SEEK_SET);
    User user;
    read(fd, &user, sizeof(User));

    if (!admin_mode && user.role != CUSTOMER) {
        write_string(client_socket, "Permission denied. Employees can only modify customers.\n");
        set_record_lock(fd, record_num, sizeof(User), F_UNLCK); close(fd); return;
    }

    write_string(client_socket, "Enter new password (or 'skip'): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    if (my_strcmp(buffer, "skip") != 0) { strcpy(user.password, buffer); }
    
    write_string(client_socket, "Enter new First Name (or 'skip'): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    if (my_strcmp(buffer, "skip") != 0) { strcpy(user.firstName, buffer); }
    
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

    if (admin_mode) {
        write_string(client_socket, "Enter new role (0=CUST, 1=EMP, 2=MAN, 3=ADMIN) (or 'skip'): ");
        read_client_input(client_socket, buffer, MAX_BUFFER);
        if (my_strcmp(buffer, "skip") != 0) { user.role = atoi(buffer); }
    }

    lseek(fd, record_num * sizeof(User), SEEK_SET);
    write(fd, &user, sizeof(User));
    set_record_lock(fd, record_num, sizeof(User), F_UNLCK);
    close(fd);
    write_string(client_socket, "User details modified successfully.\n");
}

// UPDATED: Now searches for all accounts owned by a user
void handle_set_account_status(int client_socket, int admin_mode) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter User ID to modify status: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int target_user_id = atoi(buffer);
    write_string(client_socket, "Enter status (1=Active, 0=Deactivated): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int new_status = atoi(buffer);

    int user_rec_num = find_user_record(target_user_id);
    if (user_rec_num == -1) { write_string(client_socket, "User not found.\n"); return; }
    
    int fd_user = open(USER_FILE, O_RDWR);
    set_record_lock(fd_user, user_rec_num, sizeof(User), F_WRLCK);
    lseek(fd_user, user_rec_num * sizeof(User), SEEK_SET);
    User user; read(fd_user, &user, sizeof(User));
    if (!admin_mode && user.role != CUSTOMER) {
        write_string(client_socket, "Permission denied. Managers can only modify customers.\n");
        set_record_lock(fd_user, user_rec_num, sizeof(User), F_UNLCK); close(fd_user); return;
    }
    user.isActive = new_status;
    lseek(fd_user, user_rec_num * sizeof(User), SEEK_SET);
    write(fd_user, &user, sizeof(User));
    set_record_lock(fd_user, user_rec_num, sizeof(User), F_UNLCK);
    close(fd_user);

    // Now update ALL accounts owned by this user
    int fd_acct = open(ACCOUNT_FILE, O_RDWR);
    if (fd_acct == -1) { write_string(client_socket, "User status updated, but couldn't open account file.\n"); return; }
    
    set_file_lock(fd_acct, F_WRLCK); // Lock whole file
    Account account;
    int record_num = 0;
    lseek(fd_acct, 0, SEEK_SET); // Rewind
    while(read(fd_acct, &account, sizeof(Account)) == sizeof(Account)) {
        if (account.ownerUserId == target_user_id) {
            account.isActive = new_status;
            // Go back and write the change
            lseek(fd_acct, record_num * sizeof(Account), SEEK_SET);
            write(fd_acct, &account, sizeof(Account));
            // Move forward again to continue reading
            lseek(fd_acct, (record_num + 1) * sizeof(Account), SEEK_SET);
        }
        record_num++;
    }
    set_file_lock(fd_acct, F_UNLCK);
    close(fd_acct);
    
    write_string(client_socket, "User and all their accounts updated successfully.\n");
}

// UPDATED: Now takes customer accountId to look up transactions
void handle_view_customer_transactions(int client_socket) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Customer Account Number (e.g., SB-10001): ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    
    int rec_num = find_account_record_by_number(buffer);
    if (rec_num == -1) {
        write_string(client_socket, "Account not found.\n"); return;
    }
    
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    set_record_lock(fd, rec_num, sizeof(Account), F_RDLCK);
    lseek(fd, rec_num * sizeof(Account), SEEK_SET);
    Account account;
    read(fd, &account, sizeof(Account));
    set_record_lock(fd, rec_num, sizeof(Account), F_UNLCK);
    close(fd);
    
    // Just re-use the same function
    handle_view_transaction_history(client_socket, account.accountId);
}

// UPDATED: Now deposits to the specified account
void handle_process_loan(int client_socket, int employeeId) {
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter Loan ID to process: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int loanId = atoi(buffer);
    int rec_num = find_loan_record(loanId);
    if (rec_num == -1) { write_string(client_socket, "Loan ID not found.\n"); return; }
    int fd = open(LOAN_FILE, O_RDWR);
    if (fd == -1) { write_string(client_socket, "Error accessing loan data.\n"); return; }
    set_record_lock(fd, rec_num, sizeof(Loan), F_WRLCK);
    lseek(fd, rec_num * sizeof(Loan), SEEK_SET);
    Loan loan;
    read(fd, &loan, sizeof(Loan));
    if (loan.assignedToEmployeeId != employeeId) {
        write_string(client_socket, "This loan is not assigned to you.\n");
    } else if (loan.status != PENDING && loan.status != PROCESSING) {
        write_string(client_socket, "This loan has already been processed.\n");
    } else {
        write_string(client_socket, "Choose action: 1 = Approve, 2 = Reject: ");
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        if (choice == 1) {
            loan.status = APPROVED;
            int account_rec_num = find_account_record_by_id(loan.accountIdToDeposit);
            if (account_rec_num == -1) {
                write_string(client_socket, "Loan approved, but customer account not found!\n");
            } else {
                int fd_acct = open(ACCOUNT_FILE, O_RDWR);
                set_record_lock(fd_acct, account_rec_num, sizeof(Account), F_WRLCK);
                Account account;
                lseek(fd_acct, account_rec_num * sizeof(Account), SEEK_SET);
                read(fd_acct, &account, sizeof(Account));
                account.balance += loan.amount;
                lseek(fd_acct, account_rec_num * sizeof(Account), SEEK_SET);
                write(fd_acct, &account, sizeof(Account));
                set_record_lock(fd_acct, account_rec_num, sizeof(Account), F_UNLCK);
                close(fd_acct);
                log_transaction(account.accountId, account.ownerUserId, DEPOSIT, loan.amount, account.balance, "LOAN_CREDIT");
                write_string(client_socket, "Loan approved. Amount credited to customer account.\n");
            }
        } else if (choice == 2) {
            loan.status = REJECTED;
            write_string(client_socket, "Loan rejected.\n");
        } else {
            write_string(client_socket, "Invalid choice. No action taken.\n");
        }
        lseek(fd, rec_num * sizeof(Loan), SEEK_SET);
        write(fd, &loan, sizeof(Loan));
    }
    set_record_lock(fd, rec_num, sizeof(Loan), F_UNLCK);
    close(fd);
}

// Unchanged
void handle_view_assigned_loans(int client_socket, int employeeId) {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1) {
        write_string(client_socket, "No loans found.\n");
        return;
    }

    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char buffer[256];
    int found = 0;
    
    write_string(client_socket, "\n--- Your Assigned Loans ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        // Show loans assigned to this employee that are not yet approved/rejected
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

    if (!found) {
        write_string(client_socket, "No assigned loans found.\n");
    }
}
void handle_assign_loan(int client_socket) {
    int fd = open(LOAN_FILE, O_RDWR);
    if (fd == -1) {
        write_string(client_socket, "No loans found.\n");
        return;
    }

    // First, display unassigned loans
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char buffer[256];
    int found = 0;
    
    write_string(client_socket, "\n--- Unassigned Loans (Status: PENDING) ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind file
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.assignedToEmployeeId == 0 && loan.status == PENDING) {
            found = 1;
            sprintf(buffer, "Loan ID: %d | Customer ID: %d | Amount: ₹%.2f\n",
                loan.loanId, loan.userId, loan.amount);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK);

    if (!found) {
        write_string(client_socket, "No unassigned loans found.\n");
        close(fd);
        return;
    }

    // Now, assign one
    write_string(client_socket, "Enter Loan ID to assign: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int loanId = atoi(buffer);

    write_string(client_socket, "Enter Employee ID to assign to: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int employeeId = atoi(buffer);

    // Verify employee exists and is an employee
    int emp_rec_num = find_user_record(employeeId);
    if (emp_rec_num == -1) {
        write_string(client_socket, "Employee not found.\n");
        close(fd);
        return;
    }
    // (We'd also check their role here, but we'll skip for brevity)

    int loan_rec_num = find_loan_record(loanId);
    if (loan_rec_num == -1) {
        write_string(client_socket, "Loan not found.\n");
        close(fd);
        return;
    }

    // Lock and update the loan record
    set_record_lock(fd, loan_rec_num, sizeof(Loan), F_WRLCK);
    lseek(fd, loan_rec_num * sizeof(Loan), SEEK_SET);
    read(fd, &loan, sizeof(Loan));
    
    // Check if already assigned or processed
    if (loan.assignedToEmployeeId != 0 || loan.status != PENDING) {
         write_string(client_socket, "Loan cannot be assigned (already assigned or processed).\n");
    } else {
        loan.assignedToEmployeeId = employeeId;
        loan.status = PROCESSING; // Mark as processing now
        lseek(fd, loan_rec_num * sizeof(Loan), SEEK_SET);
        write(fd, &loan, sizeof(Loan));
        write_string(client_socket, "Loan assigned successfully.\n");
    }

    set_record_lock(fd, loan_rec_num, sizeof(Loan), F_UNLCK);
    close(fd);
}
void handle_review_feedback(int client_socket) {
    int fd = open(FEEDBACK_FILE, O_RDWR);
    if (fd == -1) {
        write_string(client_socket, "No feedback found.\n");
        return;
    }

    // First, display unreviewed feedback
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    char buffer[512];
    int found = 0;
    
    write_string(client_socket, "\n--- Unreviewed Feedback ---\n");
    lseek(fd, 0, SEEK_SET); // Rewind
    while (read(fd, &feedback, sizeof(Feedback)) == sizeof(Feedback)) {
        if (feedback.isReviewed == 0) {
            found = 1;
            sprintf(buffer, "ID: %d | User: %d | Feedback: %.100s...\n",
                feedback.feedbackId, feedback.userId, feedback.feedbackText);
            write_string(client_socket, buffer);
        }
    }
    set_file_lock(fd, F_UNLCK);

    if (!found) {
        write_string(client_socket, "No unreviewed feedback found.\n");
        close(fd);
        return;
    }

    write_string(client_socket, "Enter Feedback ID to mark as reviewed: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    int feedbackId = atoi(buffer);

    int rec_num = find_feedback_record(feedbackId);
    if (rec_num == -1) {
        write_string(client_socket, "Feedback ID not found.\n");
        close(fd);
        return;
    }

    // Lock and update
    set_record_lock(fd, rec_num, sizeof(Feedback), F_WRLCK);
    lseek(fd, rec_num * sizeof(Feedback), SEEK_SET);
    read(fd, &feedback, sizeof(Feedback));

    if (feedback.isReviewed == 1) {
        write_string(client_socket, "Feedback already marked as reviewed.\n");
    } else {
        feedback.isReviewed = 1; // Mark as reviewed
        lseek(fd, rec_num * sizeof(Feedback), SEEK_SET);
        write(fd, &feedback, sizeof(Feedback));
        write_string(client_socket, "Feedback marked as reviewed.\n");
    }

    set_record_lock(fd, rec_num, sizeof(Feedback), F_UNLCK);
    close(fd);
}


// --- MENUS ---

// NEW: Account Selection Menu
void account_selection_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    Account accounts[10]; // Allow user to have up to 10 accounts
    int count = 0;
    
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd != -1) {
        set_file_lock(fd, F_RDLCK);
        Account account;
        while (read(fd, &account, sizeof(Account)) == sizeof(Account)) {
            if (account.ownerUserId == user.userId && account.isActive) {
                accounts[count] = account;
                count++;
                if (count >= 10) break;
            }
        }
        set_file_lock(fd, F_UNLCK);
        close(fd);
    }
    
    if (count == 0) {
        write_string(client_socket, "You have no active accounts. Please contact your bank.\n");
        return;
    }
    
    while(1) {
        sprintf(buffer, "\n--- Welcome, %s. Please Select an Account ---\n", user.firstName);
        write_string(client_socket, buffer);
        for (int i = 0; i < count; i++) {
            sprintf(buffer, "%d. %s (Balance: ₹%.2f)\n", i+1, accounts[i].accountNumber, accounts[i].balance);
            write_string(client_socket, buffer);
        }
        sprintf(buffer, "%d. Logout\n", count + 1);
        write_string(client_socket, buffer);
        write_string(client_socket, "Enter your choice: ");
        
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        
        if (choice > 0 && choice <= count) {
            // User selected an account. Pass the user AND the chosen accountId
            customer_menu(client_socket, user, accounts[choice - 1].accountId);
        } else if (choice == count + 1) {
            write_string(client_socket, "Logging out. Goodbye!\n");
            return;
        } else {
            write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// UPDATED: Now takes the selected accountId
void customer_menu(int client_socket, User user, int accountId) {
    char buffer[MAX_BUFFER];
    while(1) {
        write_string(client_socket, "\n--- Customer Menu ---\n");
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

// UPDATED: Now takes User struct
void employee_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    while(1) {
        write_string(client_socket, "\n--- Employee Menu ---\n");
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
            case 3: handle_modify_user_details(client_socket, 0); break;
            case 4: handle_view_customer_transactions(client_socket); break;
            case 5: handle_view_assigned_loans(client_socket, user.userId); break;
            case 6: handle_process_loan(client_socket, user.userId); break;
            case 7: handle_view_my_details(client_socket, user); break;
            case 8: handle_change_password(client_socket, user.userId); break;
            case 9: write_string(client_socket, "Logging out. Goodbye!\n"); return;
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// UPDATED
void manager_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    while(1) {
        write_string(client_socket, "\n--- Manager Menu ---\n");
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
            case 1: handle_set_account_status(client_socket, 0); break;
            case 2: handle_assign_loan(client_socket); break;
            case 3: handle_review_feedback(client_socket); break;
            case 4: handle_view_my_details(client_socket, user); break;
            case 5: handle_change_password(client_socket, user.userId); break;
            case 6: write_string(client_socket, "Logging out. Goodbye!\n"); return;
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

// UPDATED
void admin_menu(int client_socket, User user) {
    char buffer[MAX_BUFFER];
    while(1) {
        write_string(client_socket, "\n--- Admin Menu ---\n");
        write_string(client_socket, "1. Add User (Employee/Manager/Customer)\n");
        write_string(client_socket, "2. Modify User Details (Password/Role/KYC)\n");
        write_string(client_socket, "3. Activate/Deactivate Any User & Accounts\n");
        write_string(client_socket, "4. View My Personal Details\n");
        write_string(client_socket, "5. Change My Password\n");
        write_string(client_socket, "6. Logout\n");
        write_string(client_socket, "Enter your choice: ");
        
        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        switch(choice) {
            case 1: 
                write_string(client_socket, "Enter role (0=CUST, 1=EMP, 2=MAN): ");
                read_client_input(client_socket, buffer, MAX_BUFFER);
                int role = atoi(buffer);
                if (role >= 0 && role <= 2) handle_add_user(client_socket, (UserRole)role);
                else write_string(client_socket, "Invalid role.\n");
                break;
            case 2: handle_modify_user_details(client_socket, 1); break;
            case 3: handle_set_account_status(client_socket, 1); break;
            case 4: handle_view_my_details(client_socket, user); break;
            case 5: handle_change_password(client_socket, user.userId); break;
            case 6: write_string(client_socket, "Logging out. Goodbye!\n"); return;
            default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}


// --- Main Client Handler (UPDATED) ---
// src/server.c

// src/server.c

// src/server.c

// src/server.c

void* handle_client(void* client_socket_ptr) {
    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);

    char buffer[MAX_BUFFER];
    User user;
    user.userId = -1; // Default to invalid/not logged in
    int roleChoice = 0;
    int loginSuccess = 0; // Flag to track if login fully succeeded for cleanup

    write_string(client_socket, "Welcome to the Bank!\n");

    // --- Role Selection Loop ---
    while(1) {
        write_string(client_socket, "Please select your role to log in:\n");
        write_string(client_socket, " 1. Administrator\n 2. Manager\n 3. Employee\n 4. Customer\n");
        write_string(client_socket, "Enter choice (1-4): ");
        read_client_input(client_socket, buffer, MAX_BUFFER);
        roleChoice = atoi(buffer);
        if (roleChoice >= 1 && roleChoice <= 4) break;
        else write_string(client_socket, "Invalid choice. Please try again.\n");
    }

    // --- Get Credentials ---
    int userIdInput;
    char password[50];
    write_string(client_socket, "Enter User ID: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    userIdInput = atoi(buffer);
    write_string(client_socket, "Enter Password: ");
    read_client_input(client_socket, buffer, MAX_BUFFER);
    strcpy(password, buffer);

    // --- Authentication ---
    user = check_login(userIdInput, password);

    // --- Verification and Session Check ---
    if (user.userId <= 0) { // check_login failed (bad credentials, error, or deactivated)
        if (user.userId == -2) {
            write_string(STDOUT_FILENO, "Login failed (deactivated)\n");
            write_string(client_socket, "Account is deactivated. Contact your bank.\n");
        } else {
            write_string(STDOUT_FILENO, "Login failed (invalid credentials)\n");
            write_string(client_socket, "Invalid User ID or Password.\n");
        }
        // Login failed - skip to cleanup
    } else if (user.role != (UserRole)(4-roleChoice)) { // Role mismatch
        write_string(STDOUT_FILENO, "Login failed: Role mismatch.\n");
        write_string(client_socket, "Login failed: Your User ID does not match the selected role.\n");
        user.userId = 0; // Mark as failed for cleanup logic
        // Login failed - skip to cleanup
    } else { // Credentials OK, Role OK - Now check session
        pthread_mutex_lock(&sessionMutex);
        int alreadyLoggedIn = 0;
        for (int i = 0; i < activeUserCount; i++) {
            if (activeUserIds[i] == user.userId) {
                alreadyLoggedIn = 1;
                break;
            }
        }

        if (alreadyLoggedIn) {
            pthread_mutex_unlock(&sessionMutex);
            write_string(STDOUT_FILENO, "Login failed: User already logged in.\n");
            write_string(client_socket, "ERROR: This user is already logged in elsewhere.\n");
            user.userId = 0; // Mark as failed for cleanup
            // Login failed - skip to cleanup
        } else if (activeUserCount >= MAX_SESSIONS) {
            pthread_mutex_unlock(&sessionMutex);
            write_string(STDOUT_FILENO, "Login failed: Server full.\n");
            write_string(client_socket, "ERROR: Server is currently full. Please try again later.\n");
            user.userId = 0; // Mark as failed for cleanup
            // Login failed - skip to cleanup
        } else {
            // *** SUCCESS CASE ***
            // Add user to session list
            activeUserIds[activeUserCount] = user.userId;
            activeUserCount++;
            pthread_mutex_unlock(&sessionMutex);
            loginSuccess = 1; // Set the success flag

            write_string(STDOUT_FILENO, "Login success, session added.\n");
            write_string(client_socket, "Login Successful!\n"); // Send success message ONLY here

            // --- Menu Dispatch ---
            switch (user.role) {
                case CUSTOMER: account_selection_menu(client_socket, user); break;
                case EMPLOYEE: employee_menu(client_socket, user); break;
                case MANAGER: manager_menu(client_socket, user); break;
                case ADMINISTRATOR: admin_menu(client_socket, user); break;
            }
            // After menu returns, execution continues to cleanup...
        }
    }

    // --- Session Cleanup ---
    // Only remove from session list if login fully succeeded (loginSuccess flag is 1)
    if (loginSuccess == 1) {
        pthread_mutex_lock(&sessionMutex);
        for (int i = 0; i < activeUserCount; i++) {
            if (activeUserIds[i] == user.userId) {
                activeUserIds[i] = activeUserIds[activeUserCount - 1];
                activeUserCount--;
                write_string(STDOUT_FILENO, "Session removed.\n");
                break;
            }
        }
        pthread_mutex_unlock(&sessionMutex);
    }

    close(client_socket);
    write_string(STDOUT_FILENO, "Client session ended.\n");
    return NULL;
}


// --- Main Server Setup (Threaded) ---
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed"); exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    write_string(STDOUT_FILENO, "Server listening on port 8080 (Threaded Mode)...\n");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept"); continue;
        }

        int* client_sock_ptr = (int*)malloc(sizeof(int));
        *client_sock_ptr = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_sock_ptr) < 0) {
            perror("pthread_create failed");
            close(new_socket);
            free(client_sock_ptr);
        } else {
            pthread_detach(thread_id);
            write_string(STDOUT_FILENO, "New client connected, thread created.\n");
        }
    }
    
    close(server_fd);
    return 0;
}