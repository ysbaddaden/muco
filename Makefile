.POSIX:

CC = clang-5.0
CFLAGS = -g -O3 -std=gnu11 -Iinclude -Isrc -Wall -Wextra -Wpedantic $(FLAGS)
LDFLAGS = -latomic -lpthread ./libmuco.a
OBJECTS = src/muco.o src/pcg_basic.o

all: libmuco.a

libmuco.a: $(OBJECTS)
	$(AR) -cr libmuco.a $(OBJECTS)

main: samples/main.o libmuco.a
	$(CC) samples/main.o -o main $(LDFLAGS)

switch: samples/switch.o libmuco.a
	$(CC) samples/switch.o -o switch $(LDFLAGS)

clean: .phony
	rm -f libmuco.a src/*.o samples/*.o main switch

.phony:
