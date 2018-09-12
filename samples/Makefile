.POSIX:

CC = clang-6.0
CFLAGS = -g -O3 -std=gnu11 -I../include -I../src -Wall -Wextra -Wpedantic $(FLAGS)
LDFLAGS = -lpthread ../libmuco.a

all: main

../libmuco.a: .phony
	cd .. && make libmuco.a

main: main.o ../libmuco.a
	$(CC) main.o -o main $(LDFLAGS)

channel: channel.o ../libmuco.a
	$(CC) channel.o -o channel $(LDFLAGS)

buffered_channel: buffered_channel.o ../libmuco.a
	$(CC) buffered_channel.o -o buffered_channel $(LDFLAGS)

clean: .phony
	rm -f main

.phony:
