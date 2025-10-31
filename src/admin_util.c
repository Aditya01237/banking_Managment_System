#include "common.h"

int main()
{
    int fd_user, fd_account;

    fd_user = open(USER_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_user == -1)
    {
        perror("Error opening user file");
        return 1;
    }

    fd_account = open(ACCOUNT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_account == -1)
    {
        perror("Error opening account file");
        return 1;
    }

    open(LOAN_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    open(FEEDBACK_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    open(TRANSACTION_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // --- User 1: Administrator ---  
    User admin;
    admin.userId = 1;
    admin.role = ADMINISTRATOR;
    admin.isActive = 1;
    strcpy(admin.password, "admin123");
    strcpy(admin.firstName, "Admin");
    strcpy(admin.lastName, "User");
    strcpy(admin.phone, "9876543210");
    strcpy(admin.email, "admin@bank.com");
    strcpy(admin.address, "1 Bank Road, Bangalore");
    write(fd_user, &admin, sizeof(User));
    write_string(STDOUT_FILENO, "Admin user created (ID: 1, Pass: admin123)\n");

    // --- User 2: Customer ---
    User customer;
    customer.userId = 2;
    customer.role = CUSTOMER;
    customer.isActive = 1;
    strcpy(customer.password, "cust123");
    strcpy(customer.firstName, "Ravi");
    strcpy(customer.lastName, "Kumar");
    strcpy(customer.phone, "8888888888");
    strcpy(customer.email, "ravi@gmail.com");
    strcpy(customer.address, "123 MG Road, Bangalore");
    write(fd_user, &customer, sizeof(User));
    write_string(STDOUT_FILENO, "Customer user created (ID: 2, Pass: cust123)\n");

    // --- User 3: Employee ---
    User employee;
    employee.userId = 3;
    employee.role = EMPLOYEE;
    employee.isActive = 1;
    strcpy(employee.password, "emp123");
    strcpy(employee.firstName, "Priya");
    strcpy(employee.lastName, "Sharma");
    strcpy(employee.phone, "7777777777");
    strcpy(employee.email, "priya@bank.com");
    strcpy(employee.address, "456 Indiranagar, Bangalore");
    write(fd_user, &employee, sizeof(User));
    write_string(STDOUT_FILENO, "Employee user created (ID: 3, Pass: emp123)\n");

    // --- User 4: Manager ---
    User manager;
    manager.userId = 4;
    manager.role = MANAGER;
    manager.isActive = 1;
    strcpy(manager.password, "man123");
    strcpy(manager.firstName, "Vikram");
    strcpy(manager.lastName, "Singh");
    strcpy(manager.phone, "6666666666");
    strcpy(manager.email, "vikram@bank.com");
    strcpy(manager.address, "789 Koramangala, Bangalore");
    write(fd_user, &manager, sizeof(User));
    write_string(STDOUT_FILENO, "Manager user created (ID: 4, Pass: man123)\n");

    close(fd_user);

    // Account 1 (Savings)
    Account cust_account1;
    cust_account1.accountId = 1;                 
    cust_account1.ownerUserId = 2;                  
    strcpy(cust_account1.accountNumber, "SB10001"); 
    cust_account1.balance = 5000.00;
    cust_account1.isActive = 1;
    write(fd_account, &cust_account1, sizeof(Account));

    // Account 2 (Current)
    Account cust_account2;
    cust_account2.accountId = 2;                   
    cust_account2.ownerUserId = 2;                 
    strcpy(cust_account2.accountNumber, "SB10002"); 
    cust_account2.balance = 25000.00;
    cust_account2.isActive = 1;
    write(fd_account, &cust_account2, sizeof(Account));

    write_string(STDOUT_FILENO, "Customer accounts created (SB10001, SB10002)\n");

    close(fd_account);

    write_string(STDOUT_FILENO, "All data files initialized successfully.\n");

    return 0;
}