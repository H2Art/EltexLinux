CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
BUILD   = build
TARGET  = $(BUILD)/filecopy
OBJS    = $(BUILD)/main.o $(BUILD)/copy.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c copy.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

test: $(TARGET)
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all test clean