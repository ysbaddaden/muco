.POSIX:

CC = clang-5.0
CFLAGS = -O3 -std=gnu11 -Iinclude -Isrc -Wall -Wextra -Wpedantic
LDFLAGS = -latomic ./libcoro.a
OBJECTS = src/fiber.o src/scheduler.o src/deque.o

all: libcoro.a

libcoro.a: $(OBJECTS)
	$(AR) -cr libcoro.a $(OBJECTS)

main: samples/main.o libcoro.a
	$(CC) samples/main.o -o main $(LDFLAGS)

switch: samples/switch.o libcoro.a
	$(CC) samples/switch.o -o switch $(LDFLAGS)

clean: .phony
	rm -f libcoro.a src/*.o samples/*.o main switch

.phony:
