#include "customer.h"
#include "data_access.h"
#include <signal.h>

static int prompt_money(int client_socket, const char *prompt, Money *amount)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, prompt);
    if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1)
        return -1;
    if (strcmp(buffer, "0") == 0)
        return 0;
    if (!parse_money(buffer, amount) || *amount <= 0)
    {
        write_string(client_socket, "Invalid amount. Use a positive value with at most 2 decimal places.\n");
        return 0;
    }
    return 1;
}

static void send_money_line(int fd, const char *prefix, Money amount)
{
    char money[64], line[160];
    format_money(amount, money, sizeof(money));
    snprintf(line, sizeof(line), "%s%s\n", prefix, money);
    write_string(fd, line);
}

void account_selection_menu(int client_socket, User user)
{
    char buffer[MAX_BUFFER];
    Account accounts[10];
    while (1)
    {
        int count = getAccountsByOwnerId(user.userId, accounts, 10);
        if (count == 0)
        {
            write_string(client_socket, "You have no active accounts. Please contact your bank.\n");
            return;
        }

        snprintf(buffer, sizeof(buffer), "\n--- Welcome, %s. Select an Account ---\n", user.firstName);
        write_string(client_socket, buffer);
        for (int i = 0; i < count; ++i)
        {
            char money[64];
            format_money(accounts[i].balance, money, sizeof(money));
            snprintf(buffer, sizeof(buffer), "%d. %s (Balance: %s)\n", i + 1, accounts[i].accountNumber, money);
            write_string(client_socket, buffer);
        }
        snprintf(buffer, sizeof(buffer), "%d. Logout\n", count + 1);
        write_string(client_socket, buffer);
        write_string(client_socket, "Enter your choice: ");
        if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1)
            return;
        int choice = atoi(buffer);
        if (choice > 0 && choice <= count)
            customer_menu(client_socket, user, accounts[choice - 1].accountId);
        else if (choice == count + 1)
            return;
        else
            write_string(client_socket, "Invalid choice.\n");
    }
}

void customer_menu(int client_socket, User user, int accountId)
{
    char buffer[MAX_BUFFER];
    while (1)
    {
        Account account = getAccount(accountId);
        if (account.accountId < 0)
            return;
        snprintf(buffer, sizeof(buffer), "\n--- Customer Menu (Account: %s) ---\n", account.accountNumber);
        write_string(client_socket, buffer);
        write_string(client_socket,
                     "1. View Balance\n2. Deposit Money\n3. Withdraw Money\n4. Transfer Funds\n"
                     "5. View Transaction History\n6. Apply for Loan\n7. View Loan Status\n"
                     "8. View My Personal Details\n9. Add Feedback\n10. View Feedback Status\n"
                     "11. Change Password\n12. Switch Account / Logout\nEnter your choice: ");
        if (read_client_input(client_socket, buffer, MAX_BUFFER) == -1)
            return;
        switch (atoi(buffer))
        {
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
        case 12: return;
        default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

void handle_view_balance(int client_socket, int accountId)
{
    Account account = getAccount(accountId);
    if (account.accountId < 0)
    {
        write_string(client_socket, "Error retrieving account.\n");
        return;
    }
    send_money_line(client_socket, "Balance: ", account.balance);
}

static void mutate_single_account(int client_socket, int accountId, Money amount, int is_deposit)
{
    int record = find_account_record_by_id(accountId);
    if (record < 0)
    {
        write_string(client_socket, "Account not found.\n");
        return;
    }

    lock_account_ids(accountId, accountId);
    int fd = open(ACCOUNT_FILE, O_RDWR);
    Account account;
    int locked = 0;
    if (fd >= 0 && read_account_locked_fd(fd, record, &account) == 0)
        locked = 1;

    if (!locked)
    {
        if (fd >= 0) close(fd);
        unlock_account_ids(accountId, accountId);
        write_string(client_socket, "Unable to lock account.\n");
        return;
    }

    if (!account.isActive || (!is_deposit && amount > account.balance))
    {
        set_record_lock(fd, record, sizeof(Account), F_UNLCK);
        close(fd);
        unlock_account_ids(accountId, accountId);
        write_string(client_socket, account.isActive ? "Insufficient funds.\n" : "Account is inactive.\n");
        return;
    }

    account.balance += is_deposit ? amount : -amount;
    int rc = write_account_locked_fd(fd, record, &account);
    set_record_lock(fd, record, sizeof(Account), F_UNLCK);
    close(fd);
    unlock_account_ids(accountId, accountId);

    if (rc != 0)
    {
        write_string(client_socket, "Account update failed.\n");
        return;
    }

    Transaction txn = {0};
    txn.accountId = account.accountId;
    txn.userId = account.ownerUserId;
    txn.type = is_deposit ? DEPOSIT : WITHDRAWAL;
    txn.amount = amount;
    txn.newBalance = account.balance;
    strcpy(txn.otherPartyAccountNumber, "---");
    if (addTransaction(txn) != 0)
        write_string(client_socket, "Warning: balance updated but transaction audit append failed.\n");
    send_money_line(client_socket, is_deposit ? "Deposit successful. New balance: " : "Withdrawal successful. New balance: ", account.balance);
}

void handle_deposit(int client_socket, int accountId)
{
    Money amount;
    if (prompt_money(client_socket, "Enter amount to deposit (or 0 to cancel): ", &amount) == 1)
        mutate_single_account(client_socket, accountId, amount, 1);
}

void handle_withdraw(int client_socket, int accountId)
{
    Money amount;
    if (prompt_money(client_socket, "Enter amount to withdraw (or 0 to cancel): ", &amount) == 1)
        mutate_single_account(client_socket, accountId, amount, 0);
}

void handle_transfer_funds(int client_socket, int senderAccountId)
{
    char receiver_number[20], buffer[MAX_BUFFER];
    Money amount;
    write_string(client_socket, "Enter receiver account number (or 0 to cancel): ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1 || strcmp(buffer, "0") == 0)
        return;
    if (strlen(buffer) == 0 || strlen(buffer) >= sizeof(receiver_number))
    {
        write_string(client_socket, "Invalid account number.\n");
        return;
    }
    strcpy(receiver_number, buffer);
    if (prompt_money(client_socket, "Enter transfer amount (or 0 to cancel): ", &amount) != 1)
        return;

    Account receiver_snapshot = getAccountByNum(receiver_number);
    if (receiver_snapshot.accountId < 0 || receiver_snapshot.accountId == senderAccountId)
    {
        write_string(client_socket, "Invalid receiver account.\n");
        return;
    }

    int receiverAccountId = receiver_snapshot.accountId;
    int sender_record = find_account_record_by_id(senderAccountId);
    int receiver_record = find_account_record_by_id(receiverAccountId);
    if (sender_record < 0 || receiver_record < 0)
    {
        write_string(client_socket, "Account lookup failed.\n");
        return;
    }

    lock_account_ids(senderAccountId, receiverAccountId);
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
    {
        unlock_account_ids(senderAccountId, receiverAccountId);
        write_string(client_socket, "Unable to open account store.\n");
        return;
    }

    int first_id = senderAccountId < receiverAccountId ? senderAccountId : receiverAccountId;
    int first_record = first_id == senderAccountId ? sender_record : receiver_record;
    int second_record = first_id == senderAccountId ? receiver_record : sender_record;
    Account first, second;

    if (read_account_locked_fd(fd, first_record, &first) != 0 ||
        read_account_locked_fd(fd, second_record, &second) != 0)
    {
        set_record_lock(fd, first_record, sizeof(Account), F_UNLCK);
        set_record_lock(fd, second_record, sizeof(Account), F_UNLCK);
        close(fd);
        unlock_account_ids(senderAccountId, receiverAccountId);
        write_string(client_socket, "Unable to acquire transaction locks.\n");
        return;
    }

    Account *sender = first.accountId == senderAccountId ? &first : &second;
    Account *receiver = first.accountId == receiverAccountId ? &first : &second;
    if (!sender->isActive || !receiver->isActive || sender->balance < amount)
    {
        set_record_lock(fd, second_record, sizeof(Account), F_UNLCK);
        set_record_lock(fd, first_record, sizeof(Account), F_UNLCK);
        close(fd);
        unlock_account_ids(senderAccountId, receiverAccountId);
        write_string(client_socket, sender->balance < amount ? "Insufficient funds.\n" : "One of the accounts is inactive.\n");
        return;
    }

    uint64_t txid = next_wal_transaction_id();
    JournalEntry sender_undo = {txid, TXN_UNDO, sender->accountId, sender->balance};
    JournalEntry receiver_undo = {txid, TXN_UNDO, receiver->accountId, receiver->balance};
    if (journal_log_entry(sender_undo) != 0 || journal_log_entry(receiver_undo) != 0)
    {
        set_record_lock(fd, second_record, sizeof(Account), F_UNLCK);
        set_record_lock(fd, first_record, sizeof(Account), F_UNLCK);
        close(fd);
        unlock_account_ids(senderAccountId, receiverAccountId);
        write_string(client_socket, "Transaction journal unavailable; transfer aborted safely.\n");
        return;
    }

    sender->balance -= amount;
    receiver->balance += amount;
    int sender_write = write_account_locked_fd(fd, sender_record, sender);

    if (getenv("BANK_CRASH_AFTER_DEBIT") != NULL)
        kill(getpid(), SIGKILL);

    int receiver_write = sender_write == 0 ? write_account_locked_fd(fd, receiver_record, receiver) : -1;
    if (sender_write == 0 && receiver_write == 0)
    {
        JournalEntry commit = {txid, TXN_COMMIT, 0, 0};
        if (journal_log_entry(commit) != 0)
            write_string(client_socket, "Transfer data written; commit journal append failed. Recovery will conservatively roll it back.\n");
        else
        {
            Transaction out = {0}, in = {0};
            out.accountId = sender->accountId;
            out.userId = sender->ownerUserId;
            out.type = TRANSFER_OUT;
            out.amount = amount;
            out.newBalance = sender->balance;
            strcpy(out.otherPartyAccountNumber, receiver->accountNumber);
            in.accountId = receiver->accountId;
            in.userId = receiver->ownerUserId;
            in.type = TRANSFER_IN;
            in.amount = amount;
            in.newBalance = receiver->balance;
            strcpy(in.otherPartyAccountNumber, sender->accountNumber);
            addTransaction(out);
            addTransaction(in);
            write_string(client_socket, "Transfer successful.\n");
        }
    }
    else
    {
        write_string(client_socket, "Transfer interrupted. Restart recovery will restore the pre-transaction balances.\n");
    }

    set_record_lock(fd, second_record, sizeof(Account), F_UNLCK);
    set_record_lock(fd, first_record, sizeof(Account), F_UNLCK);
    close(fd);
    unlock_account_ids(senderAccountId, receiverAccountId);
}

void handle_view_transaction_history(int client_socket, int accountId)
{
    int fd = open(TRANSACTION_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No transactions found.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Transaction txn;
    char line[512], money[64], balance[64], timebuf[32];
    int found = 0;
    write_string(client_socket, "\n--- Transaction History ---\n");
    while (read(fd, &txn, sizeof(txn)) == (ssize_t)sizeof(txn))
    {
        if (txn.accountId != accountId)
            continue;
        found = 1;
        struct tm tmv;
        localtime_r(&txn.timestamp, &tmv);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tmv);
        format_money(txn.amount, money, sizeof(money));
        format_money(txn.newBalance, balance, sizeof(balance));
        const char *type = txn.type == DEPOSIT ? "DEPOSIT" : txn.type == WITHDRAWAL ? "WITHDRAW" : txn.type == TRANSFER_OUT ? "TRANSFER_OUT" : "TRANSFER_IN";
        snprintf(line, sizeof(line), "#%d | %s | %s | %s | balance %s | other %s\n",
                 txn.transactionId, timebuf, type, money, balance, txn.otherPartyAccountNumber);
        write_string(client_socket, line);
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
        write_string(client_socket, "No transactions found for this account.\n");
}

void handle_apply_loan(int client_socket, int userId)
{
    Money amount;
    char account_number[20];
    if (prompt_money(client_socket, "Enter loan amount (or 0 to cancel): ", &amount) != 1)
        return;
    write_string(client_socket, "Enter your account number for loan credit: ");
    if (read_client_input(client_socket, account_number, sizeof(account_number)) == -1)
        return;
    Account account = getAccountByNum(account_number);
    if (account.accountId < 0 || account.ownerUserId != userId)
    {
        write_string(client_socket, "Account not found or does not belong to you.\n");
        return;
    }
    Loan loan = {0};
    loan.userId = userId;
    loan.accountIdToDeposit = account.accountId;
    loan.amount = amount;
    loan.status = PENDING;
    if (addLoan(loan) == 0)
        write_string(client_socket, "Loan application submitted.\n");
    else
        write_string(client_socket, "Could not save loan application.\n");
}

void handle_view_loan_status(int client_socket, int userId)
{
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No loan applications found.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    int found = 0;
    char line[256], money[64];
    while (read(fd, &loan, sizeof(loan)) == (ssize_t)sizeof(loan))
    {
        if (loan.userId != userId)
            continue;
        found = 1;
        format_money(loan.amount, money, sizeof(money));
        const char *status = loan.status == PENDING ? "PENDING" : loan.status == PROCESSING ? "PROCESSING" : loan.status == APPROVED ? "APPROVED" : "REJECTED";
        snprintf(line, sizeof(line), "Loan %d | %s | %s\n", loan.loanId, money, status);
        write_string(client_socket, line);
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
        write_string(client_socket, "No loan applications found.\n");
}

void handle_add_feedback(int client_socket, int userId)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Enter feedback (or 0 to cancel): ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1 || strcmp(buffer, "0") == 0)
        return;
    if (*buffer == '\0')
        return;
    Feedback feedback = {0};
    feedback.userId = userId;
    snprintf(feedback.feedbackText, sizeof(feedback.feedbackText), "%s", buffer);
    if (addFeedback(feedback) == 0)
        write_string(client_socket, "Feedback submitted.\n");
}

void handle_view_feedback_status(int client_socket, int userId)
{
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No feedback found.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    int found = 0;
    char line[384];
    while (read(fd, &feedback, sizeof(feedback)) == (ssize_t)sizeof(feedback))
    {
        if (feedback.userId != userId)
            continue;
        found = 1;
        snprintf(line, sizeof(line), "Feedback %d | %s | %.200s\n", feedback.feedbackId,
                 feedback.isReviewed ? "Reviewed" : "Pending", feedback.feedbackText);
        write_string(client_socket, line);
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
        write_string(client_socket, "No feedback found.\n");
}

void handle_view_my_details(int client_socket, User user)
{
    char line[640];
    snprintf(line, sizeof(line), "\nUser ID: %d\nName: %s %s\nPhone: %s\nEmail: %s\nAddress: %s\n",
             user.userId, user.firstName, user.lastName, user.phone, user.email, user.address);
    write_string(client_socket, line);
}

void handle_change_password(int client_socket, int userId)
{
    char password[MAX_BUFFER];
    write_string(client_socket, "Enter new password (or 0 to cancel): ");
    if (read_client_input(client_socket, password, sizeof(password)) == -1 || strcmp(password, "0") == 0)
        return;
    if (strlen(password) < 8)
    {
        write_string(client_socket, "Password must be at least 8 characters.\n");
        return;
    }
    User user = getUser(userId);
    if (user.userId < 0 || hash_password(password, user.password) != 0)
    {
        write_string(client_socket, "Unable to hash password.\n");
        return;
    }
    if (updateUser(user) == 0)
        write_string(client_socket, "Password changed successfully.\n");
    else
        write_string(client_socket, "Password update failed.\n");
}
