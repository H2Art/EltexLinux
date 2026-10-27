UNITY  = unity
CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -I$(UNITY) -Isrc
BUILD  = build

all: $(BUILD)/phonebook

$(BUILD)/phonebook: src/main.c src/Contacts.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

$(BUILD)/test_contacts: tests/test_contacts.c src/Contacts.c $(UNITY)/unity.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

test: $(BUILD)/test_contacts
	./$(BUILD)/test_contacts

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all test clean