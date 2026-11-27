CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
UNITY   = ../../third_party/unity
BUILD   = build
TARGETS = $(BUILD)/server $(BUILD)/client

all: $(TARGETS)

$(BUILD)/%: $(BUILD)/%.o $(BUILD)/proto.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: %.c proto.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_proto: tests/test_proto.c proto.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_proto
	./$(BUILD)/test_proto

test: $(TARGETS) unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
.SECONDARY:     # не удалять промежуточные .o после сборки по шаблону
