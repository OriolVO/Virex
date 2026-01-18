# Virex Compiler Makefile

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Iinclude -Ilib
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
TEST_DIR = tests

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Target executable
TARGET = virexc

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files to create executable
$(TARGET): $(OBJS) runtime/virex_runtime.o
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# Build runtime object
runtime/virex_runtime.o: runtime/virex_runtime.c
	$(CC) -O2 -c runtime/virex_runtime.c -o runtime/virex_runtime.o

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Clean complete"

# Run tests (placeholder for now)
test: $(TARGET)
	@echo "Running tests..."
	@echo "No tests implemented yet"

# Debug build
debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean all

# Install (placeholder)
install: $(TARGET)
	@echo "Install not yet implemented"

# Phony targets
.PHONY: all clean test debug release install

# Show help
help:
	@echo "Virex Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the compiler (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Run tests"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release version"
	@echo "  help     - Show this help message"
