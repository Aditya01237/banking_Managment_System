#include "common.h"

static int write_user(int fd, int id, UserRole role, const char *password,
                      const char *first, const char *last,
                      const char *phone, const char *email, const char *address)
{
    User user = {0};
    user.userId = id;
    user.role = role;
    user.isActive = 1;
    if (hash_password(password, user.password) != 0)
        return -1;
    snprintf(user.firstName, sizeof(user.firstName), "%s", first);
    snprintf(user.lastName, sizeof(user.lastName), "%s", last);
    snprintf(user.phone, sizeof(user.phone), "%s", phone);
    snprintf(user.email, sizeof(user.email), "%s", email);
    snprintf(user.address, sizeof(user.address), "%s", address);
    return write(fd, &user, sizeof(user)) == (ssize_t)sizeof(user) ? 0 : -1;
}

int main(void)
{
    mkdir("data", 0755);
    int user_fd = open(USER_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int account_fd = open(ACCOUNT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (user_fd < 0 || account_fd < 0)
    {
        perror("open seed files");
        return 1;
    }

    const char *empty_files[] = {LOAN_FILE, FEEDBACK_FILE, TRANSACTION_FILE, JOURNAL_FILE};
    for (size_t i = 0; i < sizeof(empty_files) / sizeof(empty_files[0]); ++i)
    {
        int fd = open(empty_files[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
            close(fd);
    }

    if (write_user(user_fd, 1, ADMINISTRATOR, "admin123", "Admin", "User", "9876543210", "admin@bank.com", "1 Bank Road, Bangalore") != 0 ||
        write_user(user_fd, 2, CUSTOMER, "customer123", "Ravi", "Kumar", "8888888888", "ravi@gmail.com", "123 MG Road, Bangalore") != 0 ||
        write_user(user_fd, 3, EMPLOYEE, "employee123", "Priya", "Sharma", "7777777777", "priya@bank.com", "456 Indiranagar, Bangalore") != 0 ||
        write_user(user_fd, 4, MANAGER, "manager123", "Vikram", "Singh", "6666666666", "vikram@bank.com", "789 Koramangala, Bangalore") != 0)
    {
        write_string(STDERR_FILENO, "Failed to create seed users.\n");
        close(user_fd);
        close(account_fd);
        return 1;
    }
    fsync(user_fd);
    close(user_fd);

    Account a1 = {.accountId = 1, .ownerUserId = 2, .balance = 500000, .isActive = 1};
    Account a2 = {.accountId = 2, .ownerUserId = 2, .balance = 2500000, .isActive = 1};
    strcpy(a1.accountNumber, "SB10001");
    strcpy(a2.accountNumber, "SB10002");
    if (write(account_fd, &a1, sizeof(a1)) != (ssize_t)sizeof(a1) ||
        write(account_fd, &a2, sizeof(a2)) != (ssize_t)sizeof(a2))
    {
        write_string(STDERR_FILENO, "Failed to create seed accounts.\n");
        close(account_fd);
        return 1;
    }
    fsync(account_fd);
    close(account_fd);

    write_string(STDOUT_FILENO,
                 "Seed data created.\n"
                 "Admin: 1 / admin123\n"
                 "Customer: 2 / customer123\n"
                 "Employee: 3 / employee123\n"
                 "Manager: 4 / manager123\n");
    return 0;
}
