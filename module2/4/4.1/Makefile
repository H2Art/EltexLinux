UNITY  = unity
CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -I$(UNITY) -Isrc
BUILD  = build

all: $(BUILD)/phonebook

$(BUILD)/phonebook: src/main.c src/Contacts.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all testmain clean