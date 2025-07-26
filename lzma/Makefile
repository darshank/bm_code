TARGET = lzma_test
SRC = lzma_test.c
LZMA_SRC =CpuArch.c Alloc.c LzmaEnc.c LzmaDec.c LzFind.c LzmaLib.c
INCLUDES = -I.

CC = gcc
CFLAGS = -Wall -O2 $(INCLUDES) -DZ7_ST

all: $(TARGET)

$(TARGET): $(SRC) $(LZMA_SRC)
	$(CC) $(CFLAGS) $(SRC) $(LZMA_SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

