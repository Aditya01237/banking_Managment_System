#include "common.h"
#include "data_access.h"
#include <assert.h>

#define THREADS 16
#define LOOPS 1000

typedef struct { int from; int to; } Args;

static void transfer_one_rupee(int from, int to)
{
    int rf = find_account_record_by_id(from);
    int rt = find_account_record_by_id(to);
    lock_account_ids(from, to);
    int fd = open(ACCOUNT_FILE, O_RDWR);
    assert(fd >= 0);

    int first_id = from < to ? from : to;
    int first_record = first_id == from ? rf : rt;
    int second_record = first_id == from ? rt : rf;
    Account first, second;
    assert(read_account_locked_fd(fd, first_record, &first) == 0);
    assert(read_account_locked_fd(fd, second_record, &second) == 0);

    Account *src = first.accountId == from ? &first : &second;
    Account *dst = first.accountId == to ? &first : &second;
    if (src->balance >= 100)
    {
        src->balance -= 100;
        dst->balance += 100;
        assert(write_account_locked_fd(fd, rf, src) == 0);
        assert(write_account_locked_fd(fd, rt, dst) == 0);
    }

    set_record_lock(fd, second_record, sizeof(Account), F_UNLCK);
    set_record_lock(fd, first_record, sizeof(Account), F_UNLCK);
    close(fd);
    unlock_account_ids(from, to);
}

static void *runner(void *ptr)
{
    Args *a = ptr;
    for (int i = 0; i < LOOPS; ++i)
        transfer_one_rupee(a->from, a->to);
    return NULL;
}

int main(void)
{
    initialize_indexes();
    Account a0 = getAccount(1), b0 = getAccount(2);
    Money initial_total = a0.balance + b0.balance;

    pthread_t threads[THREADS];
    Args args[THREADS];
    for (int i = 0; i < THREADS; ++i)
    {
        args[i].from = (i % 2 == 0) ? 1 : 2;
        args[i].to = (i % 2 == 0) ? 2 : 1;
        assert(pthread_create(&threads[i], NULL, runner, &args[i]) == 0);
    }
    for (int i = 0; i < THREADS; ++i)
        pthread_join(threads[i], NULL);

    Account a = getAccount(1), b = getAccount(2);
    assert(a.balance >= 0 && b.balance >= 0);
    assert(a.balance + b.balance == initial_total);
    puts("test_concurrency: PASS");
    return 0;
}
