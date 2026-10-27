#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include "unity.h"
#include "Contacts.h"

/*
 * Each test launches in isolated fork, so segfault
 * or abort in one test mark ONLY it as fallen, others go on
 */

static Contact c;
static ContactList list;

static const ItemType LIST_TYPES[3] = { ITEM_PHONE, ITEM_EMAIL, ITEM_LINK };

void setUp(void) {
    memset(&c, 0, sizeof c);
    memset(&list, 0, sizeof list);
}
void tearDown(void) {}

/* ---------- helpers ---------- */

static Contact makeContact(const char *first, const char *last) {
    Contact t = {0};
    initFullName(&t, first, NULL, last);
    return t;
}

static int *countOf(Contact *t, ItemType ty) {
    switch (ty) {
    case ITEM_PHONE: return &t->phoneCount;
    case ITEM_EMAIL: return &t->emailCount;
    case ITEM_LINK:  return &t->linkCount;
    default:         return NULL;
    }
}

static char *itemAt(Contact *t, ItemType ty, int i) {
    switch (ty) {
    case ITEM_PHONE: return t->phones[i];
    case ITEM_EMAIL: return t->emails[i];
    case ITEM_LINK:  return t->links[i];
    default:         return NULL;
    }
}

static void makeStr(char *buf, size_t len, char ch) {
    memset(buf, ch, len);
    buf[len] = '\0';
}

static void assertZero(const void *p, size_t n) {
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++)
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, b[i], "memory is not zeroed");
}

static void assertNoNullText(Contact *t, ItemType ty) {
    int cnt = *countOf(t, ty);
    for (int i = 0; i < cnt; i++)
        TEST_ASSERT_NOT_EQUAL(0, strcmp(itemAt(t, ty, i), "(null)"));
}

static void fillList(int n) {
    for (int i = 0; i < n; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "N%d", i);
        Contact t = makeContact(nm, "L");
        TEST_ASSERT_EQUAL_INT(0, addContact(&list, &t));
    }
}

static void assertListNames(int n, const int *ids) {
    TEST_ASSERT_EQUAL_INT(n, list.count);
    for (int i = 0; i < n; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "N%d", ids[i]);
        TEST_ASSERT_EQUAL_STRING(nm, list.contacts[i].name.firstName);
    }
}

static int editList(Contact *c, ItemType ty, int n, const char **items) {
    char buf[MAX_ITEMS][MAX_LENGTH];
    for (int i = 0; i < n; i++) {
        snprintf(buf[i], MAX_LENGTH, "%s", items[i] ? items[i] : "");
    }
    return editContact(c, ty, n, buf);
}

/* =====================================================
 *                    initFullName
 * ===================================================== */

void test_initFullName_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", "Sergeevich", "Petrov"));
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Sergeevich", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Petrov", c.name.lastName);
}

void test_initFullName_nullMiddle_becomesEmpty(void) {
    memset(c.name.middleName, 'x', 10);
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", NULL, "Petrov"));
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
}

void test_initFullName_emptyMiddle_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Ivan", "", "Petrov"));
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
}

void test_initFullName_nullContact(void) {
    TEST_ASSERT_EQUAL_INT(-1, initFullName(NULL, "Ivan", NULL, "Petrov"));
}

void test_initFullName_emptyFirstName(void) {
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, "Petrov"));
}

void test_initFullName_nullFirstName(void) {
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, NULL, NULL, "Petrov"));
}

void test_initFullName_emptyLastName(void) {
    TEST_ASSERT_EQUAL_INT(-3, initFullName(&c, "Ivan", NULL, ""));
}

void test_initFullName_nullLastName(void) {
    TEST_ASSERT_EQUAL_INT(-3, initFullName(&c, "Ivan", NULL, NULL));
}

void test_initFullName_bothEmpty_firstErrorWins(void) {
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, ""));
}

void test_initFullName_error_keepsOldName(void) {
    initFullName(&c, "Ivan", "M", "Petrov");
    TEST_ASSERT_EQUAL_INT(-2, initFullName(&c, "", NULL, "Sidorov"));
    TEST_ASSERT_EQUAL_INT(-3, initFullName(&c, "Oleg", NULL, ""));
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("M", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Petrov", c.name.lastName);
}

void test_initFullName_overwrites(void) {
    initFullName(&c, "Ivan", "M", "Petrov");
    initFullName(&c, "Oleg", NULL, "Sidorov");
    TEST_ASSERT_EQUAL_STRING("Oleg", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Sidorov", c.name.lastName);
}

void test_initFullName_truncatesEachField(void) {
    char a[101], b[101], d[101];
    makeStr(a, 100, 'a');
    makeStr(b, 100, 'b');
    makeStr(d, 100, 'd');
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, a, b, d));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.name.firstName));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.name.middleName));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.name.lastName));
    TEST_ASSERT_EQUAL_CHAR('a', c.name.firstName[MAX_LENGTH - 2]);
    TEST_ASSERT_EQUAL_CHAR('b', c.name.middleName[0]);
    TEST_ASSERT_EQUAL_CHAR('d', c.name.lastName[0]);
}

void test_initFullName_exactly49_preserved(void) {
    char s[MAX_LENGTH];
    makeStr(s, MAX_LENGTH - 1, 'q');
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, s, NULL, s));
    TEST_ASSERT_EQUAL_STRING(s, c.name.firstName);
    TEST_ASSERT_EQUAL_STRING(s, c.name.lastName);
}

void test_initFullName_doesNotTouchOtherFields(void) {
    initWorkInformation(&c, "Dev", "Eltex");
    addItems(1, &c, ITEM_PHONE, "111");
    initFullName(&c, "Ivan", NULL, "Petrov");
    TEST_ASSERT_EQUAL_STRING("Dev", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
}

void test_initFullName_utf8_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Иван", "Сергеевич", "Петров"));
    TEST_ASSERT_EQUAL_STRING("Иван", c.name.firstName);
}

void test_initFullName_spaceInside_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initFullName(&c, "Anna Maria", NULL, "von Trapp"));
    TEST_ASSERT_EQUAL_STRING("Anna Maria", c.name.firstName);
}

void test_initFullName_whitespaceOnlyFirstName_rejected(void) {
    TEST_ASSERT_TRUE(initFullName(&c, "   ", NULL, "Petrov") < 0);
}

void test_initFullName_whitespaceOnlyLastName_rejected(void) {
    TEST_ASSERT_TRUE(initFullName(&c, "Ivan", NULL, "   ") < 0);
}

/* =====================================================
 *                 initWorkInformation
 * ===================================================== */

void test_initWork_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&c, "Developer", "Eltex"));
    TEST_ASSERT_EQUAL_STRING("Developer", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_initWork_nullsBecomeEmpty(void) {
    initWorkInformation(&c, "A", "B");
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&c, NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.company);
}

void test_initWork_onlyPosition(void) {
    initWorkInformation(&c, "Dev", NULL);
    TEST_ASSERT_EQUAL_STRING("Dev", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.company);
}

void test_initWork_onlyCompany(void) {
    initWorkInformation(&c, NULL, "Eltex");
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_initWork_overwrites(void) {
    initWorkInformation(&c, "A", "B");
    initWorkInformation(&c, "C", "D");
    TEST_ASSERT_EQUAL_STRING("C", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("D", c.workInfo.company);
}

void test_initWork_truncates_andNoOverflow(void) {
    char a[101], b[101];
    makeStr(a, 100, 'p');
    makeStr(b, 100, 'c');
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&c, a, b));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.workInfo.position));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.workInfo.company));
    TEST_ASSERT_EQUAL_CHAR('c', c.workInfo.company[0]);
}

void test_initWork_doesNotTouchName(void) {
    initFullName(&c, "Ivan", NULL, "Petrov");
    initWorkInformation(&c, "Dev", "Eltex");
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Petrov", c.name.lastName);
}

void test_initWork_nullContact(void) {
    TEST_ASSERT_TRUE(initWorkInformation(NULL, "A", "B") < 0);
}

/* =====================================================
 *                      addItems
 * ===================================================== */

void test_addItems_phones(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(2, &c, ITEM_PHONE, "111", "222"));
    TEST_ASSERT_EQUAL_INT(2, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("222", c.phones[1]);
}

void test_addItems_emails(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(2, &c, ITEM_EMAIL, "a@b.ru", "c@d.ru"));
    TEST_ASSERT_EQUAL_INT(2, c.emailCount);
    TEST_ASSERT_EQUAL_STRING("c@d.ru", c.emails[1]);
}

void test_addItems_links(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(3, &c, ITEM_LINK, "l1", "l2", "l3"));
    TEST_ASSERT_EQUAL_INT(3, c.linkCount);
    TEST_ASSERT_EQUAL_STRING("l3", c.links[2]);
}

void test_addItems_appendsToExisting(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(2, &c, ITEM_PHONE, "222", "333");
    TEST_ASSERT_EQUAL_INT(3, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("222", c.phones[1]);
    TEST_ASSERT_EQUAL_STRING("333", c.phones[2]);
}

void test_addItems_typesAreIndependent(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(2, &c, ITEM_EMAIL, "a@b.ru", "c@d.ru");
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_INT(2, c.emailCount);
    TEST_ASSERT_EQUAL_INT(0, c.linkCount);
    assertZero(c.links, sizeof c.links);
}

void test_addItems_doesNotTouchNameAndWork(void) {
    initFullName(&c, "Ivan", NULL, "Petrov");
    initWorkInformation(&c, "Dev", "Eltex");
    addItems(1, &c, ITEM_LINK, "x");
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_addItems_exactlyMaxItems_oneBatch(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(10, &c, ITEM_PHONE,
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"));
    TEST_ASSERT_EQUAL_INT(MAX_ITEMS, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("1", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("10", c.phones[9]);
}

void test_addItems_limit_allTypes_oneByOne(void) {
    for (int k = 0; k < 3; k++) {
        ItemType ty = LIST_TYPES[k];
        memset(&c, 0, sizeof c);
        for (int i = 0; i < MAX_ITEMS; i++)
            TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ty, "123"));
        TEST_ASSERT_EQUAL_INT(-6, addItems(1, &c, ty, "999"));
        TEST_ASSERT_EQUAL_INT(MAX_ITEMS, *countOf(&c, ty));
        TEST_ASSERT_EQUAL_STRING("123", itemAt(&c, ty, MAX_ITEMS - 1));
    }
}

void test_addItems_batchOverLimit_addsNothing(void) {
    addItems(9, &c, ITEM_PHONE, "1", "2", "3", "4", "5", "6", "7", "8", "9");
    TEST_ASSERT_EQUAL_INT(-6, addItems(2, &c, ITEM_PHONE, "a", "b"));
    TEST_ASSERT_EQUAL_INT(9, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("", c.phones[9]);
}

void test_addItems_batchLargerThanMax(void) {
    TEST_ASSERT_EQUAL_INT(-6, addItems(MAX_ITEMS + 1, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(0, c.phoneCount);
}

void test_addItems_zeroCount(void) {
    TEST_ASSERT_EQUAL_INT(-6, addItems(0, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(0, c.phoneCount);
}

void test_addItems_negativeCount(void) {
    TEST_ASSERT_EQUAL_INT(-6, addItems(-5, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(0, c.phoneCount);
}

void test_addItems_hugeCount_noOverflow(void) {
    TEST_ASSERT_EQUAL_INT(-6, addItems(INT_MAX, &c, ITEM_PHONE));
    TEST_ASSERT_EQUAL_INT(0, c.phoneCount);
}

void test_addItems_wrongType(void) {
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, ITEM_NAME, "x"));
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, ITEM_WORKINFO, "x"));
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, (ItemType)99, "x"));
    TEST_ASSERT_EQUAL_INT(-1, addItems(1, &c, (ItemType)-1, "x"));
}

void test_addItems_truncates_allTypes_noOverflowIntoNext(void) {
    char longStr[2 * MAX_LENGTH + 1];
    makeStr(longStr, 2 * MAX_LENGTH, 'z');
    for (int k = 0; k < 3; k++) {
        ItemType ty = LIST_TYPES[k];
        memset(&c, 0, sizeof c);
        TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ty, longStr));
        TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ty, "next"));
        TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(itemAt(&c, ty, 0)));
        TEST_ASSERT_EQUAL_STRING("next", itemAt(&c, ty, 1));
        TEST_ASSERT_EQUAL_INT(2, *countOf(&c, ty));
    }
}

void test_addItems_exactly49_preserved(void) {
    char s[MAX_LENGTH];
    makeStr(s, MAX_LENGTH - 1, 'q');
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_EMAIL, s));
    TEST_ASSERT_EQUAL_STRING(s, c.emails[0]);
}

void test_addItems_lastSlotLongString_noOverflowIntoNextArray(void) {
    char longStr[2 * MAX_LENGTH + 1];
    makeStr(longStr, 2 * MAX_LENGTH, 'z');
    for (int i = 0; i < MAX_ITEMS; i++) addItems(1, &c, ITEM_PHONE, longStr);
    assertZero(c.emails, sizeof c.emails);
    TEST_ASSERT_EQUAL_INT(0, c.emailCount);
}

void test_addItems_duplicatesAllowed(void) {
    addItems(3, &c, ITEM_PHONE, "111", "111", "111");
    TEST_ASSERT_EQUAL_INT(3, c.phoneCount);
}

void test_addItems_utf8(void) {
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &c, ITEM_LINK, "https://пример.рф"));
    TEST_ASSERT_EQUAL_STRING("https://пример.рф", c.links[0]);
}

void test_addItems_nullContact(void) {
    TEST_ASSERT_TRUE(addItems(1, NULL, ITEM_PHONE, "1") < 0);
}

void test_addItems_nullEntry_noCrash_noNullText(void) {
    addItems(3, &c, ITEM_PHONE, "1", NULL, "3");
    assertNoNullText(&c, ITEM_PHONE);
}

/* =====================================================
 *                     addContact
 * ===================================================== */

void test_addContact_ok(void) {
    Contact a = makeContact("A", "AA");
    TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(1, list.count);
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[0].name.firstName);
}

void test_addContact_keepsOrder(void) {
    fillList(5);
    int ids[] = {0, 1, 2, 3, 4};
    assertListNames(5, ids);
}

void test_addContact_copiesByValue(void) {
    Contact a = makeContact("A", "AA");
    addContact(&list, &a);
    strcpy(a.name.firstName, "Changed");
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[0].name.firstName);
}

void test_addContact_copiesAllFields(void) {
    Contact a = makeContact("A", "AA");
    initWorkInformation(&a, "Dev", "Eltex");
    addItems(2, &a, ITEM_PHONE, "1", "2");
    addItems(1, &a, ITEM_EMAIL, "a@b.ru");
    addItems(1, &a, ITEM_LINK, "t.me/a");
    addContact(&list, &a);
    TEST_ASSERT_EQUAL_MEMORY(&a, &list.contacts[0], sizeof(Contact));
}

void test_addContact_nullList(void) {
    Contact a = makeContact("A", "AA");
    TEST_ASSERT_EQUAL_INT(-1, addContact(NULL, &a));
}

void test_addContact_nullContact(void) {
    TEST_ASSERT_EQUAL_INT(-1, addContact(&list, NULL));
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_addContact_fullList(void) {
    Contact a = makeContact("A", "AA");
    for (int i = 0; i < MAX_CONTACTS; i++)
        TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(-4, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS, list.count);
}

void test_addContact_lastSlotUsable(void) {
    fillList(MAX_CONTACTS - 1);
    Contact a = makeContact("Last", "L");
    TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_STRING("Last", list.contacts[MAX_CONTACTS - 1].name.firstName);
}

void test_addContact_sameContactTwice_independentCopies(void) {
    Contact a = makeContact("A", "AA");
    addContact(&list, &a);
    addContact(&list, &a);
    strcpy(list.contacts[0].name.firstName, "X");
    TEST_ASSERT_EQUAL_STRING("A", list.contacts[1].name.firstName);
}

void test_addContact_emptyFirstName_rejected(void) {
    Contact a = makeContact("A", "AA");
    a.name.firstName[0] = '\0';
    TEST_ASSERT_TRUE(addContact(&list, &a) < 0);
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_addContact_emptyLastName_rejected(void) {
    Contact a = makeContact("A", "AA");
    a.name.lastName[0] = '\0';
    TEST_ASSERT_TRUE(addContact(&list, &a) < 0);
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_addContact_zeroedContact_rejected(void) {
    Contact a = {0};
    TEST_ASSERT_TRUE(addContact(&list, &a) < 0);
}

/* =====================================================
 *                   deleteContactAt
 * ===================================================== */

void test_delete_middle(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 1));
    int ids[] = {0, 2};
    assertListNames(2, ids);
}

void test_delete_first(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    int ids[] = {1, 2};
    assertListNames(2, ids);
}

void test_delete_last(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 2));
    int ids[] = {0, 1};
    assertListNames(2, ids);
}

void test_delete_single(void) {
    fillList(1);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, list.count);
    assertZero(&list.contacts[0], sizeof(Contact));
}

void test_delete_clearsFreedSlot(void) {
    fillList(3);
    deleteContactAt(&list, 0);
    assertZero(&list.contacts[2], sizeof(Contact));
}

void test_delete_clearsFreedSlot_fullList(void) {
    fillList(MAX_CONTACTS);
    deleteContactAt(&list, 10);
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS - 1, list.count);
    assertZero(&list.contacts[MAX_CONTACTS - 1], sizeof(Contact));
}

void test_delete_fullList_last(void) {
    fillList(MAX_CONTACTS);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, MAX_CONTACTS - 1));
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS - 1, list.count);
    TEST_ASSERT_EQUAL_STRING("N98", list.contacts[98].name.firstName);
}

void test_delete_fullList_first_shiftsEverything(void) {
    fillList(MAX_CONTACTS);
    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_STRING("N1", list.contacts[0].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N99", list.contacts[98].name.firstName);
}

void test_delete_negativeIndex(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, -1));
    TEST_ASSERT_EQUAL_INT(3, list.count);
}

void test_delete_indexEqualsCount(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, 3));
    TEST_ASSERT_EQUAL_INT(3, list.count);
}

void test_delete_extremeIndexes(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, INT_MAX));
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, INT_MIN));
    TEST_ASSERT_EQUAL_INT(3, list.count);
}

void test_delete_emptyList(void) {
    TEST_ASSERT_EQUAL_INT(-2, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_delete_nullList(void) {
    TEST_ASSERT_EQUAL_INT(-1, deleteContactAt(NULL, 0));
}

void test_delete_allFromFront(void) {
    fillList(10);
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, list.count);
    assertZero(list.contacts, sizeof list.contacts);
}

void test_delete_allFromBack(void) {
    fillList(10);
    for (int i = 9; i >= 0; i--)
        TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, i));
    TEST_ASSERT_EQUAL_INT(0, list.count);
    assertZero(list.contacts, sizeof list.contacts);
}

void test_delete_twiceSameIndex(void) {
    fillList(4);
    deleteContactAt(&list, 1);
    deleteContactAt(&list, 1);
    int ids[] = {0, 3};
    assertListNames(2, ids);
}

void test_delete_thenAddReusesSlot(void) {
    fillList(3);
    deleteContactAt(&list, 1);
    Contact a = makeContact("New", "N");
    TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));
    TEST_ASSERT_EQUAL_INT(3, list.count);
    TEST_ASSERT_EQUAL_STRING("New", list.contacts[2].name.firstName);
}

void test_delete_keepsFullContactData(void) {
    Contact a = makeContact("A", "A"), b = makeContact("B", "B");
    addItems(2, &b, ITEM_PHONE, "1", "2");
    initWorkInformation(&b, "Dev", "Eltex");
    addContact(&list, &a);
    addContact(&list, &b);
    deleteContactAt(&list, 0);
    TEST_ASSERT_EQUAL_MEMORY(&b, &list.contacts[0], sizeof(Contact));
}

void test_delete_stress_addAll_deleteEveryOther(void) {
    fillList(MAX_CONTACTS);
    for (int i = 0; i < 50; i++)
        TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, i)); 
    TEST_ASSERT_EQUAL_INT(50, list.count);
    TEST_ASSERT_EQUAL_STRING("N1", list.contacts[0].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N3", list.contacts[1].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N99", list.contacts[49].name.firstName);
}

/* =====================================================
 *                     editContact
 * ===================================================== */

void test_edit_nullContact(void) {
    TEST_ASSERT_EQUAL_INT(-1, editContact(NULL, ITEM_NAME, "A", "B", "C"));
}

void test_edit_unknownType(void) {
    TEST_ASSERT_EQUAL_INT(-2, editContact(&c, (ItemType)99));
    TEST_ASSERT_EQUAL_INT(-2, editContact(&c, (ItemType)-1));
}

void test_edit_name_ok(void) {
    initFullName(&c, "Old", NULL, "Name");
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_NAME, "New", "Mid", "Last"));
    TEST_ASSERT_EQUAL_STRING("New", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Mid", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Last", c.name.lastName);
}

void test_edit_name_nullMiddle(void) {
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_NAME, "New", NULL, "Last"));
    TEST_ASSERT_EQUAL_STRING("", c.name.middleName);
}

void test_edit_name_emptyFirst_keepsOld(void) {
    initFullName(&c, "Old", NULL, "Name");
    TEST_ASSERT_EQUAL_INT(-2, editContact(&c, ITEM_NAME, "", "Mid", "Last"));
    TEST_ASSERT_EQUAL_STRING("Old", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Name", c.name.lastName);
}

void test_edit_name_emptyLast_keepsOld(void) {
    initFullName(&c, "Old", NULL, "Name");
    TEST_ASSERT_EQUAL_INT(-3, editContact(&c, ITEM_NAME, "New", "Mid", ""));
    TEST_ASSERT_EQUAL_STRING("Old", c.name.firstName);
}

void test_edit_name_nullLast(void) {
    TEST_ASSERT_EQUAL_INT(-3, editContact(&c, ITEM_NAME, "New", "Mid", NULL));
}

void test_edit_name_argsOrder(void) {
    editContact(&c, ITEM_NAME, "F", "M", "L");
    TEST_ASSERT_EQUAL_STRING("F", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("M", c.name.middleName);
    TEST_ASSERT_EQUAL_STRING("L", c.name.lastName);
}

void test_edit_work_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_WORKINFO, "Dev", "Eltex"));
    TEST_ASSERT_EQUAL_STRING("Dev", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_edit_work_nulls_clearFields(void) {
    initWorkInformation(&c, "A", "B");
    TEST_ASSERT_EQUAL_INT(0, editContact(&c, ITEM_WORKINFO, NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("", c.workInfo.company);
}

void test_edit_work_doesNotTouchName(void) {
    initFullName(&c, "Ivan", NULL, "Petrov");
    editContact(&c, ITEM_WORKINFO, "Dev", "Eltex");
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
}

void test_edit_list_replacesAll_allTypes(void) {
    for (int k = 0; k < 3; k++) {
        ItemType ty = LIST_TYPES[k];
        memset(&c, 0, sizeof c);
        addItems(3, &c, ty, "1", "2", "3");
        const char *items[] = { "10", "20" };
        TEST_ASSERT_EQUAL_INT(0, editList(&c, ty, 2, items));
        TEST_ASSERT_EQUAL_INT(2, *countOf(&c, ty));
        TEST_ASSERT_EQUAL_STRING("10", itemAt(&c, ty, 0));
        TEST_ASSERT_EQUAL_STRING("20", itemAt(&c, ty, 1));
        TEST_ASSERT_EQUAL_STRING("", itemAt(&c, ty, 2));
    }
}

void test_edit_list_fromEmpty(void) {
    const char *items[] = { "555" };
    TEST_ASSERT_EQUAL_INT(0, editList(&c, ITEM_PHONE, 1, items));
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("555", c.phones[0]);
}

void test_edit_list_toMaxItems(void) {
    const char *items[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
    TEST_ASSERT_EQUAL_INT(0, editList(&c, ITEM_EMAIL, 10, items));
    TEST_ASSERT_EQUAL_INT(MAX_ITEMS, c.emailCount);
    TEST_ASSERT_EQUAL_STRING("10", c.emails[9]);
}

void test_edit_list_fullToOne(void) {
    addItems(10, &c, ITEM_PHONE, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10");
    const char *items[] = { "x" };
    TEST_ASSERT_EQUAL_INT(0, editList(&c, ITEM_PHONE, 1, items));
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    for (int i = 1; i < MAX_ITEMS; i++)
        TEST_ASSERT_EQUAL_STRING("", c.phones[i]);
}

void test_edit_list_doesNotTouchOtherLists(void) {
    addItems(1, &c, ITEM_PHONE, "111");
    addItems(1, &c, ITEM_LINK, "t.me/x");
    const char *items[] = { "a@b.ru" };
    TEST_ASSERT_EQUAL_INT(0, editList(&c, ITEM_EMAIL, 1, items));
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("111", c.phones[0]);
    TEST_ASSERT_EQUAL_INT(1, c.linkCount);
    TEST_ASSERT_EQUAL_STRING("t.me/x", c.links[0]);
}

void test_edit_list_doesNotTouchNameAndWork(void) {
    initFullName(&c, "Ivan", NULL, "Petrov");
    initWorkInformation(&c, "Dev", "Eltex");
    const char *items[] = { "1" };
    editList(&c, ITEM_PHONE, 1, items);
    TEST_ASSERT_EQUAL_STRING("Ivan", c.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Eltex", c.workInfo.company);
}

void test_edit_list_badCount_keepsOld(void) {
    addItems(3, &c, ITEM_PHONE, "1", "2", "3");
    char buf[MAX_ITEMS][MAX_LENGTH] = {{0}};
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, editContact(&c, ITEM_PHONE, 0, buf));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, editContact(&c, ITEM_PHONE, -1, buf));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, editContact(&c, ITEM_PHONE, MAX_ITEMS + 1, buf));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, editContact(&c, ITEM_PHONE, INT_MAX, buf));
    TEST_ASSERT_EQUAL_INT(3, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("1", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("3", c.phones[2]);
}

void test_edit_list_truncatesLongString(void) {
    char longStr[2 * MAX_LENGTH + 1];
    makeStr(longStr, 2 * MAX_LENGTH, 'z');
    const char *items[] = { longStr, "next" };
    TEST_ASSERT_EQUAL_INT(0, editList(&c, ITEM_LINK, 2, items));
    TEST_ASSERT_EQUAL_UINT(MAX_LENGTH - 1, strlen(c.links[0]));
    TEST_ASSERT_EQUAL_STRING("next", c.links[1]);
}

void test_edit_list_twiceInRow(void) {
    const char *first[]  = { "1", "2", "3" };
    const char *second[] = { "9" };
    editList(&c, ITEM_PHONE, 3, first);
    editList(&c, ITEM_PHONE, 1, second);
    TEST_ASSERT_EQUAL_INT(1, c.phoneCount);
    TEST_ASSERT_EQUAL_STRING("9", c.phones[0]);
    TEST_ASSERT_EQUAL_STRING("", c.phones[1]);
    TEST_ASSERT_EQUAL_STRING("", c.phones[2]);
}

void test_edit_contactInsideList_persists(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(0, editContact(&list.contacts[1], ITEM_NAME, "Edited", NULL, "E"));
    TEST_ASSERT_EQUAL_STRING("Edited", list.contacts[1].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N0", list.contacts[0].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N2", list.contacts[2].name.firstName);
}

void test_edit_list_nullEntry_noCrash_noNullText(void) {
    const char *items[] = { "1", NULL };
    editList(&c, ITEM_PHONE, 2, items);
    assertNoNullText(&c, ITEM_PHONE);
}

void test_edit_list_nullEntry_keepsOldOrValid(void) {
    addItems(2, &c, ITEM_EMAIL, "old1@x.ru", "old2@x.ru");
    const char *items[] = { "new@x.ru", NULL };
    editList(&c, ITEM_EMAIL, 2, items);
    assertNoNullText(&c, ITEM_EMAIL);
}

/* =====================================================
 *                 Integration Scenarios
 * ===================================================== */

void test_scenario_fullContactLifecycle(void) {
    Contact a = {0};
    TEST_ASSERT_EQUAL_INT(0, initFullName(&a, "Ivan", "S", "Petrov"));
    TEST_ASSERT_EQUAL_INT(0, initWorkInformation(&a, "Dev", "Eltex"));
    TEST_ASSERT_EQUAL_INT(0, addItems(2, &a, ITEM_PHONE, "1", "2"));
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &a, ITEM_EMAIL, "a@b.ru"));
    TEST_ASSERT_EQUAL_INT(0, addItems(1, &a, ITEM_LINK, "t.me/a"));
    TEST_ASSERT_EQUAL_INT(0, addContact(&list, &a));

    Contact *p = &list.contacts[0];
    const char *phones[] = { "999" };
    TEST_ASSERT_EQUAL_INT(0, editList(p, ITEM_PHONE, 1, phones));
    TEST_ASSERT_EQUAL_INT(0, editContact(p, ITEM_NAME, "Oleg", NULL, "Sidorov"));
    TEST_ASSERT_EQUAL_INT(1, p->phoneCount);
    TEST_ASSERT_EQUAL_STRING("999", p->phones[0]);
    TEST_ASSERT_EQUAL_STRING("Oleg", p->name.firstName);
    TEST_ASSERT_EQUAL_STRING("a@b.ru", p->emails[0]);

    TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, list.count);
}

void test_scenario_fillDeleteRefill(void) {
    for (int round = 0; round < 3; round++) {
        fillList(MAX_CONTACTS);
        TEST_ASSERT_EQUAL_INT(MAX_CONTACTS, list.count);
        for (int i = 0; i < MAX_CONTACTS; i++)
            TEST_ASSERT_EQUAL_INT(0, deleteContactAt(&list, 0));
        TEST_ASSERT_EQUAL_INT(0, list.count);
    }
}

/* =====================================================
 *            Running each test isolated
 * ===================================================== */

static void crash_stub(void) {
    TEST_FAIL_MESSAGE("test crashed (segfault/abort) - see signal above");
}

static void run_isolated(UnityTestFunction f, const char *name, int line) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {                       /* fork failed; launching as usual */
        UnityDefaultTestRun(f, name, line);
        return;
    }
    if (pid == 0) {                      /* child */
        int before = (int)Unity.TestFailures;
        UnityDefaultTestRun(f, name, line);
        fflush(stdout);
        exit((int)Unity.TestFailures > before ? 1 : 0);
    }
    int st = 0;                          /* parent */
    waitpid(pid, &st, 0);
    if (WIFEXITED(st)) {
        Unity.NumberOfTests++;
        if (WEXITSTATUS(st) != 0) Unity.TestFailures++;
    } else {
        printf("[signal %d in %s]\n", WTERMSIG(st), name);
        UnityDefaultTestRun(crash_stub, name, line);
    }
}

#define RUN(f) run_isolated(f, #f, __LINE__)

int main(void) {
    UNITY_BEGIN();

    /* initFullName */
    RUN(test_initFullName_ok);
    RUN(test_initFullName_nullMiddle_becomesEmpty);
    RUN(test_initFullName_emptyMiddle_ok);
    RUN(test_initFullName_nullContact);
    RUN(test_initFullName_emptyFirstName);
    RUN(test_initFullName_nullFirstName);
    RUN(test_initFullName_emptyLastName);
    RUN(test_initFullName_nullLastName);
    RUN(test_initFullName_bothEmpty_firstErrorWins);
    RUN(test_initFullName_error_keepsOldName);
    RUN(test_initFullName_overwrites);
    RUN(test_initFullName_truncatesEachField);
    RUN(test_initFullName_exactly49_preserved);
    RUN(test_initFullName_doesNotTouchOtherFields);
    RUN(test_initFullName_utf8_ok);
    RUN(test_initFullName_spaceInside_ok);
    RUN(test_initFullName_whitespaceOnlyFirstName_rejected);
    RUN(test_initFullName_whitespaceOnlyLastName_rejected);

    /* initWorkInformation */
    RUN(test_initWork_ok);
    RUN(test_initWork_nullsBecomeEmpty);
    RUN(test_initWork_onlyPosition);
    RUN(test_initWork_onlyCompany);
    RUN(test_initWork_overwrites);
    RUN(test_initWork_truncates_andNoOverflow);
    RUN(test_initWork_doesNotTouchName);
    RUN(test_initWork_nullContact);

    /* addItems */
    RUN(test_addItems_phones);
    RUN(test_addItems_emails);
    RUN(test_addItems_links);
    RUN(test_addItems_appendsToExisting);
    RUN(test_addItems_typesAreIndependent);
    RUN(test_addItems_doesNotTouchNameAndWork);
    RUN(test_addItems_exactlyMaxItems_oneBatch);
    RUN(test_addItems_limit_allTypes_oneByOne);
    RUN(test_addItems_batchOverLimit_addsNothing);
    RUN(test_addItems_batchLargerThanMax);
    RUN(test_addItems_zeroCount);
    RUN(test_addItems_negativeCount);
    RUN(test_addItems_hugeCount_noOverflow);
    RUN(test_addItems_wrongType);
    RUN(test_addItems_truncates_allTypes_noOverflowIntoNext);
    RUN(test_addItems_exactly49_preserved);
    RUN(test_addItems_lastSlotLongString_noOverflowIntoNextArray);
    RUN(test_addItems_duplicatesAllowed);
    RUN(test_addItems_utf8);
    RUN(test_addItems_nullContact);
    RUN(test_addItems_nullEntry_noCrash_noNullText);

    /* addContact */
    RUN(test_addContact_ok);
    RUN(test_addContact_keepsOrder);
    RUN(test_addContact_copiesByValue);
    RUN(test_addContact_copiesAllFields);
    RUN(test_addContact_nullList);
    RUN(test_addContact_nullContact);
    RUN(test_addContact_fullList);
    RUN(test_addContact_lastSlotUsable);
    RUN(test_addContact_sameContactTwice_independentCopies);
    RUN(test_addContact_emptyFirstName_rejected);
    RUN(test_addContact_emptyLastName_rejected);
    RUN(test_addContact_zeroedContact_rejected);

    /* deleteContactAt */
    RUN(test_delete_middle);
    RUN(test_delete_first);
    RUN(test_delete_last);
    RUN(test_delete_single);
    RUN(test_delete_clearsFreedSlot);
    RUN(test_delete_clearsFreedSlot_fullList);
    RUN(test_delete_fullList_last);
    RUN(test_delete_fullList_first_shiftsEverything);
    RUN(test_delete_negativeIndex);
    RUN(test_delete_indexEqualsCount);
    RUN(test_delete_extremeIndexes);
    RUN(test_delete_emptyList);
    RUN(test_delete_nullList);
    RUN(test_delete_allFromFront);
    RUN(test_delete_allFromBack);
    RUN(test_delete_twiceSameIndex);
    RUN(test_delete_thenAddReusesSlot);
    RUN(test_delete_keepsFullContactData);
    RUN(test_delete_stress_addAll_deleteEveryOther);

    /* editContact */
    RUN(test_edit_nullContact);
    RUN(test_edit_unknownType);
    RUN(test_edit_name_ok);
    RUN(test_edit_name_nullMiddle);
    RUN(test_edit_name_emptyFirst_keepsOld);
    RUN(test_edit_name_emptyLast_keepsOld);
    RUN(test_edit_name_nullLast);
    RUN(test_edit_name_argsOrder);
    RUN(test_edit_work_ok);
    RUN(test_edit_work_nulls_clearFields);
    RUN(test_edit_work_doesNotTouchName);
    RUN(test_edit_list_replacesAll_allTypes);
    RUN(test_edit_list_fromEmpty);
    RUN(test_edit_list_toMaxItems);
    RUN(test_edit_list_fullToOne);
    RUN(test_edit_list_doesNotTouchOtherLists);
    RUN(test_edit_list_doesNotTouchNameAndWork);
    RUN(test_edit_list_badCount_keepsOld);
    RUN(test_edit_list_truncatesLongString);
    RUN(test_edit_list_twiceInRow);
    RUN(test_edit_contactInsideList_persists);
    RUN(test_edit_list_nullEntry_noCrash_noNullText);
    RUN(test_edit_list_nullEntry_keepsOldOrValid);

    /* сценарии */
    RUN(test_scenario_fullContactLifecycle);
    RUN(test_scenario_fillDeleteRefill);

    return UNITY_END();
}