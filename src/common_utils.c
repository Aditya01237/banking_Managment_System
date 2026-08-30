#include "common.h"
#include <ctype.h>
#include <sodium.h>

void write_string(int fd, const char *str)
{
    size_t len = strlen(str);
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, str + written, len - written);
        if (n > 0)
        {
            written += (size_t)n;
        }
        else if (n < 0 && errno == EINTR)
        {
            continue;
        }
        else
        {
            break;
        }
    }
}

int my_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int read_client_input(int client_socket, char *buffer, int size)
{
    memset(buffer, 0, (size_t)size);
    int total_read = 0;
    while (total_read < size - 1)
    {
        char ch;
        ssize_t n = read(client_socket, &ch, 1);
        if (n == 1)
        {
            if (ch == '\n')
                break;
            if (ch != '\r')
                buffer[total_read++] = ch;
        }
        else if (n == 0)
        {
            buffer[0] = '\0';
            return -1;
        }
        else if (errno != EINTR)
        {
            perror("read from client");
            buffer[0] = '\0';
            return -1;
        }
    }
    buffer[total_read] = '\0';
    return 0;
}

int is_valid_number(const char *str)
{
    Money ignored;
    return parse_money(str, &ignored);
}

int is_valid_email(const char *str)
{
    if (strlen(str) == 0)
        return 0;
    return strstr(str, "@") != NULL && strstr(str, ".") != NULL;
}

int is_valid_phone(const char *str)
{
    if (strlen(str) != 10)
        return 0;
    for (int i = 0; i < 10; i++)
    {
        if (!isdigit((unsigned char)str[i]))
            return 0;
    }
    return 1;
}

int parse_money(const char *str, Money *out)
{
    if (str == NULL || out == NULL || *str == '\0')
        return 0;

    Money whole = 0;
    int fraction = 0;
    int fraction_digits = 0;
    int seen_dot = 0;

    for (const char *p = str; *p; ++p)
    {
        if (*p == '.')
        {
            if (seen_dot)
                return 0;
            seen_dot = 1;
            continue;
        }
        if (!isdigit((unsigned char)*p))
            return 0;

        int digit = *p - '0';
        if (!seen_dot)
        {
            if (whole > (INT64_MAX - digit) / 10)
                return 0;
            whole = whole * 10 + digit;
        }
        else
        {
            if (fraction_digits >= 2)
                return 0;
            fraction = fraction * 10 + digit;
            fraction_digits++;
        }
    }

    if (fraction_digits == 1)
        fraction *= 10;

    if (whole > (INT64_MAX - fraction) / 100)
        return 0;

    *out = whole * 100 + fraction;
    return 1;
}

void format_money(Money amount, char *buffer, size_t size)
{
    if (buffer == NULL || size == 0)
        return;

    int negative = amount < 0;
    uint64_t abs_value = negative ? (uint64_t)(-(amount + 1)) + 1 : (uint64_t)amount;
    uint64_t rupees = abs_value / 100;
    uint64_t paise = abs_value % 100;
    snprintf(buffer, size, "%s₹%llu.%02llu",
             negative ? "-" : "",
             (unsigned long long)rupees,
             (unsigned long long)paise);
}

int hash_password(const char *plain_password, char out_hash[PASSWORD_HASH_MAX])
{
    if (plain_password == NULL || out_hash == NULL || *plain_password == '\0')
        return -1;

    if (sodium_init() < 0)
        return -1;

    if (crypto_pwhash_str(out_hash,
                          plain_password,
                          strlen(plain_password),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        return -1;
    }
    return 0;
}

int verify_password(const char *stored_hash, const char *plain_password)
{
    if (stored_hash == NULL || plain_password == NULL)
        return 0;
    if (sodium_init() < 0)
        return 0;
    return crypto_pwhash_str_verify(stored_hash, plain_password, strlen(plain_password)) == 0;
}
