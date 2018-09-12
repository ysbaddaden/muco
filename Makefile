.POSIX:

#CC = clang-6.0 -mcx16
CC = clang-6.0
CFLAGS = -g -O3 -std=gnu11 -Iinclude -Isrc -Wall -Wextra -Wpedantic $(FLAGS)
LDFLAGS = -lpthread ./libmuco.a
OBJECTS = src/muco.o src/mutex.o src/channel.o

all: libmuco.a

libmuco.a: $(OBJECTS)
	$(AR) -cr libmuco.a $(OBJECTS)

main: samples/main.o libmuco.a
	$(CC) samples/main.o -o main $(LDFLAGS)

switch: samples/switch.o libmuco.a
	$(CC) samples/switch.o -o switch $(LDFLAGS)

mutex: samples/mutex.o libmuco.a
	$(CC) samples/mutex.o -o mutex $(LDFLAGS)

queue: samples/queue.o libmuco.a
	$(CC) samples/queue.o -o queue $(LDFLAGS)

channel: samples/channel.o libmuco.a
	$(CC) samples/channel.o -o channel $(LDFLAGS)

clean: .phony
	rm -f libmuco.a src/*.o samples/*.o main switch mutex channel

.phony:
