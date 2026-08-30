#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER 1024
#define PASSWORD_HASH_MAX 128

#define USER_FILE "data/users.dat"
#define ACCOUNT_FILE "data/accounts.dat"
#define LOAN_FILE "data/loans.dat"
#define FEEDBACK_FILE "data/feedback.dat"
#define TRANSACTION_FILE "data/transactions.dat"
#define JOURNAL_FILE "data/journal.log"

typedef int64_t Money;

typedef enum { CUSTOMER, EMPLOYEE, MANAGER, ADMINISTRATOR } UserRole;

typedef struct
{
    int userId;
    char password[PASSWORD_HASH_MAX];
    UserRole role;
    int isActive;
    char firstName[50];
    char lastName[50];
    char phone[15];
    char email[100];
    char address[256];
} User;

typedef struct
{
    int accountId;
    int ownerUserId;
    char accountNumber[20];
    Money balance;
    int isActive;
} Account;

typedef enum { DEPOSIT, WITHDRAWAL, TRANSFER_OUT, TRANSFER_IN } TransactionType;

typedef struct
{
    int transactionId;
    int accountId;
    int userId;
    TransactionType type;
    Money amount;
    Money newBalance;
    char otherPartyAccountNumber[20];
    time_t timestamp;
} Transaction;

typedef enum { PENDING, PROCESSING, APPROVED, REJECTED } LoanStatus;

typedef struct
{
    int loanId;
    int userId;
    int accountIdToDeposit;
    Money amount;
    LoanStatus status;
    int assignedToEmployeeId;
} Loan;

typedef struct
{
    int feedbackId;
    int userId;
    char feedbackText[256];
    int isReviewed;
} Feedback;

typedef enum { TXN_UNDO, TXN_COMMIT } JournalEntryType;

typedef struct
{
    uint64_t transactionId;
    JournalEntryType type;
    int accountId;
    Money oldBalance;
} JournalEntry;

void write_string(int fd, const char *str);
int my_strcmp(const char *s1, const char *s2);
int read_client_input(int client_socket, char *buffer, int size);
int is_valid_number(const char *str);
int is_valid_email(const char *str);
int is_valid_phone(const char *str);
int parse_money(const char *str, Money *out);
void format_money(Money amount, char *buffer, size_t size);
int hash_password(const char *plain_password, char out_hash[PASSWORD_HASH_MAX]);
int verify_password(const char *stored_hash, const char *plain_password);

#endif
