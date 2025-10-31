// include/data_access.h
#ifndef DATA_ACCESS_H
#define DATA_ACCESS_H

#include "common.h" // Needs the struct definitions

// --- Locking Functions ---
int set_file_lock(int fd, int lock_type);
int set_record_lock(int fd, int record_num, int record_size, int lock_type);

// --- ID Generation ---
int get_next_user_id();
int get_next_account_id();
int get_next_loan_id();
int get_next_feedback_id();
int get_next_transaction_id();

// --- Record Finding ---
int find_user_record(int userId);
int find_account_record_by_id(int accountId);
int find_account_record_by_number(char* acc_num);
int find_loan_record(int loanId);
int find_feedback_record(int feedbackId);

// --- Data Reading ---
User getUser(int userId); // Gets a User struct by ID
Account getAccount(int accountId); // Gets an Account struct by ID
Account getAccountByNum(char* accNum); // Gets an Account struct by number
Loan getLoan(int loanId);
Feedback getFeedback(int feedbackId);
int getAccountsByOwnerId(int ownerUserId, Account* accountList, int maxAccounts); // Fills a list of accounts for a user

// --- Data Writing/Updating ---
int addUser(User newUser); // Returns 0 on success, -1 on error
int addAccount(Account newAccount);
int addLoan(Loan newLoan);
int addFeedback(Feedback newFeedback);
int addTransaction(Transaction newTransaction);

int updateUser(User userToUpdate); // Updates the user record with matching userId
int updateAccount(Account accountToUpdate);
int updateLoan(Loan loanToUpdate);
int updateFeedback(Feedback feedbackToUpdate);

// --- Utility ---
void generate_new_account_number(char* new_acc_num);


#endif // DATA_ACCESS_H