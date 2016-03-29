CC = gcc
TARGET = shell server
OBJECT = shell.o util.o server.o

all: server shell

server: server.o util.o
	$(CC) -o server server.o util.o

server.o: server.c
	$(CC) -c server.c

shell: shell.o util.o
	$(CC) -o shell shell.o util.o

shell.o: shell.c
	$(CC) -c shell.c

util.o: util.c
	$(CC) -c util.c

clean:
	rm -f $(OBJECT) $(TARGET)
