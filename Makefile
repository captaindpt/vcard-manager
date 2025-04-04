# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c11 -g -I./include
LDFLAGS = -shared

# Directories
SRC_DIR = src
BIN_DIR = bin

# Source files
SOURCES = $(SRC_DIR)/VCParser.c $(SRC_DIR)/LinkedListAPI.c $(SRC_DIR)/CardWriter.c

# Output file
OUTPUT = $(BIN_DIR)/libvcparser.so

# Default target
all: parser

# Build the shared library
parser: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -fpic -o $(OUTPUT) $(SOURCES)

# Clean up
clean:
	rm -f $(BIN_DIR)/*.so $(SRC_DIR)/*.o

# Phony targets
.PHONY: all parser clean 