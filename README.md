# Concurrent Banking Transaction Server

A Linux systems/backend project written in **C** that implements a concurrent TCP banking server with crash-safe fund transfers, record-level persistence, role-based access control, and bounded worker concurrency.

This is intentionally **not** a web CRUD application. The project focuses on operating-system and backend concepts: sockets, pthreads, synchronization, file descriptors, record locks, write-ahead logging, recovery, durability, indexing, and secure credential storage.

## Why this project is interesting

The server handles real concurrency problems that a simple banking demo normally avoids:

- multiple clients can operate concurrently;
- two-account transfers acquire locks in deterministic order to avoid deadlock;
- in-process `pthread` lock stripes complement process-associated `fcntl` locks;
- balances are stored as integer **paise**, avoiding floating-point money errors;
- transfers write undo records to a transaction-ID based WAL before modifying account data;
- `fsync()` is used on critical persistent writes;
- incomplete transactions are rolled back during server startup;
- a bounded worker pool provides backpressure instead of creating one thread per connection;
- account/user hot-path lookups use startup-built in-memory hash indexes;
- passwords are stored as **Argon2id hashes** using libsodium, never plaintext;
- CI runs strict compilation, automated tests, AddressSanitizer, and UndefinedBehaviorSanitizer builds.

## Architecture

```text
                         TCP clients
                    ┌────────┼────────┐
                    │        │        │
                    ▼        ▼        ▼
              ┌──────────────────────────┐
              │      Listening Socket    │
              └────────────┬─────────────┘
                           ▼
                 ┌──────────────────┐
                 │  Bounded Socket  │
                 │      Queue       │
                 └────────┬─────────┘
                          ▼
              ┌──────────────────────────┐
              │  Fixed pthread workers   │
              │ authentication/sessions  │
              └────────────┬─────────────┘
                           ▼
              ┌──────────────────────────┐
              │   Transaction / Role     │
              │       Operations         │
              └───────┬─────────┬────────┘
                      │         │
                 lock stripes   │ transaction-ID WAL
                      │         │ + fsync
                      ▼         ▼
              ┌──────────────────────────┐
              │ Binary persistent files  │
              │ users/accounts/loans/... │
              └────────────┬─────────────┘
                           ▲
                    startup recovery
```

## Core concurrency model

### Bounded worker pool

The listening thread accepts sockets and pushes them into a bounded producer/consumer queue. A fixed group of worker threads waits on a condition variable and processes connections.

This avoids unbounded thread creation and gives the server a simple form of backpressure when the queue is full.

### Account isolation

`fcntl` locks are useful for coordinating record access across processes, but traditional POSIX record locks are process-associated. Threads inside the same server therefore also use hashed `pthread_mutex_t` account lock stripes.

For a transfer from account `A` to `B`, the server:

1. determines both account records;
2. acquires the two in-process account locks in deterministic order;
3. acquires both persistent record locks in deterministic account-ID order;
4. rereads both records while the transaction locks are held;
5. validates account state and sufficient funds;
6. writes WAL undo entries and `fsync()`s them;
7. debits and credits the two account records;
8. writes a commit record and `fsync()`s it;
9. releases record and thread locks.

Deterministic ordering prevents the classic `A -> B` / `B -> A` deadlock cycle.

## Crash recovery / Write-Ahead Log

Each transfer receives a unique transaction ID. Before account records are changed, the journal records the previous balances:

```text
TX 91231 | UNDO | account 15 | old balance 500000
TX 91231 | UNDO | account 44 | old balance 210000
TX 91231 | COMMIT
```

Journal writes are forced to durable storage with `fsync()` before the account update proceeds.

At startup the recovery pass groups journal entries by transaction ID. Transactions with a commit record are left intact. Transactions containing undo records but no commit are restored to their previous balances.

For fault-injection testing, the environment variable below deliberately kills the server after the sender debit has reached disk:

```bash
BANK_CRASH_AFTER_DEBIT=1 ./server
```

Restarting the server exercises WAL recovery.

## Exact monetary representation

Money is represented using signed 64-bit integer paise:

```c
typedef int64_t Money;
```

So:

```text
₹1250.75 -> 125075
₹0.10    -> 10
```

This avoids binary floating-point rounding problems in account balances and loan amounts.

## Authentication

Passwords are never persisted as plaintext. `crypto_pwhash_str()` from libsodium stores an encoded Argon2id password hash containing its parameters and salt. Login uses `crypto_pwhash_str_verify()`.

## In-memory indexes

At startup, the data layer scans persistent user/account files and builds open-addressed hash indexes for:

```text
user ID        -> record number
account ID     -> record number
account number -> record number
```

This removes repeated full-file scans from common authentication and account lookup paths, giving expected constant-time index lookup before direct record access.

## Features

- Customer, Employee, Manager, and Administrator roles
- Multiple accounts per customer
- Deposit and withdrawal
- Atomic account-to-account transfer
- Transaction history
- Loan application, assignment, approval, rejection, and account credit
- Feedback workflow
- User activation/deactivation
- Single-active-session protection
- Persistent binary-file storage using `open/read/write/lseek`
- Shared/exclusive `fcntl` locks
- `pthread` synchronization
- TCP client/server communication

## Build

### Dependencies

Ubuntu/Debian:

```bash
sudo apt-get install build-essential libsodium-dev
```

macOS with Homebrew:

```bash
brew install libsodium
```

### Compile

```bash
make
```

### Initialize local runtime data

```bash
./admin_util
```

Seed users:

```text
Admin     1 / admin123
Customer  2 / customer123
Employee  3 / employee123
Manager   4 / manager123
```

These credentials are only seed inputs; the persisted user records contain Argon2id hashes.

### Run

Terminal 1:

```bash
./server
```

Terminal 2:

```bash
./client
```

## Tests

Run the automated suite:

```bash
make test
```

The current suite checks:

- exact decimal-to-paise conversion;
- rejection of invalid monetary input;
- Argon2id hash/verify behavior;
- indexed user/account lookups;
- seeded balance correctness;
- concurrent bidirectional transfers across 16 threads;
- total-balance conservation and non-negative balance invariants.

Build with sanitizers:

```bash
make sanitize
```

GitHub Actions performs the build, tests, and sanitizer build on pushes and pull requests.

## Repository layout

```text
include/                  public module interfaces
src/
  server.c                socket listener, worker pool, sessions, recovery
  data_access.c           persistence, indexes, locks, WAL helpers
  customer.c              customer and transaction flows
  employee.c              customer/loan operations
  manager.c               loan assignment and feedback/status flows
  admin.c                 administrator menu
  client.c                terminal TCP client
  common_utils.c          input, money, password helpers
  admin_util.c            seed-data generator
tests/
  test_core.c
  test_concurrency.c
.github/workflows/ci.yml
Makefile
```

Runtime `.dat` files, logs, executables, object files, and platform metadata are ignored rather than committed.

## Interview discussion topics

This project is designed to support concrete systems discussions around:

- why `fcntl` locking alone is insufficient for sibling pthreads;
- race conditions and lost updates;
- deterministic lock ordering and deadlock prevention;
- WAL ordering and crash consistency;
- `fsync()` and durability;
- fixed thread pools vs thread-per-connection servers;
- producer/consumer queues and condition variables;
- integer vs floating-point money representation;
- password hashing vs encryption;
- persistent-file scans vs in-memory indexes;
- fault injection and invariant-based concurrency testing.
