CFLAGS = -Wall -Wextra -g -fsanitize=address

all: build/myqueue

build/myqueue: build src/main.c src/myqueue.c src/myqueue.h
	gcc $(CFLAGS) -o $@ src/main.c src/myqueue.c


build: 
	mkdir -p build

clean:
	rm -rf build/myqueue