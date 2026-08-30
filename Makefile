CC ?= gcc
CFLAGS ?= -Iinclude -std=c11 -Wall -Wextra -Wpedantic -g -D_XOPEN_SOURCE=700
LDLIBS = -lpthread -lsodium

SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = tests

COMMON_OBJ = $(OBJ_DIR)/common_utils.o
DATA_OBJ = $(OBJ_DIR)/data_access.o
ROLE_OBJS = $(OBJ_DIR)/customer.o $(OBJ_DIR)/employee.o $(OBJ_DIR)/manager.o $(OBJ_DIR)/admin.o
SERVER_OBJS = $(OBJ_DIR)/server.o $(COMMON_OBJ) $(DATA_OBJ) $(ROLE_OBJS)
CLIENT_OBJS = $(OBJ_DIR)/client.o $(COMMON_OBJ)
ADMIN_OBJS = $(OBJ_DIR)/admin_util.o $(COMMON_OBJ) $(DATA_OBJ)

.PHONY: all clean sanitize test
all: server client admin_util

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

admin_util: $(ADMIN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=address,undefined" \
	        LDLIBS="$(LDLIBS) -fsanitize=address,undefined"

$(TEST_DIR)/test_core: $(TEST_DIR)/test_core.c $(SRC_DIR)/common_utils.c $(SRC_DIR)/data_access.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_DIR)/test_concurrency: $(TEST_DIR)/test_concurrency.c $(SRC_DIR)/common_utils.c $(SRC_DIR)/data_access.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: all $(TEST_DIR)/test_core $(TEST_DIR)/test_concurrency
	./admin_util >/dev/null
	./$(TEST_DIR)/test_core
	./admin_util >/dev/null
	./$(TEST_DIR)/test_concurrency

clean:
	rm -rf $(OBJ_DIR) server client admin_util $(TEST_DIR)/test_core $(TEST_DIR)/test_concurrency
	rm -f data/*.dat data/*.log
