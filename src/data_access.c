#include "data_access.h" 
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>  
#include <stdlib.h> 

// --- Locking Functions ---

int set_file_lock(int fd, int lock_type)
{
    struct flock fl;
    fl.l_type = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();
    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        perror("fcntl file lock");
        return -1;
    }
    return 0;
}

int set_record_lock(int fd, int record_num, int record_size, int lock_type)
{
    struct flock fl;
    fl.l_type = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = record_num * record_size;
    fl.l_len = record_size;
    fl.l_pid = getpid();
    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        perror("fcntl record lock");
        return -1;
    }
    return 0;
}


// Helper for ID generation
int get_next_id_from_file(const char *filename, size_t record_size)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return 1;
    } 

    set_file_lock(fd, F_RDLCK);
    off_t offset = lseek(fd, -record_size, SEEK_END);
    int next_id = 1;

    if (offset != -1)
    { 
        int last_id;
        if (read(fd, &last_id, sizeof(int)) == sizeof(int))
        {
            next_id = last_id + 1;
        }
    }

    set_file_lock(fd, F_UNLCK);
    close(fd);
    return next_id;
}

int get_next_user_id() { return get_next_id_from_file(USER_FILE, sizeof(User)); }
int get_next_account_id() { return get_next_id_from_file(ACCOUNT_FILE, sizeof(Account)); }
int get_next_loan_id() { return get_next_id_from_file(LOAN_FILE, sizeof(Loan)); }
int get_next_feedback_id() { return get_next_id_from_file(FEEDBACK_FILE, sizeof(Feedback)); }
int get_next_transaction_id() { return get_next_id_from_file(TRANSACTION_FILE, sizeof(Transaction)); }

// This function's purpose is to find the record number (position) of a
// user within the USER_FILE, not to retrieve their data.
int find_user_record(int userId)
{
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    set_file_lock(fd, F_RDLCK);
    User user;
    int record_num = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User))
    {
        if (user.userId == userId)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_account_record_by_id(int accountId)
{
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    set_file_lock(fd, F_RDLCK);
    Account account;
    int record_num = 0;
    while (read(fd, &account, sizeof(Account)) == sizeof(Account))
    {
        if (account.accountId == accountId)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_account_record_by_number(char *acc_num)
{
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    set_file_lock(fd, F_RDLCK);
    Account account;
    int record_num = 0;
    while (read(fd, &account, sizeof(Account)) == sizeof(Account))
    {
        if (my_strcmp(account.accountNumber, acc_num) == 0)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_loan_record(int loanId)
{
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    int record_num = 0;
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan))
    {
        if (loan.loanId == loanId)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_feedback_record(int feedbackId)
{
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    int record_num = 0;
    while (read(fd, &feedback, sizeof(Feedback)) == sizeof(Feedback))
    {
        if (feedback.feedbackId == feedbackId)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record_num;
        }
        record_num++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

// Finds a user record by phone number
// Returns 0 if found, -1 if not found
int find_user_by_phone(const char *phone)
{
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    set_file_lock(fd, F_RDLCK); 
    User user;
    while (read(fd, &user, sizeof(User)) == sizeof(User))
    {
        if (my_strcmp(user.phone, phone) == 0)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return 0; 
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

// Finds a user record by email
// Returns 0 if found, -1 if not found
int find_user_by_email(const char *email)
{
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    set_file_lock(fd, F_RDLCK); 
    User user;
    while (read(fd, &user, sizeof(User)) == sizeof(User))
    {
        if (my_strcmp(user.email, email) == 0)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return 0;
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

// --- Data Reading Functions ---

// Helper function to read a specific record
int read_record(void *record_buffer, int record_num, size_t record_size, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    // Lock the specific record for reading
    if (set_record_lock(fd, record_num, record_size, F_RDLCK) == -1)
    {
        close(fd);
        return -1;
    }

    lseek(fd, record_num * record_size, SEEK_SET);

    ssize_t bytes_read = read(fd, record_buffer, record_size);

    set_record_lock(fd, record_num, record_size, F_UNLCK);
    close(fd);

    return (bytes_read == (ssize_t)record_size) ? 0 : -1;
}

User getUser(int userId)
{
    User user;
    user.userId = -1;
    int record_num = find_user_record(userId);
    if (record_num != -1)
    {
        read_record(&user, record_num, sizeof(User), USER_FILE);
    }
    return user;
}

Account getAccount(int accountId)
{
    Account account;
    account.accountId = -1;
    int record_num = find_account_record_by_id(accountId);
    if (record_num != -1)
    {
        read_record(&account, record_num, sizeof(Account), ACCOUNT_FILE);
    }
    return account;
}

Account getAccountByNum(char *accNum)
{
    Account account;
    account.accountId = -1;
    int record_num = find_account_record_by_number(accNum);
    if (record_num != -1)
    {
        read_record(&account, record_num, sizeof(Account), ACCOUNT_FILE);
    }
    return account;
}

Loan getLoan(int loanId)
{
    Loan loan;
    loan.loanId = -1;
    int record_num = find_loan_record(loanId);
    if (record_num != -1)
    {
        read_record(&loan, record_num, sizeof(Loan), LOAN_FILE);
    }
    return loan;
}

Feedback getFeedback(int feedbackId)
{
    Feedback feedback;
    feedback.feedbackId = -1;
    int record_num = find_feedback_record(feedbackId);
    if (record_num != -1)
    {
        read_record(&feedback, record_num, sizeof(Feedback), FEEDBACK_FILE);
    }
    return feedback;
}

// Fills accountList with accounts owned by ownerUserId, returns count
int getAccountsByOwnerId(int ownerUserId, Account *accountList, int maxAccounts)
{
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1)
    {
        return 0;
    }

    set_file_lock(fd, F_RDLCK);
    Account account;
    int count = 0;
    while (read(fd, &account, sizeof(Account)) == sizeof(Account))
    {
        if (account.ownerUserId == ownerUserId && account.isActive)
        {
            if (count < maxAccounts)
            {
                accountList[count] = account;
                count++;
            }
            else
            {
                break;
            }
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return count;
}

// Data Writing/Updating Functions

// Helper function to append a record
int append_record(void *new_record, size_t record_size, const char *filename)
{
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("open for append");
        return -1;
    }

    set_file_lock(fd, F_WRLCK); // Lock whole file for appending
    ssize_t bytes_written = write(fd, new_record, record_size);

    if (bytes_written > 0)
    {
        fsync(fd); // Force write to disk
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);

    return (bytes_written == (ssize_t)record_size) ? 0 : -1; 
}

// Helper function to update a specific record
int update_record(void *record_buffer, int record_num, size_t record_size, const char *filename)
{
    int fd = open(filename, O_WRONLY);
    if (fd == -1)
    {
        perror("open for update");
        return -1;
    }

    // Lock the specific record for writing
    if (set_record_lock(fd, record_num, record_size, F_WRLCK) == -1)
    {
        close(fd);
        return -1;
    }

    lseek(fd, record_num * record_size, SEEK_SET);
    ssize_t bytes_written = write(fd, record_buffer, record_size);

    if (bytes_written > 0)
    {
        fsync(fd); // Force write to disk
    }

    set_record_lock(fd, record_num, record_size, F_UNLCK);
    close(fd);

    return (bytes_written == (ssize_t)record_size) ? 0 : -1;
}

int addUser(User newUser)
{
    newUser.userId = get_next_user_id(); // Assign the next available ID
    return append_record(&newUser, sizeof(User), USER_FILE);
}

int addAccount(Account newAccount)
{
    newAccount.accountId = get_next_account_id();
    return append_record(&newAccount, sizeof(Account), ACCOUNT_FILE);
}

int addLoan(Loan newLoan)
{
    newLoan.loanId = get_next_loan_id();
    return append_record(&newLoan, sizeof(Loan), LOAN_FILE);
}

int addFeedback(Feedback newFeedback)
{
    newFeedback.feedbackId = get_next_feedback_id();
    return append_record(&newFeedback, sizeof(Feedback), FEEDBACK_FILE);
}

int addTransaction(Transaction newTransaction)
{
    newTransaction.transactionId = get_next_transaction_id(); 
    newTransaction.timestamp = time(NULL);
    return append_record(&newTransaction, sizeof(Transaction), TRANSACTION_FILE);
}

int updateUser(User userToUpdate)
{
    int record_num = find_user_record(userToUpdate.userId);
    if (record_num == -1)
    {
        return -1;
    }
    return update_record(&userToUpdate, record_num, sizeof(User), USER_FILE);
}

int updateAccount(Account accountToUpdate)
{
    int record_num = find_account_record_by_id(accountToUpdate.accountId);
    if (record_num == -1)
    {
        return -1;
    }
    return update_record(&accountToUpdate, record_num, sizeof(Account), ACCOUNT_FILE);
}

int updateLoan(Loan loanToUpdate)
{
    int record_num = find_loan_record(loanToUpdate.loanId);
    if (record_num == -1)
    {
        return -1;
    }
    return update_record(&loanToUpdate, record_num, sizeof(Loan), LOAN_FILE);
}

int updateFeedback(Feedback feedbackToUpdate)
{
    int record_num = find_feedback_record(feedbackToUpdate.feedbackId);
    if (record_num == -1)
    {
        return -1;
    }
    return update_record(&feedbackToUpdate, record_num, sizeof(Feedback), FEEDBACK_FILE);
}

void generate_new_account_number(char *new_acc_num)
{

    const char *prefix = "SB"; 
    int start_num = 10001;

    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1)
    {
        sprintf(new_acc_num, "%s%d", prefix, start_num);
        return;
    }

    set_file_lock(fd, F_RDLCK);
    int next_num = start_num;

    if (lseek(fd, -sizeof(Account), SEEK_END) != -1)
    {
        Account last_account;
        if (read(fd, &last_account, sizeof(Account)) == sizeof(Account))
        {
            if (strncmp(last_account.accountNumber, prefix, strlen(prefix)) == 0)
            {
                int last_num = atoi(last_account.accountNumber + strlen(prefix));
                next_num = last_num + 1;
            }
            else
            {
                next_num = start_num;
            }
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    sprintf(new_acc_num, "%s%d", prefix, next_num);
}

// Journaling Functions

// Appends a single journal entry
void journal_log_entry(JournalEntry entry)
{
    int fd = open(JOURNAL_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("FATAL: Could not open journal file");
        return; // In a real system, we might halt the server
    }

    // We don't need to lock the journal, as it's append-only
    // and each write is small enough to be "atomic" by the OS.
    ssize_t bytes_written = write(fd, &entry, sizeof(JournalEntry));

    if (bytes_written > 0)
    {
        fsync(fd);
    }
    else
    {
        perror("FATAL: Could not write to journal file");
    }
    close(fd);
}

// Clears the journal file after successful recovery or commit
void journal_log_clear()
{
    // Open with O_TRUNC to wipe the file
    int fd = open(JOURNAL_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd != -1)
    {
        fsync(fd); // Ensure the truncation is written
        close(fd);
    }
}