#include <string.h>
#include "unity.h"
#include "Contacts.h"

static Contact c;
static ContactList list;

void setUp(void) {
    memset(&c, 0, sizeof c);
    memset(&list, 0, sizeof list);
}

void tearDown(void) {}

/* helper: контакт с заданным именем */
static Contact makeContact(const char *first, const char *last) {
    Contact t = {0};
    initFullName(&t, first, NULL, last);
    return t;
}

/* ---------- initFullName ---------- */

void test_initFullName_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", "Sergeevich", "Petrov"));
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Sergeevich", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Petrov", c.name.lastName);
}

void test_initFullName_nullMiddle_becomesEmpty(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", NULL, "Petrov"));
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
}

void test_initFullName_nullContact(void) {
    TEST_ASSERT_EQUAL_INT(-1, initFullName(NULL, "Ivan", NULL, "Petrov"));
}

void test_initFullName_emptyOrNullFirstName(void) {
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, "Petrov"));
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, NULL, NULL, "Petrov"));
}

void test_initFullName_emptyOrNullLastName(void) {
    TEST_ASSERT_EQUAL_INT(-3, initFullName(&c, "Ivan", NULL, ""));
    TEST_ASSERT_EQUAL_INT(-3, initFullName(&c, "Ivan", NULL, NULL));
}

void test_initFullName_error_keepsOldName(void) {
    initFullName(&c, "Ivan", NULL, "Petrov");
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, "Sidorov"));
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Petrov", c.name.lastName);
}

void test_initFullName_truncatesLongString(void) {
    char longStr[101];
    memset(longStr, 'a', 100);
    longStr[100] = '\0';
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, longStr, NULL, "Petrov"));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.name.firstName));
}

/* ---------- initWorkInformation ---------- */

void test_initWorkInformation_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&c, "Developer", "Eltex"));
    TEST_ASSERT_EQUAL_STRING("Developer", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_initWorkInformation_nullsBecomeEmpty(void) {
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&c, NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.company);
}

void test_initWorkInformation_overwrites(void) {
    initWorkInformation(&c, "A", "B");
    initWorkInformation(&c, "C", "D");
    TEST_ASSERT_EQUAL_STRING("C", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("D", c.workInfo.company);
}

/* ---------- addItems ---------- */

void test_addItems_phones(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(2, &c, ITEM_PHONE, "111", "222"));
    TEST_ASSERT_EQUAL_INT(2, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("222", c.phones[1]);
}

void test_addItems_emails(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_EMAIL, "a@b.ru"));
    TEST_ASSERT_EQUAL_INT(1, c.emailCount);
    TEST_ASSERT_EQUAL_STRING("a@b.ru", c.emails[0]);
}

void test_addItems_links(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_LINK, "t.me/x"));
    TEST_ASSERT_EQUAL_INT(1, c.linkCount);
    TEST_ASSERT_EQUAL_STRING("t.me/x", c.links[0]);
}

void test_addItems_appendsToExisting(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(2, &c, ITEM_PHONE, "222", "333");
    TEST_ASSERT_EQUAL_INT(3, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("333", c.phones[2]);
}

void test_addItems_typesAreIndependent(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(1, &c, ITEM_EMAIL, "a@b.ru");
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_INT(1, c.emailCount);
    TEST_ASSERT_EQUAL_INT(0, c.linkCount);
}

void test_addItems_exactlyMaxItems(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(10, &c, ITEM_PHONE,
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"));
    TEST_ASSERT_EQUAL_INT(MAX_ITEMS, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("10", c.phones[9]);
}

void test_addItems_overLimit_returnsMinus3_andKeepsState(void) {
    for (int i = 0; i < MAX_ITEMS; i++)
        TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_PHONE, "123"));
    TEST_ASSERT_EQUAL_INT(-3, addItems(1, &c, ITEM_PHONE, "999"));
    TEST_ASSERT_EQUAL_INT(MAX_ITEMS, c.phoneCount);
}

void test_addItems_batchOverLimit_addsNothing(void) {
    addItems(9, &c, ITEM_PHONE, "1", "2", "3", "4", "5", "6", "7", "8", "9");
    TEST_ASSERT_EQUAL_INT(-3, addItems(2, &c, ITEM_PHONE, "a", "b"));
    TEST_ASSERT_EQUAL_INT(9, c.phoneCount);
}

void test_addItems_zeroOrNegativeCount(void) {
    TEST_ASSERT_EQUAL_INT(-2, addItems(0, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(-2, addItems(-1, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(0, c.phoneCount);
}

void test_addItems_wrongType(void) {
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, ITEM_NAME, "x"));
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, ITEM_WORKINFO, "x"));
}

void test_addItems_truncatesLongString(void) {
    char longStr[101];
    memset(longStr, 'x', 100);
    longStr[100] = '\0';
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_EMAIL, longStr));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.emails[0]));
}

/* ---------- addContact ---------- */

void test_addContact_ok(void) {
    Contact a = makeContact("A", "AA");
    TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(1, list.count);
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[0].name.firstName);
}

void test_addContact_keepsOrder(void) {
    Contact a = makeContact("A", "AA"), b = makeContact("B", "BB");
    addContact(&list, &a);
    addContact(&list, &b);
    TEST_ASSERT_EQUAL_STRING("B", list.contacts[1].name.firstName);
}

void test_addContact_copiesByValue(void) {
    Contact a = makeContact("A", "AA");
    addContact(&list, &a);
    strcpy(a.name.firstName, "Changed");
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[0].name.firstName);
}

void test_addContact_nulls(void) {
    Contact a = makeContact("A", "AA");
    TEST_ASSERT_EQUAL_INT(-1, addContact(NULL, &a));
    TEST_ASSERT_EQUAL_INT(-1, addContact(&list, NULL));
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_addContact_fullList(void) {
    Contact a = makeContact("A", "AA");
    for (int i = 0; i < MAX_CONTACTS; i++)
        TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(-2, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS, list.count);
}

/* ---------- deleteContactAt ---------- */

static void fillThree(void) {
    Contact a = makeContact("A", "A"), b = makeContact("B", "B"), d = makeContact("D", "D");
    addContact(&list, &a);
    addContact(&list, &b);
    addContact(&list, &d);
}

void test_deleteContactAt_middle(void) {
    fillThree();
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 1));
    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[0].name.firstName);
    TEST_ASSERT_EQUAL_STRING("D", list.contacts[1].name.firstName);
}

void test_deleteContactAt_first(void) {
    fillThree();
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_STRING("B", list.contacts[0].name.firstName);
}

void test_deleteContactAt_last(void) {
    fillThree();
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 2));
    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_STRING("B", list.contacts[1].name.firstName);
}

void test_deleteContactAt_single(void) {
    Contact a = makeContact("A", "A");
    addContact(&list, &a);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_deleteContactAt_clearsFreedSlot(void) {
    fillThree();
    deleteContactAt(&list, 0);
    Contact zero = {0};
    TEST_ASSERT_EQUAL_MEMORY(&zero, &list.contacts[2], sizeof(Contact));
}

void test_deleteContactAt_badIndex(void) {
    fillThree();
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, -1));
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, 3));
    TEST_ASSERT_EQUAL_INT(3, list.count);
}

void test_deleteContactAt_emptyList(void) {
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, 0));
}

void test_deleteContactAt_null(void) {
    TEST_ASSERT_EQUAL_INT(-1, deleteContactAt(NULL, 0));
}

void test_deleteContactAt_fullList_first(void) {
    Contact a = makeContact("A", "A");
    for (int i = 0; i < MAX_CONTACTS; i++) addContact(&list, &a);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS - 1, list.count);
}

/* ---------- editContact ---------- */

void test_editContact_nullContact(void) {
    TEST_ASSERT_EQUAL_INT(-1, editContact(NULL, ITEM_NAME, "A", "B", "C"));
}

void test_editContact_name_ok(void) {
    initFullName(&c, "Old", NULL, "Name");
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_NAME, "New", "Mid", "Last"));
    TEST_ASSERT_EQUAL_STRING("New", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Mid", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Last", c.name.lastName);
}

void test_editContact_name_invalid_keepsOld(void) {
    initFullName(&c, "Old", NULL, "Name");
    TEST_ASSERT_EQUAL_INT(-2, editContact(&c, ITEM_NAME, "", "Mid", "Last"));
    TEST_ASSERT_EQUAL_STRING("Old", c.name.firstName);
}

void test_editContact_workinfo_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_WORKINFO, "Dev", "Eltex"));
    TEST_ASSERT_EQUAL_STRING("Dev", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_editContact_phones_replacesAll(void) {
    addItems(3, &c, ITEM_PHONE, "1", "2", "3");
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_PHONE, 2, "10", "20"));
    TEST_ASSERT_EQUAL_INT(2, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("10", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("20", c.phones[1]);
    TEST_ASSERT_EQUAL_STRING("", c.phones[2]);   /* старое затёрто */
}

void test_editContact_emails_doesNotTouchOthers(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(1, &c, ITEM_LINK, "t.me/x");
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_EMAIL, 1, "a@b.ru"));
    TEST_ASSERT_EQUAL_INT(1, c.emailCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("t.me/x", c.links[0]);
}

void test_editContact_links_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_LINK, 2, "l1", "l2"));
    TEST_ASSERT_EQUAL_INT(2, c.linkCount);
    TEST_ASSERT_EQUAL_STRING("l2", c.links[1]);
}

void test_editContact_list_wrongCount_keepsOld(void) {
    addItems(3, &c, ITEM_PHONE, "1", "2", "3");
    TEST_ASSERT_EQUAL_INT(-3, editContact(&c, ITEM_PHONE, 0));
    TEST_ASSERT_EQUAL_INT(-3, editContact(&c, ITEM_PHONE, -1));
    TEST_ASSERT_EQUAL_INT(-3, editContact(&c, ITEM_PHONE, MAX_ITEMS + 1));
    TEST_ASSERT_EQUAL_INT(3, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("1", c.phones[0]);
}

void test_editContact_unknownType(void) {
    TEST_ASSERT_EQUAL_INT(-2, editContact(&c, (ItemType)99));
}

/* ---------- runner ---------- */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_initFullName_ok);
    RUN_TEST(test_initFullName_nullMiddle_becomesEmpty);
    RUN_TEST(test_initFullName_nullContact);
    RUN_TEST(test_initFullName_emptyOrNullFirstName);
    RUN_TEST(test_initFullName_emptyOrNullLastName);
    RUN_TEST(test_initFullName_error_keepsOldName);
    RUN_TEST(test_initFullName_truncatesLongString);

    RUN_TEST(test_initWorkInformation_ok);
    RUN_TEST(test_initWorkInformation_nullsBecomeEmpty);
    RUN_TEST(test_initWorkInformation_overwrites);

    RUN_TEST(test_addItems_phones);
    RUN_TEST(test_addItems_emails);
    RUN_TEST(test_addItems_links);
    RUN_TEST(test_addItems_appendsToExisting);
    RUN_TEST(test_addItems_typesAreIndependent);
    RUN_TEST(test_addItems_exactlyMaxItems);
    RUN_TEST(test_addItems_overLimit_returnsMinus3_andKeepsState);
    RUN_TEST(test_addItems_batchOverLimit_addsNothing);
    RUN_TEST(test_addItems_zeroOrNegativeCount);
    RUN_TEST(test_addItems_wrongType);
    RUN_TEST(test_addItems_truncatesLongString);

    RUN_TEST(test_addContact_ok);
    RUN_TEST(test_addContact_keepsOrder);
    RUN_TEST(test_addContact_copiesByValue);
    RUN_TEST(test_addContact_nulls);
    RUN_TEST(test_addContact_fullList);

    RUN_TEST(test_deleteContactAt_middle);
    RUN_TEST(test_deleteContactAt_first);
    RUN_TEST(test_deleteContactAt_last);
    RUN_TEST(test_deleteContactAt_single);
    RUN_TEST(test_deleteContactAt_clearsFreedSlot);
    RUN_TEST(test_deleteContactAt_badIndex);
    RUN_TEST(test_deleteContactAt_emptyList);
    RUN_TEST(test_deleteContactAt_null);
    RUN_TEST(test_deleteContactAt_fullList_first);

    RUN_TEST(test_editContact_nullContact);
    RUN_TEST(test_editContact_name_ok);
    RUN_TEST(test_editContact_name_invalid_keepsOld);
    RUN_TEST(test_editContact_workinfo_ok);
    RUN_TEST(test_editContact_phones_replacesAll);
    RUN_TEST(test_editContact_emails_doesNotTouchOthers);
    RUN_TEST(test_editContact_links_ok);
    RUN_TEST(test_editContact_list_wrongCount_keepsOld);
    RUN_TEST(test_editContact_unknownType);

    return UNITY_END();
}