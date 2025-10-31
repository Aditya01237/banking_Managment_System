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
#include <time.h>

// --- Project-Specific Definitions ---
#define PORT 8080
#define MAX_BUFFER 1024

// --- File Paths ---
#define USER_FILE "data/users.dat"
#define ACCOUNT_FILE "data/accounts.dat"
#define LOAN_FILE "data/loans.dat"
#define FEEDBACK_FILE "data/feedback.dat"
#define TRANSACTION_FILE "data/transactions.dat"

// --- Data Structures (Unchanged from our last refactor) ---

typedef enum {
    CUSTOMER, EMPLOYEE, MANAGER, ADMINISTRATOR
} UserRole;

typedef struct {
    int userId; char password[50]; UserRole role; int isActive;
    char firstName[50]; char lastName[50]; char phone[15];
    char email[100]; char address[256];
} User;

typedef struct {
    int accountId; int ownerUserId; char accountNumber[20];
    double balance; int isActive;
} Account;

typedef enum { DEPOSIT, WITHDRAWAL, TRANSFER_OUT, TRANSFER_IN } TransactionType;

typedef struct {
    int transactionId;
    int accountId;
    int userId;
    TransactionType type;
    double amount;
    double newBalance;
    char otherPartyAccountNumber[20];
    time_t timestamp; // <-- ADD THIS FIELD (stores seconds since epoch)
} Transaction;

typedef enum { PENDING, PROCESSING, APPROVED, REJECTED } LoanStatus;

typedef struct {
    int loanId; int userId; int accountIdToDeposit; double amount;
    LoanStatus status; int assignedToEmployeeId;
} Loan;

typedef struct {
    int feedbackId; int userId; char feedbackText[256]; int isReviewed;
} Feedback;

// --- Generic Utility Function Prototypes ---
// (These could potentially move to a utils.h/utils.c)
void write_string(int fd, const char* str);
int my_strcmp(const char* s1, const char* s2);
void read_client_input(int client_socket, char* buffer, int size);


#endif // COMMON_H