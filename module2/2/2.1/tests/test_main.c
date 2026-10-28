#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include "unity.h"
#include "main.h"

static AppState st;

void setUp(void) {
    memset(&st, 0, sizeof st);
    st.currentIndex = -1;
    st.running = 1;
}
void tearDown(void) {}

/* ---------- helpers ---------- */

static FILE *in_from(const char *s) {
    return fmemopen((void *)s, strlen(s), "r");
}

static Contact makeContact(const char *first, const char *last) {
    Contact t = {0};
    initFullName(&t, first, NULL, last);
    return t;
}

static void fillList(int n) {
    for (int i = 0; i < n; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "N%d", i);
        Contact t = makeContact(nm, "L");
        TEST_ASSERT_EQUAL_INT(0, addContact(&st.contactList, &t));
    }
}

/* =====================================================
 *                    readChoice
 * ===================================================== */

void test_readChoice_basic(void) {
    FILE *in = in_from("3\n");
    TEST_ASSERT_EQUAL_INT(3, readChoice(in));
    fclose(in);
}

void test_readChoice_nine(void) {
    FILE *in = in_from("9\n");
    TEST_ASSERT_EQUAL_INT(9, readChoice(in));
    fclose(in);
}

void test_readChoice_ten(void) {
    FILE *in = in_from("10\n");
    TEST_ASSERT_EQUAL_INT(10, readChoice(in));
    fclose(in);
}

void test_readChoice_zero(void) {
    FILE *in = in_from("0\n");
    TEST_ASSERT_EQUAL_INT(0, readChoice(in));
    fclose(in);
}

void test_readChoice_newline_only(void) {
    FILE *in = in_from("\n");
    TEST_ASSERT_EQUAL_INT(0, readChoice(in));
    fclose(in);
}

void test_readChoice_garbage(void) {
    FILE *in = in_from("abc\n");
    TEST_ASSERT_EQUAL_INT(0, readChoice(in));
    fclose(in);
}

void test_readChoice_leading_spaces(void) {
    FILE *in = in_from("   3\n");
    TEST_ASSERT_EQUAL_INT(3, readChoice(in));
    fclose(in);
}

void test_readChoice_no_newline(void) {
    FILE *in = in_from("7");
    TEST_ASSERT_EQUAL_INT(7, readChoice(in));
    fclose(in);
}

void test_readChoice_eof(void) {
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(0, readChoice(in));
    fclose(in);
}

void test_readChoice_long_input_consumes_tail(void) {
    FILE *in = in_from("12345678901234567890\n5\n");
    readChoice(in);
    TEST_ASSERT_EQUAL_INT(5, readChoice(in)); 
    fclose(in);
}

/* =====================================================
 *              checkContactAtIndex
 * ===================================================== */

void test_checkIndex_null_state(void) {
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_NULL_ARG, checkContactAtIndex(NULL, 0));
}

void test_checkIndex_negative(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, -1));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, INT_MIN));
}

void test_checkIndex_at_count(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, 3));
}

void test_checkIndex_beyond_count(void) {
    fillList(2);
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, 5));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, MAX_CONTACTS - 1));
}

void test_checkIndex_valid(void) {
    fillList(3);
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, checkContactAtIndex(&st, 0));
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, checkContactAtIndex(&st, 2));
}

void test_checkIndex_empty_list(void) {
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, checkContactAtIndex(&st, 0));
}

/* =====================================================
 *              processMainChoice — create
 * ===================================================== */

void test_main_create_resets_and_invites(void) {
    st.currentContact = makeContact("Old", "Name");
    FILE *in = in_from("Ivan\n\nPetrov\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_CREATE_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("Ivan", st.currentContact.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Petrov", st.currentContact.name.lastName);
}

/* =====================================================
 *              processMainChoice — add
 * ===================================================== */

void test_main_add_ok(void) {
    st.currentContact = makeContact("Ivan", "Petrov");
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_ADD_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.contactList.count);
    TEST_ASSERT_EQUAL_STRING("Ivan", st.contactList.contacts[0].name.firstName);
}

void test_main_add_empty_name_fails(void) {
    st.currentContact = (Contact){0};
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_EMPTY_FIRST_NAME,
                          processMainChoice(&st, UO_ADD_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(0, st.contactList.count);
}

void test_main_add_when_full(void) {
    fillList(MAX_CONTACTS);
    st.currentContact = makeContact("Ivan", "Petrov");
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_LIST_FULL,
                          processMainChoice(&st, UO_ADD_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(MAX_CONTACTS, st.contactList.count);
}

/* =====================================================
 *              processMainChoice — delete
 * ===================================================== */

void test_main_delete_no_selection(void) {
    fillList(3);
    st.currentIndex = -1;
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX,
                          processMainChoice(&st, UO_DELETE_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(3, st.contactList.count);
}

void test_main_delete_valid(void) {
    fillList(3);
    st.currentIndex = 1;
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK,
                          processMainChoice(&st, UO_DELETE_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(2, st.contactList.count);
    TEST_ASSERT_EQUAL_STRING("N0", st.contactList.contacts[0].name.firstName);
    TEST_ASSERT_EQUAL_STRING("N2", st.contactList.contacts[1].name.firstName);
}

void test_main_delete_index_out_of_range(void) {
    fillList(2);
    st.currentIndex = 5;
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX,
                          processMainChoice(&st, UO_DELETE_CURR_CONTACT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(2, st.contactList.count);
}

/* =====================================================
 *              processMainChoice — print
 * ===================================================== */

void test_main_print_no_selection(void) {
    fillList(2);
    st.currentIndex = -1;
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX,
                          processMainChoice(&st, UO_PRINT_CURR_CONTACT, in));
    fclose(in);
}

void test_main_print_valid(void) {
    st.currentContact = makeContact("Ivan", "Petrov");
    st.contactList.contacts[0] = st.currentContact;
    st.contactList.count = 1;
    st.currentIndex = 0;
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK,
                          processMainChoice(&st, UO_PRINT_CURR_CONTACT, in));
    fclose(in);
}

/* =====================================================
 *              processMainChoice — choose
 * ===================================================== */

void test_main_choose_valid(void) {
    fillList(3);
    FILE *in = in_from("1\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK,
                          processMainChoice(&st, UO_CHOOSE_CONTACT_BY_IND, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.currentIndex);
}

void test_main_choose_zero(void) {
    fillList(3);
    FILE *in = in_from("0\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK,
                          processMainChoice(&st, UO_CHOOSE_CONTACT_BY_IND, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(0, st.currentIndex);
}

void test_main_choose_out_of_range(void) {
    fillList(2);
    FILE *in = in_from("5\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX,
                          processMainChoice(&st, UO_CHOOSE_CONTACT_BY_IND, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(-1, st.currentIndex);
}

/* =====================================================
 *              processMainChoice — exit / unknown
 * ===================================================== */

void test_main_exit_stops_running(void) {
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_EXIT, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(0, st.running);
}

void test_main_unknown_choice(void) {
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processMainChoice(&st, 0, in));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processMainChoice(&st, 7, in));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processMainChoice(&st, 99, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.running);
}

/* =====================================================
 *              processEditChoice
 * ===================================================== */

void test_edit_choice_phone(void) {
    FILE *in = in_from("2\n123\n456\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 1, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(2, st.currentContact.phoneCount);
    TEST_ASSERT_EQUAL_STRING("123", st.currentContact.phones[0]);
    TEST_ASSERT_EQUAL_STRING("456", st.currentContact.phones[1]);
}

void test_edit_choice_email(void) {
    FILE *in = in_from("1\na@b.ru\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 2, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.currentContact.emailCount);
    TEST_ASSERT_EQUAL_STRING("a@b.ru", st.currentContact.emails[0]);
}

void test_edit_choice_link(void) {
    FILE *in = in_from("1\nt.me/x\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 3, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.currentContact.linkCount);
    TEST_ASSERT_EQUAL_STRING("t.me/x", st.currentContact.links[0]);
}

void test_edit_choice_name(void) {
    FILE *in = in_from("Ivan\n\nPetrov\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 4, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("Ivan", st.currentContact.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Petrov", st.currentContact.name.lastName);
}

void test_edit_choice_workinfo(void) {
    FILE *in = in_from("Dev\nEltex\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 5, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("Dev", st.currentContact.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", st.currentContact.workInfo.company);
}

void test_edit_choice_exit(void) {
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processEditChoice(&st, 6, in));
    fclose(in);
}

void test_edit_choice_unknown(void) {
    FILE *in = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processEditChoice(&st, 0, in));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processEditChoice(&st, 7, in));
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_INDEX, processEditChoice(&st, 99, in));
    fclose(in);
}

/* =====================================================
 *                    itemInvite
 * ===================================================== */

void test_itemInvite_bad_count_zero(void) {
    FILE *in = in_from("0\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
}

void test_itemInvite_bad_count_negative(void) {
    FILE *in = in_from("-1\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
}

void test_itemInvite_bad_count_too_big(void) {
    FILE *in = in_from("11\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_BAD_COUNT, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
}

void test_itemInvite_ok(void) {
    FILE *in = in_from("3\na\nb\nc\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(3, st.currentContact.phoneCount);
    TEST_ASSERT_EQUAL_STRING("a", st.currentContact.phones[0]);
    TEST_ASSERT_EQUAL_STRING("b", st.currentContact.phones[1]);
    TEST_ASSERT_EQUAL_STRING("c", st.currentContact.phones[2]);
}

void test_itemInvite_empty_entry(void) {
    FILE *in = in_from("3\na\n\nc\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_EMPTY_ITEM, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(0, st.currentContact.phoneCount);
}

void test_itemInvite_eof(void) {
    FILE *in = in_from("3\na\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_EMPTY_ITEM, itemInvite(&st, ITEM_PHONE, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(0, st.currentContact.phoneCount);
}

void test_itemInvite_max_items(void) {
    FILE *in = in_from("10\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, itemInvite(&st, ITEM_EMAIL, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(MAX_ITEMS, st.currentContact.emailCount);
    TEST_ASSERT_EQUAL_STRING("10", st.currentContact.emails[9]);
}

void test_itemInvite_replaces_old(void) {
    addItems(2, &st.currentContact, ITEM_LINK, "old1", "old2");
    FILE *in = in_from("1\nnew\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, itemInvite(&st, ITEM_LINK, in));
    fclose(in);
    TEST_ASSERT_EQUAL_INT(1, st.currentContact.linkCount);
    TEST_ASSERT_EQUAL_STRING("new", st.currentContact.links[0]);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.links[1]);
}

/* =====================================================
 *                    fullNameInvite
 * ===================================================== */

void test_fullName_ok(void) {
    FILE *in = in_from("Ivan\nSergeevich\nPetrov\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, fullNameInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("Ivan", st.currentContact.name.firstName);
    TEST_ASSERT_EQUAL_STRING("Sergeevich", st.currentContact.name.middleName);
    TEST_ASSERT_EQUAL_STRING("Petrov", st.currentContact.name.lastName);
}

void test_fullName_empty_middle(void) {
    FILE *in = in_from("Ivan\n\nPetrov\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, fullNameInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.name.middleName);
}

void test_fullName_empty_first_fails(void) {
    FILE *in = in_from("\n\nPetrov\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_EMPTY_FIRST_NAME, fullNameInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.name.firstName);
}

void test_fullName_empty_last_fails(void) {
    FILE *in = in_from("Ivan\n\n\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_EMPTY_LAST_NAME, fullNameInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.name.lastName);
}

/* =====================================================
 *                    workInfoInvite
 * ===================================================== */

void test_work_ok(void) {
    FILE *in = in_from("Dev\nEltex\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, workInfoInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("Dev", st.currentContact.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("Eltex", st.currentContact.workInfo.company);
}

void test_work_empty_ok(void) {
    FILE *in = in_from("\n\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, workInfoInvite(&st, in));
    fclose(in);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.workInfo.position);
    TEST_ASSERT_EQUAL_STRING("", st.currentContact.workInfo.company);
}

/* =====================================================
 *                    printContact
 * ===================================================== */

void test_print_null(void) {
    TEST_ASSERT_EQUAL_INT(CONTACT_ERR_NULL_ARG, printContact(NULL));
}

void test_print_ok(void) {
    Contact t = makeContact("Ivan", "Petrov");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, printContact(&t));
}

/* =====================================================
 *                    Сценарии
 * ===================================================== */

void test_scenario_create_add_choose_delete(void) {
    st.currentContact = makeContact("Ivan", "Petrov");
    FILE *in1 = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_ADD_CURR_CONTACT, in1));
    fclose(in1);
    TEST_ASSERT_EQUAL_INT(1, st.contactList.count);

    st.currentContact = makeContact("Oleg", "Sidorov");
    FILE *in2 = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_ADD_CURR_CONTACT, in2));
    fclose(in2);
    TEST_ASSERT_EQUAL_INT(2, st.contactList.count);

    FILE *in3 = in_from("1\n");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_CHOOSE_CONTACT_BY_IND, in3));
    fclose(in3);
    TEST_ASSERT_EQUAL_INT(1, st.currentIndex);

    FILE *in4 = in_from("");
    TEST_ASSERT_EQUAL_INT(CONTACT_OK, processMainChoice(&st, UO_DELETE_CURR_CONTACT, in4));
    fclose(in4);
    TEST_ASSERT_EQUAL_INT(1, st.contactList.count);
    TEST_ASSERT_EQUAL_STRING("Ivan", st.contactList.contacts[0].name.firstName);
}

void test_scenario_exit_stops_loop(void) {
    int iterations = 0;
    while (st.running && iterations < 10) {
        FILE *in = in_from("");
        processMainChoice(&st, UO_EXIT, in);
        fclose(in);
        iterations++;
    }
    TEST_ASSERT_EQUAL_INT(1, iterations);
    TEST_ASSERT_EQUAL_INT(0, st.running);
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
    if (pid < 0) {
        UnityDefaultTestRun(f, name, line);
        return;
    }
    if (pid == 0) {
        int before = (int)Unity.TestFailures;
        UnityDefaultTestRun(f, name, line);
        fflush(stdout);
        exit((int)Unity.TestFailures > before ? 1 : 0);
    }
    int st_status = 0;
    waitpid(pid, &st_status, 0);
    if (WIFEXITED(st_status)) {
        Unity.NumberOfTests++;
        if (WEXITSTATUS(st_status) != 0) Unity.TestFailures++;
    } else {
        printf("[signal %d in %s]\n", WTERMSIG(st_status), name);
        UnityDefaultTestRun(crash_stub, name, line);
    }
}

#define RUN(f) run_isolated(f, #f, __LINE__)

int main(void) {
    UNITY_BEGIN();

    /* readChoice */
    RUN(test_readChoice_basic);
    RUN(test_readChoice_nine);
    RUN(test_readChoice_ten);
    RUN(test_readChoice_zero);
    RUN(test_readChoice_newline_only);
    RUN(test_readChoice_garbage);
    RUN(test_readChoice_leading_spaces);
    RUN(test_readChoice_no_newline);
    RUN(test_readChoice_eof);
    RUN(test_readChoice_long_input_consumes_tail);

    /* checkContactAtIndex */
    RUN(test_checkIndex_null_state);
    RUN(test_checkIndex_negative);
    RUN(test_checkIndex_at_count);
    RUN(test_checkIndex_beyond_count);
    RUN(test_checkIndex_valid);
    RUN(test_checkIndex_empty_list);

    /* processMainChoice — create/add */
    RUN(test_main_create_resets_and_invites);
    RUN(test_main_add_ok);
    RUN(test_main_add_empty_name_fails);
    RUN(test_main_add_when_full);

    /* processMainChoice — delete */
    RUN(test_main_delete_no_selection);
    RUN(test_main_delete_valid);
    RUN(test_main_delete_index_out_of_range);

    /* processMainChoice — print */
    RUN(test_main_print_no_selection);
    RUN(test_main_print_valid);

    /* processMainChoice — choose */
    RUN(test_main_choose_valid);
    RUN(test_main_choose_zero);
    RUN(test_main_choose_out_of_range);

    /* processMainChoice — exit/unknown */
    RUN(test_main_exit_stops_running);
    RUN(test_main_unknown_choice);

    /* processEditChoice */
    RUN(test_edit_choice_phone);
    RUN(test_edit_choice_email);
    RUN(test_edit_choice_link);
    RUN(test_edit_choice_name);
    RUN(test_edit_choice_workinfo);
    RUN(test_edit_choice_exit);
    RUN(test_edit_choice_unknown);

    /* itemInvite */
    RUN(test_itemInvite_bad_count_zero);
    RUN(test_itemInvite_bad_count_negative);
    RUN(test_itemInvite_bad_count_too_big);
    RUN(test_itemInvite_ok);
    RUN(test_itemInvite_empty_entry);
    RUN(test_itemInvite_eof);
    RUN(test_itemInvite_max_items);
    RUN(test_itemInvite_replaces_old);

    /* fullNameInvite */
    RUN(test_fullName_ok);
    RUN(test_fullName_empty_middle);
    RUN(test_fullName_empty_first_fails);
    RUN(test_fullName_empty_last_fails);

    /* workInfoInvite */
    RUN(test_work_ok);
    RUN(test_work_empty_ok);

    /* printContact */
    RUN(test_print_null);
    RUN(test_print_ok);

    /* сценарии */
    RUN(test_scenario_create_add_choose_delete);
    RUN(test_scenario_exit_stops_loop);

    return UNITY_END();
}