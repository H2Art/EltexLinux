CFLAGS = -Wall -Wextra -g -fsanitize=address

all: build/mychmod

build/mychmod: build src/main.c src/mychmod.c src/mychmod.h
	gcc $(CFLAGS) -o $@ src/main.c src/mychmod.c


build: 
	mkdir -p build

clean:
	rm -rf build/mychmod