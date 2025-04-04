CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = filesys
OBJS = filesys.o fat32.o lexer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

filesys.o: filesys.c fat32.h lexer.h
	$(CC) $(CFLAGS) -c filesys.c

fat32.o: fat32.c fat32.h
	$(CC) $(CFLAGS) -c fat32.c

lexer.o: lexer.c lexer.h
	$(CC) $(CFLAGS) -c lexer.c

clean:
	rm -f $(TARGET) $(OBJS)

