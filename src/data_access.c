#include "data_access.h"
#include <stdatomic.h>

#define INDEX_CAPACITY 4096
#define ACCOUNT_LOCK_STRIPES 257

typedef struct
{
    int used;
    int key;
    int record;
} IntIndexEntry;

typedef struct
{
    int used;
    char key[20];
    int record;
} StringIndexEntry;

static IntIndexEntry user_index[INDEX_CAPACITY];
static IntIndexEntry account_id_index[INDEX_CAPACITY];
static StringIndexEntry account_number_index[INDEX_CAPACITY];
static pthread_rwlock_t index_lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t account_locks[ACCOUNT_LOCK_STRIPES];
static pthread_once_t account_locks_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t journal_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_ullong wal_sequence = 1;
static int indexes_ready = 0;

static void init_account_locks_once(void)
{
    for (int i = 0; i < ACCOUNT_LOCK_STRIPES; ++i)
        pthread_mutex_init(&account_locks[i], NULL);
}

static unsigned hash_int(int key)
{
    return ((unsigned)key * 2654435761u) % INDEX_CAPACITY;
}

static unsigned hash_string(const char *s)
{
    unsigned long h = 5381;
    while (*s)
        h = ((h << 5) + h) ^ (unsigned char)*s++;
    return (unsigned)(h % INDEX_CAPACITY);
}

static void int_index_put(IntIndexEntry *table, int key, int record)
{
    unsigned start = hash_int(key);
    for (unsigned i = 0; i < INDEX_CAPACITY; ++i)
    {
        unsigned pos = (start + i) % INDEX_CAPACITY;
        if (!table[pos].used || table[pos].key == key)
        {
            table[pos].used = 1;
            table[pos].key = key;
            table[pos].record = record;
            return;
        }
    }
}

static int int_index_get(IntIndexEntry *table, int key)
{
    unsigned start = hash_int(key);
    for (unsigned i = 0; i < INDEX_CAPACITY; ++i)
    {
        unsigned pos = (start + i) % INDEX_CAPACITY;
        if (!table[pos].used)
            return -1;
        if (table[pos].key == key)
            return table[pos].record;
    }
    return -1;
}

static void string_index_put(StringIndexEntry *table, const char *key, int record)
{
    unsigned start = hash_string(key);
    for (unsigned i = 0; i < INDEX_CAPACITY; ++i)
    {
        unsigned pos = (start + i) % INDEX_CAPACITY;
        if (!table[pos].used || strcmp(table[pos].key, key) == 0)
        {
            table[pos].used = 1;
            snprintf(table[pos].key, sizeof(table[pos].key), "%s", key);
            table[pos].record = record;
            return;
        }
    }
}

static int string_index_get(StringIndexEntry *table, const char *key)
{
    unsigned start = hash_string(key);
    for (unsigned i = 0; i < INDEX_CAPACITY; ++i)
    {
        unsigned pos = (start + i) % INDEX_CAPACITY;
        if (!table[pos].used)
            return -1;
        if (strcmp(table[pos].key, key) == 0)
            return table[pos].record;
    }
    return -1;
}

int set_file_lock(int fd, int lock_type)
{
    struct flock fl = {0};
    fl.l_type = (short)lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, F_SETLKW, &fl);
}

int set_record_lock(int fd, int record_num, int record_size, int lock_type)
{
    struct flock fl = {0};
    fl.l_type = (short)lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = (off_t)record_num * record_size;
    fl.l_len = record_size;
    return fcntl(fd, F_SETLKW, &fl);
}

void lock_account_ids(int account_a, int account_b)
{
    pthread_once(&account_locks_once, init_account_locks_once);
    unsigned a = (unsigned)account_a % ACCOUNT_LOCK_STRIPES;
    unsigned b = (unsigned)account_b % ACCOUNT_LOCK_STRIPES;
    if (a == b)
    {
        pthread_mutex_lock(&account_locks[a]);
        return;
    }
    if (a > b)
    {
        unsigned t = a;
        a = b;
        b = t;
    }
    pthread_mutex_lock(&account_locks[a]);
    pthread_mutex_lock(&account_locks[b]);
}

void unlock_account_ids(int account_a, int account_b)
{
    unsigned a = (unsigned)account_a % ACCOUNT_LOCK_STRIPES;
    unsigned b = (unsigned)account_b % ACCOUNT_LOCK_STRIPES;
    if (a == b)
    {
        pthread_mutex_unlock(&account_locks[a]);
        return;
    }
    if (a > b)
    {
        unsigned t = a;
        a = b;
        b = t;
    }
    pthread_mutex_unlock(&account_locks[b]);
    pthread_mutex_unlock(&account_locks[a]);
}

void invalidate_indexes(void)
{
    pthread_rwlock_wrlock(&index_lock);
    indexes_ready = 0;
    memset(user_index, 0, sizeof(user_index));
    memset(account_id_index, 0, sizeof(account_id_index));
    memset(account_number_index, 0, sizeof(account_number_index));
    pthread_rwlock_unlock(&index_lock);
}

int initialize_indexes(void)
{
    pthread_rwlock_wrlock(&index_lock);
    memset(user_index, 0, sizeof(user_index));
    memset(account_id_index, 0, sizeof(account_id_index));
    memset(account_number_index, 0, sizeof(account_number_index));

    int fd = open(USER_FILE, O_RDONLY);
    if (fd >= 0)
    {
        User u;
        int record = 0;
        while (read(fd, &u, sizeof(u)) == (ssize_t)sizeof(u))
            int_index_put(user_index, u.userId, record++);
        close(fd);
    }

    fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd >= 0)
    {
        Account a;
        int record = 0;
        while (read(fd, &a, sizeof(a)) == (ssize_t)sizeof(a))
        {
            int_index_put(account_id_index, a.accountId, record);
            string_index_put(account_number_index, a.accountNumber, record);
            record++;
        }
        close(fd);
    }

    indexes_ready = 1;
    pthread_rwlock_unlock(&index_lock);
    return 0;
}

static int ensure_indexes(void)
{
    pthread_rwlock_rdlock(&index_lock);
    int ready = indexes_ready;
    pthread_rwlock_unlock(&index_lock);
    if (!ready)
        return initialize_indexes();
    return 0;
}

static int get_next_id_from_file(const char *filename, size_t record_size)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        return 1;
    if (set_file_lock(fd, F_RDLCK) == -1)
    {
        close(fd);
        return 1;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    int next = 1;
    if (size >= (off_t)record_size && lseek(fd, size - (off_t)record_size, SEEK_SET) >= 0)
    {
        int id = 0;
        if (read(fd, &id, sizeof(id)) == (ssize_t)sizeof(id))
            next = id + 1;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return next;
}

int get_next_user_id(void) { return get_next_id_from_file(USER_FILE, sizeof(User)); }
int get_next_account_id(void) { return get_next_id_from_file(ACCOUNT_FILE, sizeof(Account)); }
int get_next_loan_id(void) { return get_next_id_from_file(LOAN_FILE, sizeof(Loan)); }
int get_next_feedback_id(void) { return get_next_id_from_file(FEEDBACK_FILE, sizeof(Feedback)); }
int get_next_transaction_id(void) { return get_next_id_from_file(TRANSACTION_FILE, sizeof(Transaction)); }

int find_user_record(int userId)
{
    ensure_indexes();
    pthread_rwlock_rdlock(&index_lock);
    int r = int_index_get(user_index, userId);
    pthread_rwlock_unlock(&index_lock);
    return r;
}

int find_account_record_by_id(int accountId)
{
    ensure_indexes();
    pthread_rwlock_rdlock(&index_lock);
    int r = int_index_get(account_id_index, accountId);
    pthread_rwlock_unlock(&index_lock);
    return r;
}

int find_account_record_by_number(char *acc_num)
{
    ensure_indexes();
    pthread_rwlock_rdlock(&index_lock);
    int r = string_index_get(account_number_index, acc_num);
    pthread_rwlock_unlock(&index_lock);
    return r;
}

static int find_record_linear(const char *filename, size_t record_size, int id)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -1;
    set_file_lock(fd, F_RDLCK);
    int record = 0;
    unsigned char buf[sizeof(Feedback) > sizeof(Loan) ? sizeof(Feedback) : sizeof(Loan)];
    while (read(fd, buf, record_size) == (ssize_t)record_size)
    {
        if (*(int *)buf == id)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return record;
        }
        record++;
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_loan_record(int loanId) { return find_record_linear(LOAN_FILE, sizeof(Loan), loanId); }
int find_feedback_record(int feedbackId) { return find_record_linear(FEEDBACK_FILE, sizeof(Feedback), feedbackId); }

int find_user_by_phone(const char *phone)
{
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1)
        return -1;
    set_file_lock(fd, F_RDLCK);
    User u;
    while (read(fd, &u, sizeof(u)) == (ssize_t)sizeof(u))
    {
        if (strcmp(u.phone, phone) == 0)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return 0;
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

int find_user_by_email(const char *email)
{
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1)
        return -1;
    set_file_lock(fd, F_RDLCK);
    User u;
    while (read(fd, &u, sizeof(u)) == (ssize_t)sizeof(u))
    {
        if (strcmp(u.email, email) == 0)
        {
            set_file_lock(fd, F_UNLCK);
            close(fd);
            return 0;
        }
    }
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return -1;
}

static int read_record(void *out, int record_num, size_t record_size, const char *filename)
{
    if (record_num < 0)
        return -1;
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -1;
    if (set_record_lock(fd, record_num, (int)record_size, F_RDLCK) == -1)
    {
        close(fd);
        return -1;
    }
    int ok = lseek(fd, (off_t)record_num * record_size, SEEK_SET) >= 0 &&
             read(fd, out, record_size) == (ssize_t)record_size;
    set_record_lock(fd, record_num, (int)record_size, F_UNLCK);
    close(fd);
    return ok ? 0 : -1;
}

User getUser(int userId)
{
    User u = {.userId = -1};
    read_record(&u, find_user_record(userId), sizeof(u), USER_FILE);
    return u;
}

Account getAccount(int accountId)
{
    Account a = {.accountId = -1};
    pthread_once(&account_locks_once, init_account_locks_once);
    unsigned stripe = (unsigned)accountId % ACCOUNT_LOCK_STRIPES;
    pthread_mutex_lock(&account_locks[stripe]);
    read_record(&a, find_account_record_by_id(accountId), sizeof(a), ACCOUNT_FILE);
    pthread_mutex_unlock(&account_locks[stripe]);
    return a;
}

Account getAccountByNum(char *accNum)
{
    Account a = {.accountId = -1};
    int record = find_account_record_by_number(accNum);
    if (record < 0)
        return a;
    if (read_record(&a, record, sizeof(a), ACCOUNT_FILE) != 0)
        a.accountId = -1;
    return a;
}

Loan getLoan(int loanId)
{
    Loan l = {.loanId = -1};
    read_record(&l, find_loan_record(loanId), sizeof(l), LOAN_FILE);
    return l;
}

Feedback getFeedback(int feedbackId)
{
    Feedback f = {.feedbackId = -1};
    read_record(&f, find_feedback_record(feedbackId), sizeof(f), FEEDBACK_FILE);
    return f;
}

int getAccountsByOwnerId(int ownerUserId, Account *list, int maxAccounts)
{
    int fd = open(ACCOUNT_FILE, O_RDONLY);
    if (fd == -1)
        return 0;
    set_file_lock(fd, F_RDLCK);
    Account a;
    int count = 0;
    while (count < maxAccounts && read(fd, &a, sizeof(a)) == (ssize_t)sizeof(a))
        if (a.ownerUserId == ownerUserId && a.isActive)
            list[count++] = a;
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return count;
}

static int append_record(void *record, size_t size, const char *filename)
{
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
        return -1;
    if (set_file_lock(fd, F_WRLCK) == -1)
    {
        close(fd);
        return -1;
    }
    ssize_t n = write(fd, record, size);
    int ok = n == (ssize_t)size && fsync(fd) == 0;
    set_file_lock(fd, F_UNLCK);
    close(fd);
    return ok ? 0 : -1;
}

static int update_record(void *record, int record_num, size_t size, const char *filename)
{
    if (record_num < 0)
        return -1;
    int fd = open(filename, O_WRONLY);
    if (fd == -1)
        return -1;
    if (set_record_lock(fd, record_num, (int)size, F_WRLCK) == -1)
    {
        close(fd);
        return -1;
    }
    int ok = lseek(fd, (off_t)record_num * size, SEEK_SET) >= 0 &&
             write(fd, record, size) == (ssize_t)size && fsync(fd) == 0;
    set_record_lock(fd, record_num, (int)size, F_UNLCK);
    close(fd);
    return ok ? 0 : -1;
}

int addUser(User u)
{
    if (u.userId <= 0)
        u.userId = get_next_user_id();
    if (append_record(&u, sizeof(u), USER_FILE) != 0)
        return -1;
    initialize_indexes();
    return 0;
}

int addAccount(Account a)
{
    if (a.accountId <= 0)
        a.accountId = get_next_account_id();
    if (append_record(&a, sizeof(a), ACCOUNT_FILE) != 0)
        return -1;
    initialize_indexes();
    return 0;
}

int addLoan(Loan l)
{
    if (l.loanId <= 0)
        l.loanId = get_next_loan_id();
    return append_record(&l, sizeof(l), LOAN_FILE);
}

int addFeedback(Feedback f)
{
    if (f.feedbackId <= 0)
        f.feedbackId = get_next_feedback_id();
    return append_record(&f, sizeof(f), FEEDBACK_FILE);
}

int addTransaction(Transaction t)
{
    if (t.transactionId <= 0)
        t.transactionId = get_next_transaction_id();
    t.timestamp = time(NULL);
    return append_record(&t, sizeof(t), TRANSACTION_FILE);
}

int updateUser(User u) { return update_record(&u, find_user_record(u.userId), sizeof(u), USER_FILE); }

int updateAccount(Account a)
{
    pthread_once(&account_locks_once, init_account_locks_once);
    unsigned stripe = (unsigned)a.accountId % ACCOUNT_LOCK_STRIPES;
    pthread_mutex_lock(&account_locks[stripe]);
    int rc = update_record(&a, find_account_record_by_id(a.accountId), sizeof(a), ACCOUNT_FILE);
    pthread_mutex_unlock(&account_locks[stripe]);
    return rc;
}

int updateLoan(Loan l) { return update_record(&l, find_loan_record(l.loanId), sizeof(l), LOAN_FILE); }
int updateFeedback(Feedback f) { return update_record(&f, find_feedback_record(f.feedbackId), sizeof(f), FEEDBACK_FILE); }

void generate_new_account_number(char *out)
{
    int id = get_next_account_id();
    snprintf(out, 20, "SB%d", 10000 + id);
}

int read_account_locked_fd(int fd, int record_num, Account *out)
{
    if (set_record_lock(fd, record_num, sizeof(Account), F_WRLCK) == -1)
        return -1;
    if (lseek(fd, (off_t)record_num * sizeof(Account), SEEK_SET) < 0 ||
        read(fd, out, sizeof(*out)) != (ssize_t)sizeof(*out))
    {
        set_record_lock(fd, record_num, sizeof(Account), F_UNLCK);
        return -1;
    }
    return 0;
}

int write_account_locked_fd(int fd, int record_num, const Account *a)
{
    if (lseek(fd, (off_t)record_num * sizeof(Account), SEEK_SET) < 0)
        return -1;
    if (write(fd, a, sizeof(*a)) != (ssize_t)sizeof(*a))
        return -1;
    return fsync(fd);
}

uint64_t next_wal_transaction_id(void)
{
    uint64_t seq = atomic_fetch_add(&wal_sequence, 1);
    return ((uint64_t)time(NULL) << 20) ^ ((uint64_t)getpid() << 8) ^ seq;
}

int journal_log_entry(JournalEntry entry)
{
    pthread_mutex_lock(&journal_mutex);
    int fd = open(JOURNAL_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    int rc = -1;
    if (fd >= 0)
    {
        ssize_t n = write(fd, &entry, sizeof(entry));
        if (n == (ssize_t)sizeof(entry) && fsync(fd) == 0)
            rc = 0;
        close(fd);
    }
    pthread_mutex_unlock(&journal_mutex);
    return rc;
}

int journal_log_clear(void)
{
    pthread_mutex_lock(&journal_mutex);
    int fd = open(JOURNAL_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    int rc = fd >= 0 ? fsync(fd) : -1;
    if (fd >= 0)
        close(fd);
    pthread_mutex_unlock(&journal_mutex);
    return rc;
}
