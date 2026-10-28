UNITY  = unity
CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -I$(UNITY) -Isrc
BUILD  = build

all: $(BUILD)/phonebook

$(BUILD)/phonebook: src/main.c src/Contacts.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

$(BUILD)/test_contacts: tests/test_contacts.c src/Contacts.c $(UNITY)/unity.c | $(BUILD)
	gcc $(CFLAGS) $^ -o $@

$(BUILD)/test_main: tests/test_main.c src/main.c src/Contacts.c $(UNITY)/unity.c | $(BUILD)
	gcc $(CFLAGS) -DUNIT_TEST $^ -o $@

test: $(BUILD)/test_contacts
	./$(BUILD)/test_contacts

testmain: $(BUILD)/test_main
	./$(BUILD)/test_main

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all test testmain clean