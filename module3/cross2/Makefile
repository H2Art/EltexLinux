CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
LDLIBS  = -lrt
UNITY   = ../../third_party/unity
BUILD   = build

all: $(BUILD)/taxi

$(BUILD)/taxi: cli.c taxi.c taxi.h | $(BUILD)
	$(CC) $(CFLAGS) cli.c taxi.c -o $@ $(LDLIBS)

$(BUILD)/test_taxi: tests/test_taxi.c taxi.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_taxi
	./$(BUILD)/test_taxi

test: $(BUILD)/taxi unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
