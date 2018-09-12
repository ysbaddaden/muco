.POSIX:

#CC = clang-6.0 -mcx16
CC = clang-6.0
CFLAGS = -g -O3 -std=gnu11 -Iinclude -Isrc -Wall -Wextra -Wpedantic $(FLAGS)
LDFLAGS = -lpthread ./libmuco.a
OBJECTS = src/muco.o src/mutex.o src/channel.o

all: libmuco.a

libmuco.a: $(OBJECTS)
	$(AR) -cr libmuco.a $(OBJECTS)

clean: .phony
	rm -f libmuco.a src/*.o samples/*.o
	cd samples && make clean
	cd benchmarks && make clean

.phony:
