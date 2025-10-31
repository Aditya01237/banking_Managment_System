# --- Compiler and Flags ---
CC = gcc
# CFLAGS: Compiler flags
# -Iinclude: Look in the 'include' folder for header files (.h)
# -Wall -Wextra: Show all recommended warnings
# -g: Include debugging information
CFLAGS = -Iinclude -Wall -Wextra -g
# LDFLAGS: Linker flags
# -lpthread: Link the POSIX Threads library (for the server)
LDFLAGS_SERVER = -lpthread

# --- Directories ---
SRC_DIR = src
OBJ_DIR = obj

# --- Executable Targets ---
TARGET_SERVER = server
TARGET_CLIENT = client
TARGET_ADMIN = admin_util

# --- Object File Lists ---
# Find all .c files in the src directory
SRCS = $(wildcard $(SRC_DIR)/*.c)

# List all .o (object) files that will be created in the obj directory
# This maps src/server.c to obj/server.o, src/customer.c to obj/customer.o, etc.
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Specific object files needed for each executable
# $(OBJ_DIR)/common_utils.o
COMMON_OBJS = $(OBJ_DIR)/common_utils.o
# $(OBJ_DIR)/data_access.o
DATA_OBJS = $(OBJ_DIR)/data_access.o
# $(OBJ_DIR)/customer.o $(OBJ_DIR)/employee.o $(OBJ_DIR)/manager.o $(OBJ_DIR)/admin.o
ROLE_OBJS = $(OBJ_DIR)/customer.o $(OBJ_DIR)/employee.o $(OBJ_DIR)/manager.o $(OBJ_DIR)/admin.o

# The server needs (almost) everything
SERVER_OBJS = $(OBJ_DIR)/server.o $(COMMON_OBJS) $(DATA_OBJS) $(ROLE_OBJS)
# The client is simple
CLIENT_OBJS = $(OBJ_DIR)/client.o $(COMMON_OBJS)
# The admin util needs data access and common utils
ADMIN_OBJS = $(OBJ_DIR)/admin_util.o $(COMMON_OBJS) $(DATA_OBJS)

# --- Build Rules ---

# The 'all' rule is the default. Typing 'make' will run this.
# It depends on our three executable files.
.PHONY: all
all: $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_ADMIN)

# Rule to link the server
$(TARGET_SERVER): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS_SERVER)
	@echo "Server build complete."

# Rule to link the client
$(TARGET_CLIENT): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Client build complete."

# Rule to link the admin utility
$(TARGET_ADMIN): $(ADMIN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Admin utility build complete."

# Pattern rule: How to build any .o file from its .c file
# $<: The first dependency (the .c file)
# $@: The target (the .o file)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the 'obj' directory if it doesn't exist
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Rule to create the 'data' directory (used by admin_util)
# We can make 'admin_util' depend on this.
$(TARGET_ADMIN): | data
data:
	@mkdir -p data

# --- Cleanup Rule ---
.PHONY: clean
clean:
	@rm -rf $(OBJ_DIR) $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_ADMIN)
	@rm -f data/*.dat data/*.log
	@echo "Cleanup complete. Removed all build artifacts and data files."