CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -Isrc
BUILD  = build
OPS    = $(BUILD)/ops
LIBS   = $(OPS)/libadd.so $(OPS)/libsubtract.so $(OPS)/libmultiply.so $(OPS)/libdivide.so

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
