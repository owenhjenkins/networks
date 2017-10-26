CC=gcc
CFLAGS= -lssl -lcrypto -pthread -I./src -std=c99
DEPS = src/server.h

all: src/servermain.o src/serverfunc.o 
	$(CC) -o server src/servermain.o src/serverfunc.o $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
