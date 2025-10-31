#include "admin.h"       // Function declarations for admin module
#include "employee.h"    // For shared handle_add_user, handle_modify_user_details
#include "manager.h"     // For shared handle_set_account_status
#include "customer.h"    // For shared handle_view_my_details, handle_change_password
#include "data_access.h" // Needed by shared functions
#include "common.h"      // For structs, enums, read_client_input, write_string
#include <stdio.h>       // For sprintf
#include <stdlib.h>      // For atoi

// --- Admin Menu ---
void admin_menu(int client_socket, User user)
{
    char buffer[MAX_BUFFER];
    while (1)
    {
        sprintf(buffer, "\n--- Admin Menu (User: %s %s) ---\n", user.firstName, user.lastName);
        write_string(client_socket, buffer);
        write_string(client_socket, "1. Add User (Customer/Employee/Manager)\n");
        write_string(client_socket, "2. Modify User Details (Password/Role/KYC)\n");
        write_string(client_socket, "3. Activate/Deactivate Any User & Accounts\n");
        write_string(client_socket, "4. View My Personal Details\n");
        write_string(client_socket, "5. Change My Password\n");
        write_string(client_socket, "6. Logout\n");
        write_string(client_socket, "Enter your choice: ");

        read_client_input(client_socket, buffer, MAX_BUFFER);
        int choice = atoi(buffer);
        switch (choice)
        {
        case 1:
            write_string(client_socket, "Enter role (1=EMP, 2=MAN): ");
            read_client_input(client_socket, buffer, MAX_BUFFER);
            int role = atoi(buffer);
            if (role == 1 || role == 2)
            {
                handle_add_user(client_socket, (UserRole)role);
            }
            else
            {
                write_string(client_socket, "Invalid role.\n");
            }
            break;
        case 2:
            handle_modify_user_details(client_socket, 1); 
            break;
        case 3:
            handle_set_account_status(client_socket, 1);
            break;
        case 4:
            handle_view_my_details(client_socket, user);
            break;
        case 5:
            handle_change_password(client_socket, user.userId);
            break;
        case 6:
            write_string(client_socket, "Logging out. Goodbye!\n");
            return; 
        default:
            write_string(client_socket, "Invalid choice.\n");
        }
    }
}