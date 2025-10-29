// src/common_utils.c
#include "common.h"

// --- I/O and String Functions (Unchanged) ---

void write_string(int fd, const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    write(fd, str, len);
}

int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// --- Locking Functions ---

int set_file_lock(int fd, int lock_type) {
    struct flock fl;
    fl.l_type = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl file lock");
        return -1;
    }
    return 0;
}

// --- BUG FIX: Added lock_type as an argument ---
int set_record_lock(int fd, int record_num, int record_size, int lock_type) {
    struct flock fl;
    fl.l_type = lock_type; // Now this variable exists
    fl.l_whence = SEEK_SET;
    fl.l_start = record_num * record_size;
    fl.l_len = record_size;
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl record lock");
        return -1;
    }
    return 0;
}

// --- ID Generation Functions ---

int get_next_user_id() {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) { return 1; }

    set_file_lock(fd, F_RDLCK);
    if (lseek(fd, -sizeof(User), SEEK_END) == -1) {
        set_file_lock(fd, F_UNLCK); close(fd); return 1;
    }
    User last_user;
    if (read(fd, &last_user, sizeof(User)) == sizeof(User)) {
        set_file_lock(fd, F_UNLCK); close(fd);
        return last_user.userId + 1;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return 1;
}

// --- NEW FUNCTION ---
int get_next_account_id() {
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1) { return 1; } 

    set_file_lock(fd, F_RDLCK);
    if (lseek(fd, -sizeof(Account), SEEK_END) == -1) {
        set_file_lock(fd, F_UNLCK); close(fd); return 1;
    }
    Account last_account;
    if (read(fd, &last_account, sizeof(Account)) == sizeof(Account)) {
        set_file_lock(fd, F_UNLCK); close(fd);
        return last_account.accountId + 1;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return 1; 
}

int get_next_loan_id() {
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1) { return 1; } 

    set_file_lock(fd, F_RDLCK);
    if (lseek(fd, -sizeof(Loan), SEEK_END) == -1) {
        set_file_lock(fd, F_UNLCK); close(fd); return 1;
    }
    Loan last_loan;
    if (read(fd, &last_loan, sizeof(Loan)) == sizeof(Loan)) {
        set_file_lock(fd, F_UNLCK); close(fd);
        return last_loan.loanId + 1;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return 1; 
}

int get_next_feedback_id() {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) { return 1; } 

    set_file_lock(fd, F_RDLCK);
    if (lseek(fd, -sizeof(Feedback), SEEK_END) == -1) {
        set_file_lock(fd, F_UNLCK); close(fd); return 1;
    }
    Feedback last_feedback;
    if (read(fd, &last_feedback, sizeof(Feedback)) == sizeof(Feedback)) {
        set_file_lock(fd, F_UNLCK); close(fd);
        return last_feedback.feedbackId + 1;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return 1; 
}

int get_next_transaction_id() {
    int fd = open(TRANSACTION_FILE, O_RDONLY);
    if (fd == -1) { return 1; } 

    set_file_lock(fd, F_RDLCK);
    if (lseek(fd, -sizeof(Transaction), SEEK_END) == -1) {
        set_file_lock(fd, F_UNLCK); close(fd); return 1;
    }
    Transaction last_txn;
    if (read(fd, &last_txn, sizeof(Transaction)) == sizeof(Transaction)) {
        set_file_lock(fd, F_UNLCK); close(fd);
        return last_txn.transactionId + 1;
    }
    set_file_lock(fd, F_UNLCK); close(fd);
    return 1; 
}