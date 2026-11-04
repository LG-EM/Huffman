CC = gcc
CFLAGS = -ansi -pedantic -Wall -Wextra -O2
TARGET = huffman
OBJS = bitio.o huffman.o main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bitio.o: bitio.c bitio.h
huffman.o: huffman.c huffman.h bitio.h
main.o: main.c huffman.h

clean:
	rm -f $(OBJS) $(TARGET)