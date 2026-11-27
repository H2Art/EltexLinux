CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
UNITY   = ../../third_party/unity
BUILD   = build
SRC     = main.c sniff.c

all: $(BUILD)/sniffer

$(BUILD)/sniffer: $(SRC) sniff.h | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@

$(BUILD)/test_sniff: tests/test_sniff.c sniff.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_sniff
	./$(BUILD)/test_sniff

test: $(BUILD)/sniffer unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
