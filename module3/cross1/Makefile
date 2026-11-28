CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
UNITY   = ../../third_party/unity
BUILD   = build
TARGETS = $(BUILD)/server $(BUILD)/client

all: $(TARGETS)

$(BUILD)/server: server.c rawudp.c table.c rawudp.h table.h | $(BUILD)
	$(CC) $(CFLAGS) server.c rawudp.c table.c -o $@

$(BUILD)/client: client.c rawudp.c rawudp.h | $(BUILD)
	$(CC) $(CFLAGS) client.c rawudp.c -o $@

$(BUILD)/test_unit: tests/test_unit.c rawudp.c table.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_unit
	./$(BUILD)/test_unit

test: $(TARGETS) unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
