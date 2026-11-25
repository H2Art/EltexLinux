CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
LDLIBS  = -pthread -lrt   # sem_* из libpthread, shm_open из librt (в glibc ≥ 2.34 уже в libc)
UNITY   = ../../third_party/unity
BUILD   = build
TARGETS = $(BUILD)/producer $(BUILD)/consumer

all: $(TARGETS)

$(BUILD)/%: $(BUILD)/%.o $(BUILD)/pc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c pc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_pc: tests/test_pc.c pc.c $(UNITY)/unity.c | $(BUILD)
	$(CC) $(TFLAGS) -I$(UNITY) -I. $^ -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $@

unit: $(BUILD)/test_pc
	./$(BUILD)/test_pc

test: $(TARGETS) unit
	./tests/test.sh

clean:
	rm -rf $(BUILD)

.PHONY: all unit test clean
.SECONDARY:     # не удалять промежуточные .o после сборки по шаблону
