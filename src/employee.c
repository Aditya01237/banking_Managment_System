#include "employee.h"
#include "customer.h"
#include "data_access.h"

static pthread_mutex_t create_user_mutex = PTHREAD_MUTEX_INITIALIZER;

void employee_menu(int client_socket, User user)
{
    char buffer[MAX_BUFFER];
    while (1)
    {
        snprintf(buffer, sizeof(buffer), "\n--- Employee Menu (%s %s) ---\n", user.firstName, user.lastName);
        write_string(client_socket, buffer);
        write_string(client_socket,
                     "1. Add Customer\n2. Add Account\n3. Modify Customer\n4. View Customer Transactions\n"
                     "5. View Assigned Loans\n6. Process Loan\n7. View My Details\n8. Change Password\n9. Logout\nChoice: ");
        if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
            return;
        switch (atoi(buffer))
        {
        case 1: handle_add_user(client_socket, CUSTOMER); break;
        case 2: handle_add_new_account(client_socket); break;
        case 3: handle_modify_user_details(client_socket, 0); break;
        case 4: handle_view_customer_transactions(client_socket); break;
        case 5: handle_view_assigned_loans(client_socket, user.userId); break;
        case 6: handle_process_loan(client_socket, user.userId); break;
        case 7: handle_view_my_details(client_socket, user); break;
        case 8: handle_change_password(client_socket, user.userId); break;
        case 9: return;
        default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

static int prompt_field(int fd, const char *prompt, char *dest, size_t dest_size)
{
    char buffer[MAX_BUFFER];
    write_string(fd, prompt);
    if (read_client_input(fd, buffer, sizeof(buffer)) == -1)
        return -1;
    if (*buffer == '\0' || strlen(buffer) >= dest_size)
    {
        write_string(fd, "Invalid or too-long value.\n");
        return 0;
    }
    snprintf(dest, dest_size, "%s", buffer);
    return 1;
}

void handle_add_user(int client_socket, UserRole role_to_add)
{
    User user = {0};
    user.role = role_to_add;
    user.isActive = 1;
    char password[MAX_BUFFER];

    write_string(client_socket, "New password (minimum 8 characters): ");
    if (read_client_input(client_socket, password, sizeof(password)) == -1 || strlen(password) < 8)
    {
        write_string(client_socket, "Invalid password.\n");
        return;
    }
    if (hash_password(password, user.password) != 0)
    {
        write_string(client_socket, "Password hashing failed.\n");
        return;
    }
    if (prompt_field(client_socket, "First name: ", user.firstName, sizeof(user.firstName)) != 1 ||
        prompt_field(client_socket, "Last name: ", user.lastName, sizeof(user.lastName)) != 1 ||
        prompt_field(client_socket, "Phone: ", user.phone, sizeof(user.phone)) != 1 ||
        prompt_field(client_socket, "Email: ", user.email, sizeof(user.email)) != 1 ||
        prompt_field(client_socket, "Address: ", user.address, sizeof(user.address)) != 1)
        return;

    if (!is_valid_phone(user.phone) || !is_valid_email(user.email))
    {
        write_string(client_socket, "Phone/email format invalid.\n");
        return;
    }

    pthread_mutex_lock(&create_user_mutex);
    if (find_user_by_phone(user.phone) == 0 || find_user_by_email(user.email) == 0)
    {
        pthread_mutex_unlock(&create_user_mutex);
        write_string(client_socket, "Phone or email already exists.\n");
        return;
    }

    user.userId = get_next_user_id();
    if (addUser(user) != 0)
    {
        pthread_mutex_unlock(&create_user_mutex);
        write_string(client_socket, "Could not create user.\n");
        return;
    }

    if (role_to_add == CUSTOMER)
    {
        Account account = {0};
        account.accountId = get_next_account_id();
        account.ownerUserId = user.userId;
        account.balance = 0;
        account.isActive = 1;
        generate_new_account_number(account.accountNumber);
        if (addAccount(account) != 0)
            write_string(client_socket, "User created, but initial account creation failed.\n");
        else
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Customer created. User ID %d, account %s.\n", user.userId, account.accountNumber);
            write_string(client_socket, msg);
        }
    }
    else
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "User created with ID %d.\n", user.userId);
        write_string(client_socket, msg);
    }
    pthread_mutex_unlock(&create_user_mutex);
}

void handle_add_new_account(int client_socket)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Customer user ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    int userId = atoi(buffer);
    User user = getUser(userId);
    if (user.userId < 0 || user.role != CUSTOMER)
    {
        write_string(client_socket, "Customer not found.\n");
        return;
    }

    pthread_mutex_lock(&create_user_mutex);
    Account account = {0};
    account.accountId = get_next_account_id();
    account.ownerUserId = userId;
    account.isActive = 1;
    generate_new_account_number(account.accountNumber);
    int rc = addAccount(account);
    pthread_mutex_unlock(&create_user_mutex);

    if (rc == 0)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Created account %s.\n", account.accountNumber);
        write_string(client_socket, msg);
    }
    else
        write_string(client_socket, "Account creation failed.\n");
}

void handle_modify_user_details(int client_socket, int admin_mode)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, "User ID to modify: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    User user = getUser(atoi(buffer));
    if (user.userId < 0 || (!admin_mode && user.role != CUSTOMER))
    {
        write_string(client_socket, "User not found or permission denied.\n");
        return;
    }

    write_string(client_socket, "New password or 'skip': ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    if (strcmp(buffer, "skip") != 0)
    {
        if (strlen(buffer) < 8 || hash_password(buffer, user.password) != 0)
        {
            write_string(client_socket, "Password update rejected.\n");
            return;
        }
    }

#define UPDATE_TEXT_FIELD(prompt, field)                                                     \
    do                                                                                       \
    {                                                                                        \
        write_string(client_socket, prompt " or 'skip': ");                                 \
        if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1) return;          \
        if (strcmp(buffer, "skip") != 0)                                                     \
        {                                                                                    \
            if (strlen(buffer) >= sizeof(user.field)) { write_string(client_socket, "Too long.\n"); return; } \
            snprintf(user.field, sizeof(user.field), "%s", buffer);                         \
        }                                                                                    \
    } while (0)

    UPDATE_TEXT_FIELD("First name", firstName);
    UPDATE_TEXT_FIELD("Last name", lastName);
    UPDATE_TEXT_FIELD("Phone", phone);
    UPDATE_TEXT_FIELD("Email", email);
    UPDATE_TEXT_FIELD("Address", address);
#undef UPDATE_TEXT_FIELD

    if (admin_mode)
    {
        write_string(client_socket, "New role 0=Customer 1=Employee 2=Manager 3=Admin or 'skip': ");
        if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
            return;
        if (strcmp(buffer, "skip") != 0)
        {
            int role = atoi(buffer);
            if (role < CUSTOMER || role > ADMINISTRATOR)
            {
                write_string(client_socket, "Invalid role.\n");
                return;
            }
            user.role = (UserRole)role;
        }
    }

    if (!is_valid_phone(user.phone) || !is_valid_email(user.email))
    {
        write_string(client_socket, "Resulting phone/email is invalid.\n");
        return;
    }
    write_string(client_socket, updateUser(user) == 0 ? "User updated.\n" : "User update failed.\n");
}

void handle_view_customer_transactions(int client_socket)
{
    char accountNumber[20];
    write_string(client_socket, "Customer account number: ");
    if (read_client_input(client_socket, accountNumber, sizeof(accountNumber)) == -1)
        return;
    Account account = getAccountByNum(accountNumber);
    if (account.accountId < 0)
        write_string(client_socket, "Account not found.\n");
    else
        handle_view_transaction_history(client_socket, account.accountId);
}

void handle_view_assigned_loans(int client_socket, int employeeId)
{
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No loans found.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    int found = 0;
    char msg[256], money[64];
    while (read(fd, &loan, sizeof(loan)) == (ssize_t)sizeof(loan))
    {
        if (loan.assignedToEmployeeId != employeeId || (loan.status != PENDING && loan.status != PROCESSING))
            continue;
        found = 1;
        format_money(loan.amount, money, sizeof(money));
        snprintf(msg, sizeof(msg), "Loan %d | user %d | %s | %s\n", loan.loanId, loan.userId, money,
                 loan.status == PENDING ? "PENDING" : "PROCESSING");
        write_string(client_socket, msg);
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
        write_string(client_socket, "No assigned loans.\n");
}

void handle_process_loan(int client_socket, int employeeId)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, "Loan ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    Loan loan = getLoan(atoi(buffer));
    if (loan.loanId < 0 || loan.assignedToEmployeeId != employeeId ||
        (loan.status != PENDING && loan.status != PROCESSING))
    {
        write_string(client_socket, "Loan unavailable or not assigned to you.\n");
        return;
    }

    write_string(client_socket, "1 Approve, 2 Reject: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    int choice = atoi(buffer);
    if (choice == 2)
    {
        loan.status = REJECTED;
        write_string(client_socket, updateLoan(loan) == 0 ? "Loan rejected.\n" : "Loan update failed.\n");
        return;
    }
    if (choice != 1)
    {
        write_string(client_socket, "Invalid action.\n");
        return;
    }

    int accountId = loan.accountIdToDeposit;
    int record = find_account_record_by_id(accountId);
    lock_account_ids(accountId, accountId);
    int fd = open(ACCOUNT_FILE, O_RDWR);
    Account account;
    int locked = fd >= 0 && record >= 0 && read_account_locked_fd(fd, record, &account) == 0;
    if (!locked || !account.isActive)
    {
        if (locked) set_record_lock(fd, record, sizeof(Account), F_UNLCK);
        if (fd >= 0) close(fd);
        unlock_account_ids(accountId, accountId);
        write_string(client_socket, "Loan credit account unavailable.\n");
        return;
    }

    account.balance += loan.amount;
    int rc = write_account_locked_fd(fd, record, &account);
    set_record_lock(fd, record, sizeof(Account), F_UNLCK);
    close(fd);
    unlock_account_ids(accountId, accountId);
    if (rc != 0)
    {
        write_string(client_socket, "Loan credit failed.\n");
        return;
    }

    loan.status = APPROVED;
    if (updateLoan(loan) != 0)
    {
        write_string(client_socket, "Credit succeeded, but loan status update failed.\n");
        return;
    }

    Transaction txn = {0};
    txn.accountId = account.accountId;
    txn.userId = account.ownerUserId;
    txn.type = DEPOSIT;
    txn.amount = loan.amount;
    txn.newBalance = account.balance;
    strcpy(txn.otherPartyAccountNumber, "LOAN_CREDIT");
    addTransaction(txn);
    write_string(client_socket, "Loan approved and credited.\n");
}
