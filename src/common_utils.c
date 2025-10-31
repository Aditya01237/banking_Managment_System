// src/common_utils.c
#include "common.h" // Includes system headers

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
void read_client_input(int client_socket, char* buffer, int size) {
    int read_size = read(client_socket, buffer, size - 1);
    if (read_size > 0) {
        buffer[read_size] = '\0';
        // Remove trailing newline if present
        if (buffer[read_size - 1] == '\n') {
            buffer[read_size - 1] = '\0';
        }
    } else if (read_size == 0) {
        // Handle client disconnect gracefully (e.g., Ctrl+D)
        buffer[0] = '\0'; 
    } else {
        // Handle read error
        perror("read from client");
        buffer[0] = '\0';
    }
}