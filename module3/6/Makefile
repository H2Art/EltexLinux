CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
UNITY   = ../../third_party/unity
BUILD   = build
TARGET  = $(BUILD)/chat
OBJS    = $(BUILD)/main.o $(BUILD)/chat.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: %.c chat.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_chat: tests/test_chat.c chat.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_chat
	./$(BUILD)/test_chat

test: $(TARGET) unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
