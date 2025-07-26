TARGET = lz4_test
SRC = lz4_test.c
LZ4_SRC = lz4.c
INCLUDES = -I.

CC = gcc
CFLAGS = -Wall -O2 $(INCLUDES)

all: $(TARGET)

$(TARGET): $(SRC) $(LZ4_SRC)
	$(CC) $(CFLAGS) $(SRC) $(LZ4_SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

