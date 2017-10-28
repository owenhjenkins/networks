CC=gcc
CFLAGS= -lssl -lcrypto -pthread -I./src -std=c99
DEPS = server.h

all: servermain.o serverfunc.o 
	$(CC) -o ../server servermain.o serverfunc.o $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
