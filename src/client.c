// src/client.c
#include "common.h"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        write_string(STDOUT_FILENO, "\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        write_string(STDOUT_FILENO, "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        write_string(STDOUT_FILENO, "\nConnection Failed \n");
        return -1;
    }

    write_string(STDOUT_FILENO, "Connected to bank server.\n");

    int read_size;
    int is_logged_in = 0;

    // Main communication loop
    while ((read_size = read(sock, buffer, MAX_BUFFER - 1)) > 0) {
        buffer[read_size] = '\0';
        write_string(STDOUT_FILENO, buffer); // Print server message

        // --- Handle Login Success ---
        if (!is_logged_in && strstr(buffer, "Login Successful!")) {
            is_logged_in = 1;
        }
        
        // --- Handle Logout ---
        if (strstr(buffer, "Logging out. Goodbye!")) {
            break; // Server is closing connection
        }
        
        // --- Handle Login Failure ---
        if (strstr(buffer, "Invalid User ID") || strstr(buffer, "deactivated")) {
            break; // Server is closing connection
        }

        // --- Check for a prompt ---
        // If the server's message ends with ": ", it's a prompt for input
        if (read_size > 2 && buffer[read_size - 2] == ':' && buffer[read_size - 1] == ' ') {
            // Read from user's keyboard (stdin)
            int user_read_size = read(STDIN_FILENO, buffer, MAX_BUFFER - 1);
            if (user_read_size <= 0) {
                break; // User pressed Ctrl+D
            }
            buffer[user_read_size] = '\0';
            write(sock, buffer, user_read_size); // Send to server
        }
    }

    write_string(STDOUT_FILENO, "\nDisconnected from server.\n");
    close(sock);
    return 0;
}