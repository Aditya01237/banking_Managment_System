// src/common_utils.c
#include "common.h"
#include <ctype.h> // For isdigit()
#include <errno.h> // For errno

// --- Generic Utility Functions ---

void write_string(int fd, const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    write(fd, str, len);
}

int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Reads a line from the client, ensures null termination, removes trailing newline
// Reads a line, returns 0 on success, -1 on disconnect/error
int read_client_input(int client_socket, char* buffer, int size) {
    memset(buffer, 0, size);
    int total_read = 0;
    int read_size = 0;
    char temp_char;

    while (total_read < size - 1) {
        read_size = read(client_socket, &temp_char, 1);

        if (read_size == 1) { // Read one char
            if (temp_char == '\n') {
                break; // End of line
            }
            if (temp_char != '\r') {
                 buffer[total_read] = temp_char;
                 total_read++;
            }
        } else if (read_size == 0) { // Client disconnected
            buffer[0] = '\0';
            return -1; // <-- SIGNAL DISCONNECT
        } else { // Read error
             if (errno == EINTR) continue; // Interrupted, try again
             perror("read from client");
             buffer[0] = '\0';
             return -1; // <-- SIGNAL ERROR
        }
    }
    buffer[total_read] = '\0';
    return 0; // Success
}

// --- NEW VALIDATION FUNCTIONS ---

// Checks if a string is a valid positive number (for amount)
int is_valid_number(const char* str) {
    if (str[0] == '\0') return 0; // Empty
    int decimal_point_found = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '.') {
            if (decimal_point_found) return 0; // Second decimal point
            decimal_point_found = 1;
        } else if (!isdigit(str[i])) {
            return 0; // Not a digit
        }
    }
    return 1;
}

// Simple email check (contains '@' and '.')
int is_valid_email(const char* str) {
    if (strlen(str) == 0) return 0;
    return (strstr(str, "@") != NULL && strstr(str, ".") != NULL);
}

// Simple 10-digit phone check
int is_valid_phone(const char* str) {
    if (strlen(str) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}