UNITY  = unity
CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -I$(UNITY) -Isrc
BUILD  = build

all: static dynamic

# 6.1: статическая библиотека
static: $(BUILD)/phonebook_static

$(BUILD)/Contacts_static.o: src/Contacts.c | $(BUILD)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/libcontacts.a: $(BUILD)/Contacts_static.o
	ar rcs $@ $^

$(BUILD)/phonebook_static: src/main.c $(BUILD)/libcontacts.a
	gcc $(CFLAGS) src/main.c $(BUILD)/libcontacts.a -o $@

# 6.2: динамическая библиотека
dynamic: $(BUILD)/phonebook_dynamic

$(BUILD)/libcontacts.so: src/Contacts.c | $(BUILD)
	gcc $(CFLAGS) -fPIC -shared $< -o $@

$(BUILD)/phonebook_dynamic: src/main.c $(BUILD)/libcontacts.so
	gcc $(CFLAGS) src/main.c -L$(BUILD) -lcontacts -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all static dynamic clean