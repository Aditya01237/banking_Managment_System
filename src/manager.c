#include "manager.h"
#include "customer.h"
#include "data_access.h"

void manager_menu(int client_socket, User user)
{
    char buffer[MAX_BUFFER];
    while (1)
    {
        snprintf(buffer, sizeof(buffer), "\n--- Manager Menu (%s %s) ---\n", user.firstName, user.lastName);
        write_string(client_socket, buffer);
        write_string(client_socket,
                     "1. Activate/Deactivate Customer & Accounts\n2. Assign Loan\n3. Review Feedback\n"
                     "4. View My Details\n5. Change Password\n6. Logout\nChoice: ");
        if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
            return;
        switch (atoi(buffer))
        {
        case 1: handle_set_account_status(client_socket, 0); break;
        case 2: handle_assign_loan(client_socket); break;
        case 3: handle_review_feedback(client_socket); break;
        case 4: handle_view_my_details(client_socket, user); break;
        case 5: handle_change_password(client_socket, user.userId); break;
        case 6: return;
        default: write_string(client_socket, "Invalid choice.\n");
        }
    }
}

void handle_set_account_status(int client_socket, int admin_mode)
{
    char buffer[MAX_BUFFER];
    write_string(client_socket, "User ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    int userId = atoi(buffer);
    User user = getUser(userId);
    if (user.userId < 0 || (!admin_mode && user.role != CUSTOMER))
    {
        write_string(client_socket, "User not found or permission denied.\n");
        return;
    }

    write_string(client_socket, "Status 1=Active, 0=Inactive: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    int status = atoi(buffer);
    if (status != 0 && status != 1)
    {
        write_string(client_socket, "Invalid status.\n");
        return;
    }

    user.isActive = status;
    if (updateUser(user) != 0)
    {
        write_string(client_socket, "Could not update user.\n");
        return;
    }

    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
    {
        write_string(client_socket, "User updated; account store unavailable.\n");
        return;
    }
    set_file_lock(fd, F_WRLCK);
    Account account;
    int record = 0, updated = 0;
    while (read(fd, &account, sizeof(account)) == (ssize_t)sizeof(account))
    {
        if (account.ownerUserId == userId && account.isActive != status)
        {
            account.isActive = status;
            if (lseek(fd, (off_t)record * sizeof(account), SEEK_SET) >= 0 &&
                write(fd, &account, sizeof(account)) == (ssize_t)sizeof(account))
                updated++;
            lseek(fd, (off_t)(record + 1) * sizeof(account), SEEK_SET);
        }
        record++;
    }
    fsync(fd);
    set_file_lock(fd, F_UNLCK);
    close(fd);

    snprintf(buffer, sizeof(buffer), "User status updated; %d account(s) changed.\n", updated);
    write_string(client_socket, buffer);
}

void handle_assign_loan(int client_socket)
{
    int fd = open(LOAN_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No loans found.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Loan loan;
    char line[256], money[64];
    int found = 0;
    while (read(fd, &loan, sizeof(loan)) == (ssize_t)sizeof(loan))
    {
        if (loan.assignedToEmployeeId == 0 && loan.status == PENDING)
        {
            found = 1;
            format_money(loan.amount, money, sizeof(money));
            snprintf(line, sizeof(line), "Loan %d | customer %d | %s\n", loan.loanId, loan.userId, money);
            write_string(client_socket, line);
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
    {
        write_string(client_socket, "No unassigned loans.\n");
        return;
    }

    char buffer[MAX_BUFFER];
    write_string(client_socket, "Loan ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    Loan selected = getLoan(atoi(buffer));
    if (selected.loanId < 0 || selected.assignedToEmployeeId != 0 || selected.status != PENDING)
    {
        write_string(client_socket, "Loan unavailable.\n");
        return;
    }

    write_string(client_socket, "Employee ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    User employee = getUser(atoi(buffer));
    if (employee.userId < 0 || employee.role != EMPLOYEE || !employee.isActive)
    {
        write_string(client_socket, "Invalid employee.\n");
        return;
    }

    selected.assignedToEmployeeId = employee.userId;
    selected.status = PROCESSING;
    write_string(client_socket, updateLoan(selected) == 0 ? "Loan assigned.\n" : "Loan assignment failed.\n");
}

void handle_review_feedback(int client_socket)
{
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(client_socket, "No feedback.\n");
        return;
    }
    set_file_lock(fd, F_RDLCK);
    Feedback feedback;
    int found = 0;
    char line[384];
    while (read(fd, &feedback, sizeof(feedback)) == (ssize_t)sizeof(feedback))
    {
        if (!feedback.isReviewed)
        {
            found = 1;
            snprintf(line, sizeof(line), "Feedback %d | user %d | %.220s\n", feedback.feedbackId, feedback.userId, feedback.feedbackText);
            write_string(client_socket, line);
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    if (!found)
    {
        write_string(client_socket, "No unreviewed feedback.\n");
        return;
    }

    char buffer[MAX_BUFFER];
    write_string(client_socket, "Feedback ID to mark reviewed: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
        return;
    Feedback selected = getFeedback(atoi(buffer));
    if (selected.feedbackId < 0)
    {
        write_string(client_socket, "Feedback not found.\n");
        return;
    }
    selected.isReviewed = 1;
    write_string(client_socket, updateFeedback(selected) == 0 ? "Feedback reviewed.\n" : "Feedback update failed.\n");
}
