CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -Isrc
BUILD  = build
OPS    = $(BUILD)/ops
SRC_OPS = $(wildcard src/ops/*.c)
LIBS    = $(patsubst src/ops/%.c,$(OPS)/lib%.so,$(SRC_OPS))

all: $(BUILD)/calculator $(LIBS)

$(BUILD)/calculator: src/main.c src/calculator.c src/loader.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

# one library per function: src/ops/NAME.c -> build/ops/libNAME.so
$(OPS)/lib%.so: src/ops/%.c | $(OPS)
	gcc $(CFLAGS) -fPIC -shared $< -o $@

$(BUILD) $(OPS):
	mkdir -p $@

run: all
	./$(BUILD)/calculator $(OPS)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
