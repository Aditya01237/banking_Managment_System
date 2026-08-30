#include "server.h"
#include "admin.h"
#include "customer.h"
#include "data_access.h"
#include "employee.h"
#include "manager.h"

#define MAX_SESSIONS 100
#define WORKER_COUNT 8
#define SOCKET_QUEUE_CAPACITY 128
#define MAX_RECOVERY_ENTRIES 8192

static int activeUserIds[MAX_SESSIONS];
static int activeUserCount = 0;
static pthread_mutex_t sessionMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int sockets[SOCKET_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} SocketQueue;

static SocketQueue socket_queue = {
    .head = 0,
    .tail = 0,
    .count = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
    .not_full = PTHREAD_COND_INITIALIZER};

static void queue_push(int client_socket)
{
    pthread_mutex_lock(&socket_queue.mutex);
    while (socket_queue.count == SOCKET_QUEUE_CAPACITY)
        pthread_cond_wait(&socket_queue.not_full, &socket_queue.mutex);
    socket_queue.sockets[socket_queue.tail] = client_socket;
    socket_queue.tail = (socket_queue.tail + 1) % SOCKET_QUEUE_CAPACITY;
    socket_queue.count++;
    pthread_cond_signal(&socket_queue.not_empty);
    pthread_mutex_unlock(&socket_queue.mutex);
}

static int queue_pop(void)
{
    pthread_mutex_lock(&socket_queue.mutex);
    while (socket_queue.count == 0)
        pthread_cond_wait(&socket_queue.not_empty, &socket_queue.mutex);
    int client_socket = socket_queue.sockets[socket_queue.head];
    socket_queue.head = (socket_queue.head + 1) % SOCKET_QUEUE_CAPACITY;
    socket_queue.count--;
    pthread_cond_signal(&socket_queue.not_full);
    pthread_mutex_unlock(&socket_queue.mutex);
    return client_socket;
}

static void *worker_main(void *unused)
{
    (void)unused;
    for (;;)
    {
        int client_socket = queue_pop();
        int *arg = malloc(sizeof(*arg));
        if (arg == NULL)
        {
            close(client_socket);
            continue;
        }
        *arg = client_socket;
        handle_client(arg);
    }
    return NULL;
}

User check_login(int userId, const char *password)
{
    User result = {.userId = 0};
    int record = find_user_record(userId);
    if (record < 0)
        return result;

    int fd = open(USER_FILE, O_RDONLY);
    if (fd < 0)
    {
        result.userId = -1;
        return result;
    }
    if (set_record_lock(fd, record, sizeof(User), F_RDLCK) != 0)
    {
        close(fd);
        result.userId = -1;
        return result;
    }

    User stored;
    int ok = lseek(fd, (off_t)record * sizeof(User), SEEK_SET) >= 0 &&
             read(fd, &stored, sizeof(stored)) == (ssize_t)sizeof(stored);
    set_record_lock(fd, record, sizeof(User), F_UNLCK);
    close(fd);

    if (!ok)
    {
        result.userId = -1;
        return result;
    }
    if (!stored.isActive)
    {
        result.userId = -2;
        return result;
    }
    if (stored.userId == userId && verify_password(stored.password, password))
        return stored;
    return result;
}

static int tx_is_committed(uint64_t txid, const JournalEntry *entries, int count)
{
    for (int i = 0; i < count; ++i)
        if (entries[i].transactionId == txid && entries[i].type == TXN_COMMIT)
            return 1;
    return 0;
}

void run_server_recovery(void)
{
    int fd = open(JOURNAL_FILE, O_RDONLY);
    if (fd < 0)
    {
        write_string(STDOUT_FILENO, "Recovery: no journal found.\n");
        return;
    }

    JournalEntry entries[MAX_RECOVERY_ENTRIES];
    int count = 0;
    while (count < MAX_RECOVERY_ENTRIES &&
           read(fd, &entries[count], sizeof(JournalEntry)) == (ssize_t)sizeof(JournalEntry))
        count++;
    close(fd);

    if (count == 0)
        return;

    int restored = 0;
    for (int i = count - 1; i >= 0; --i)
    {
        JournalEntry *entry = &entries[i];
        if (entry->type != TXN_UNDO || tx_is_committed(entry->transactionId, entries, count))
            continue;

        Account account = getAccount(entry->accountId);
        if (account.accountId >= 0 && account.balance != entry->oldBalance)
        {
            account.balance = entry->oldBalance;
            if (updateAccount(account) == 0)
                restored++;
        }
    }

    journal_log_clear();
    char msg[128];
    snprintf(msg, sizeof(msg), "Recovery complete: restored %d account record(s).\n", restored);
    write_string(STDOUT_FILENO, msg);
}

static int try_add_session(int user_id)
{
    pthread_mutex_lock(&sessionMutex);
    for (int i = 0; i < activeUserCount; ++i)
    {
        if (activeUserIds[i] == user_id)
        {
            pthread_mutex_unlock(&sessionMutex);
            return 0;
        }
    }
    if (activeUserCount >= MAX_SESSIONS)
    {
        pthread_mutex_unlock(&sessionMutex);
        return -1;
    }
    activeUserIds[activeUserCount++] = user_id;
    pthread_mutex_unlock(&sessionMutex);
    return 1;
}

static void remove_session(int user_id)
{
    pthread_mutex_lock(&sessionMutex);
    for (int i = 0; i < activeUserCount; ++i)
    {
        if (activeUserIds[i] == user_id)
        {
            activeUserIds[i] = activeUserIds[activeUserCount - 1];
            activeUserCount--;
            break;
        }
    }
    pthread_mutex_unlock(&sessionMutex);
}

void *handle_client(void *client_socket_ptr)
{
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    char buffer[MAX_BUFFER];
    write_string(client_socket, "Welcome to the Bank!\nSelect role: 1 Admin, 2 Manager, 3 Employee, 4 Customer\nChoice: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
    {
        close(client_socket);
        return NULL;
    }

    UserRole expectedRole;
    switch (atoi(buffer))
    {
    case 1: expectedRole = ADMINISTRATOR; break;
    case 2: expectedRole = MANAGER; break;
    case 3: expectedRole = EMPLOYEE; break;
    case 4: expectedRole = CUSTOMER; break;
    default:
        write_string(client_socket, "Invalid role.\n");
        close(client_socket);
        return NULL;
    }

    write_string(client_socket, "User ID: ");
    if (read_client_input(client_socket, buffer, sizeof(buffer)) == -1)
    {
        close(client_socket);
        return NULL;
    }
    int user_id = atoi(buffer);

    write_string(client_socket, "Password: ");
    char password[MAX_BUFFER];
    if (read_client_input(client_socket, password, sizeof(password)) == -1)
    {
        close(client_socket);
        return NULL;
    }

    User user = check_login(user_id, password);
    if (user.userId == -2)
        write_string(client_socket, "Login failed: account deactivated.\n");
    else if (user.userId <= 0)
        write_string(client_socket, "Login failed: invalid credentials.\n");
    else if (user.role != expectedRole)
        write_string(client_socket, "Login failed: selected role does not match this user.\n");
    else
    {
        int session = try_add_session(user.userId);
        if (session == 0)
            write_string(client_socket, "Login failed: user already logged in.\n");
        else if (session < 0)
            write_string(client_socket, "Login failed: session capacity reached.\n");
        else
        {
            write_string(client_socket, "Login successful.\n");
            switch (user.role)
            {
            case CUSTOMER: account_selection_menu(client_socket, user); break;
            case EMPLOYEE: employee_menu(client_socket, user); break;
            case MANAGER: manager_menu(client_socket, user); break;
            case ADMINISTRATOR: admin_menu(client_socket, user); break;
            }
            remove_session(user.userId);
        }
    }

    close(client_socket);
    return NULL;
}

int main(void)
{
    initialize_indexes();
    run_server_recovery();
    initialize_indexes();

    pthread_t workers[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        if (pthread_create(&workers[i], NULL, worker_main, NULL) != 0)
        {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
        pthread_detach(workers[i]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(server_fd, SOCKET_QUEUE_CAPACITY) < 0)
    {
        perror("server setup");
        close(server_fd);
        return EXIT_FAILURE;
    }

    write_string(STDOUT_FILENO, "Bank server listening on port 8080 with bounded worker pool.\n");
    for (;;)
    {
        struct sockaddr_in client_address;
        socklen_t len = sizeof(client_address);
        int client_socket = accept(server_fd, (struct sockaddr *)&client_address, &len);
        if (client_socket < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }
        queue_push(client_socket);
    }
}
