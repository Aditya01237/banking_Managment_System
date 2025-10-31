// include/admin.h
#ifndef ADMIN_H
#define ADMIN_H

#include "common.h"

// --- Menu ---
void admin_menu(int client_socket, User user);

// --- Action Handlers ---
// handle_add_user is shared (defined in employee.h)
// handle_modify_user_details is shared (defined in employee.h)
// handle_set_account_status is shared (defined in manager.h)
// handle_view_my_details is shared (defined in customer.h)
// handle_change_password is shared (defined in customer.h)

#endif // ADMIN_H