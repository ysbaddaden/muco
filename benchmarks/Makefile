.POSIX:

#CC = clang-6.0 -mcx16
CC = clang-6.0
CFLAGS = -g -O3 -std=gnu11 -I../include -I../src -Wall -Wextra -Wpedantic $(FLAGS)
LDFLAGS = -lpthread ../libmuco.a

all: switch mutex queue channel

../libmuco.a: .phony
	cd .. && make libmuco.a

switch: switch.o ../libmuco.a
	$(CC) switch.o -o switch $(LDFLAGS)

mutex: mutex.o ../libmuco.a
	$(CC) mutex.o -o mutex $(LDFLAGS)

queue: queue.o ../libmuco.a
	$(CC) queue.o -o queue $(LDFLAGS)

channel: channel.o ../libmuco.a
	$(CC) channel.o -o channel $(LDFLAGS)

clean: .phony
	rm -f switch mutex queue channel

.phony:
