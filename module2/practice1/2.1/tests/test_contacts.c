#include "unity.h"
#include "Contacts.h"

static ContactList list;
static Contact c;

void setUp(void) {              // calling before each test
    list = (ContactList){0};
    c = (Contact){0};
}

void tearDown(void) {}          // calling after each test

void test_initFullName_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", NULL, "Petrov"));
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
}

void test_initFullName_emptyFirstName(void) {
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, "Petrov"));
}

void test_addItems_limit(void) {
    for (int i = 0; i < MAX_ITEMS; i++)
        TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_PHONE, "123"));
    TEST_ASSERT_EQUAL_INT(-3, addItems(1, &c, ITEM_PHONE, "123"));
}

void test_deleteContactAt_middle(void) {
    Contact a = {0}, b = {0}, d = {0};
    initFullName(&a, "A", NULL, "A");
    initFullName(&b, "B", NULL, "B");
    initFullName(&d, "D", NULL, "D");
    addContact(&list, &a); addContact(&list, &b); addContact(&list, &d);

    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 1));
    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_STRING("D", list.contacts[1].name.firstName);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initFullName_ok);
    RUN_TEST(test_initFullName_emptyFirstName);
    RUN_TEST(test_addItems_limit);
    RUN_TEST(test_deleteContactAt_middle);
    return UNITY_END();
}