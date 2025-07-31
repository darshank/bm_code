# Project settings
TARGET = compress_test
SRC = compress_test.c
MINIZ_DIR = algos
MINIZ_SRC = $(MINIZ_DIR)/miniz.c $(MINIZ_DIR)/miniz_tdef.c $(MINIZ_DIR)/miniz_tinfl.c
INCLUDES = -I$(MINIZ_DIR)

# Compiler settings
CC = gcc
CFLAGS = -Wall -O2 $(INCLUDES)

# Build target
all: $(TARGET)

$(TARGET): $(SRC) $(MINIZ_SRC)
	$(CC) $(CFLAGS) $(SRC) $(MINIZ_SRC) -o $(TARGET)

# Clean up
clean:
	rm -f $(TARGET)

