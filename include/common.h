// include/common.h
#ifndef COMMON_H
#define COMMON_H

// --- System Call Headers ---
#include <stdio.h>      // For perror() only
#include <stdlib.h>     // For exit(), atoi()
#include <unistd.h>     // For open, read, write, lseek, close, fork
#include <fcntl.h>      // For fcntl() (locking) and file flags
#include <string.h>     // For strcmp, strcpy, memset
#include <sys/socket.h> // For socket programming
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h>  // For inet_addr, inet_pton
#include <errno.h>      // For errno
#include <sys/types.h>  // For lseek
#include <pthread.h>    // For threads

// --- Project-Specific Definitions ---
#define PORT 8080
#define MAX_BUFFER 1024

// --- File Paths ---
#define USER_FILE "data/users.dat"
#define ACCOUNT_FILE "data/accounts.dat"
#define LOAN_FILE "data/loans.dat"
#define FEEDBACK_FILE "data/feedback.dat"
#define TRANSACTION_FILE "data/transactions.dat"

// --- Data Structures ---

typedef enum {
    CUSTOMER,
    EMPLOYEE,
    MANAGER,
    ADMINISTRATOR
} UserRole;

// --- 1. UPDATED USER STRUCT ---
typedef struct {
    int userId;
    char password[50];
    UserRole role;
    int isActive;
    // --- NEW FIELDS (KYC) ---
    char firstName[50];
    char lastName[50];
    char phone[15];
    char email[100];
    char address[256];
} User;

// --- 2. UPDATED ACCOUNT STRUCT (1-to-Many) ---
typedef struct {
    int accountId;         // Unique ID for the account (1, 2, 3...)
    int ownerUserId;       // ID of the user who owns this (e.g., 2)
    char accountNumber[20]; // The string "SB-10001"
    double balance;
    int isActive;
} Account;

// --- 3. UPDATED TRANSACTION STRUCT (Uses Account Numbers) ---

typedef enum {
    DEPOSIT,
    WITHDRAWAL,
    TRANSFER_OUT,
    TRANSFER_IN
} TransactionType;

typedef struct {
    int transactionId;
    int accountId;      // Which account this transaction belongs to
    int userId;         // Which user performed this
    TransactionType type;
    double amount;
    double newBalance;
    char otherPartyAccountNumber[20]; // e.g., "SB-10002"
} Transaction;


typedef enum {
    PENDING,
    PROCESSING,
    APPROVED,
    REJECTED
} LoanStatus;

typedef struct {
    int loanId;
    int userId;
    int accountIdToDeposit; // NEW: Which account to put the money in
    double amount;
    LoanStatus status;
    int assignedToEmployeeId;
} Loan;

typedef struct {
    int feedbackId;
    int userId;
    char feedbackText[256];
    int isReviewed;
} Feedback;


// --- Function Prototypes (from common_utils.c) ---
void write_string(int fd, const char* str);
int my_strcmp(const char* s1, const char* s2);
int set_file_lock(int fd, int lock_type);
int set_record_lock(int fd, int record_num, int record_size, int lock_type); // <-- Bug fix
int get_next_user_id();
int get_next_account_id(); // <-- NEW
int get_next_loan_id();
int get_next_feedback_id();
int get_next_transaction_id();

// --- Server Function Prototypes ---
User check_login(int userId, char* password);
void* handle_client(void* client_socket_ptr);
int find_user_record(int userId);
int find_account_record_by_id(int accountId); // <-- RENAMED
int find_account_record_by_number(char* acc_num); // <-- NEW
int find_loan_record(int loanId);
int find_feedback_record(int feedbackId);
void read_client_input(int client_socket, char* buffer, int size);
void log_transaction(int accountId, int userId, TransactionType type, double amount, double newBalance, const char* otherPartyAccount); // <-- UPDATED

// --- Menu Prototypes ---
void account_selection_menu(int client_socket, User user); // <-- NEW
void customer_menu(int client_socket, User user, int accountId); // <-- UPDATED
void employee_menu(int client_socket, User user); // <-- UPDATED
void manager_menu(int client_socket, User user); // <-- UPDATED
void admin_menu(int client_socket, User user); // <-- UPDATED

// --- Customer Function Prototypes ---
void handle_view_balance(int client_socket, int accountId); // <-- UPDATED
void handle_deposit(int client_socket, int accountId); // <-- UPDATED
void handle_withdraw(int client_socket, int accountId); // <-- UPDATED
void handle_change_password(int client_socket, int userId); // Unchanged
void handle_transfer_funds(int client_socket, int senderAccountId); // <-- UPDATED
void handle_view_transaction_history(int client_socket, int accountId); // <-- UPDATED
void handle_apply_loan(int client_socket, int userId); // <-- UPDATED (will ask for account)
void handle_view_loan_status(int client_socket, int userId);
void handle_add_feedback(int client_socket, int userId);
void handle_view_feedback_status(int client_socket, int userId);
void handle_view_my_details(int client_socket, User user); // <-- NEW

// --- Employee/Manager/Admin Function Prototypes ---
void handle_add_user(int client_socket, UserRole role_to_add);
void handle_modify_user_details(int client_socket, int admin_mode);
void handle_view_customer_transactions(int client_socket); 
void handle_view_assigned_loans(int client_socket, int employeeId);
void handle_process_loan(int client_socket, int employeeId);
void handle_set_account_status(int client_socket, int admin_mode);
void handle_assign_loan(int client_socket);
void handle_review_feedback(int client_socket);
void handle_add_new_account(int client_socket); // <-- NEW (for employee)

#endif // COMMON_H