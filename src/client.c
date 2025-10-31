#include "common.h"

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        write_string(STDOUT_FILENO, "\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        write_string(STDOUT_FILENO, "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        write_string(STDOUT_FILENO, "\nConnection Failed \n");
        return -1;
    }

    write_string(STDOUT_FILENO, "Connected to bank server.\n");

    int read_size;
    while (1)
    {
        read_size = read(sock, buffer, MAX_BUFFER - 1);
        if (read_size <= 0)
        {
            break;
        }
        buffer[read_size] = '\0';
        write_string(STDOUT_FILENO, buffer);

        if (strstr(buffer, "Enter") != NULL)
        {
            read_size = read(STDIN_FILENO, buffer, MAX_BUFFER - 1);
            if (read_size <= 0)
            {
                break;
            }
            buffer[read_size] = '\0';
            write(sock, buffer, read_size); 
        }

        if (strstr(buffer, "Successful!") || strstr(buffer, "Invalid") || strstr(buffer, "deactivated"))
        {
            break;
        }
    }

    write_string(STDOUT_FILENO, "Disconnected from server.\n");
    close(sock);
    return 0;
}