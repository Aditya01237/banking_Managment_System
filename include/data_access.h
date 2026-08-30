#ifndef DATA_ACCESS_H
#define DATA_ACCESS_H

#include "common.h"

int set_file_lock(int fd, int lock_type);
int set_record_lock(int fd, int record_num, int record_size, int lock_type);

int get_next_user_id(void);
int get_next_account_id(void);
int get_next_loan_id(void);
int get_next_feedback_id(void);
int get_next_transaction_id(void);

int find_user_record(int userId);
int find_account_record_by_id(int accountId);
int find_account_record_by_number(char *acc_num);
int find_loan_record(int loanId);
int find_feedback_record(int feedbackId);
int find_user_by_phone(const char *phone);
int find_user_by_email(const char *email);

User getUser(int userId);
Account getAccount(int accountId);
Account getAccountByNum(char *accNum);
Loan getLoan(int loanId);
Feedback getFeedback(int feedbackId);
int getAccountsByOwnerId(int ownerUserId, Account *accountList, int maxAccounts);

int addUser(User newUser);
int addAccount(Account newAccount);
int addLoan(Loan newLoan);
int addFeedback(Feedback newFeedback);
int addTransaction(Transaction newTransaction);
int updateUser(User userToUpdate);
int updateAccount(Account accountToUpdate);
int updateLoan(Loan loanToUpdate);
int updateFeedback(Feedback feedbackToUpdate);
void generate_new_account_number(char *new_acc_num);

/* Startup indexes reduce hot-path ID/account-number lookups to expected O(1). */
int initialize_indexes(void);
void invalidate_indexes(void);

/* In-process account lock stripes complement process-associated fcntl locks. */
void lock_account_ids(int account_a, int account_b);
void unlock_account_ids(int account_a, int account_b);

/* Lock-aware helpers for multi-record transactions. Caller owns the account mutexes. */
int read_account_locked_fd(int fd, int record_num, Account *out);
int write_account_locked_fd(int fd, int record_num, const Account *account);

uint64_t next_wal_transaction_id(void);
int journal_log_entry(JournalEntry entry);
int journal_log_clear(void);

#endif
