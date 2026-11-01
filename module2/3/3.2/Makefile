CFLAGS = -Wall -Wextra -g -fsanitize=address

all: build/netsim

build/netsim: build src/main.c src/netsim.c src/netsim.h
	gcc $(CFLAGS) -o $@ src/main.c src/netsim.c


build: 
	mkdir -p build

clean:
	rm -rf build/netsim