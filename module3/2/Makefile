CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
UNITY   = ../../third_party/unity
BUILD   = build
TARGET  = $(BUILD)/pubsub
OBJS    = $(BUILD)/main.o $(BUILD)/broker.o $(BUILD)/subscriber.o $(BUILD)/publisher.o \
          $(BUILD)/registry.o $(BUILD)/common.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c common.h registry.h broker.h subscriber.h publisher.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_common: tests/test_common.c common.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@

$(BUILD)/test_registry: tests/test_registry.c registry.c common.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_common $(BUILD)/test_registry
	./$(BUILD)/test_common
	./$(BUILD)/test_registry

test: $(TARGET) unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
