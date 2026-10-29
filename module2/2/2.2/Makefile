CFLAGS = -Wall -Wextra -g -fsanitize=address

all: build/calculator

build/calculator: build src/main.c src/calculator.c src/calculator.h
	gcc $(CFLAGS) -o $@ src/main.c src/calculator.c


build: 
	mkdir -p build

clean:
	rm -rf build/calculator