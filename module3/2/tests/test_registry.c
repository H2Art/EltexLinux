#include <stdlib.h>
#include "unity.h"
#include "registry.h"

static struct registry r;

void setUp(void) { reg_init(&r); }
void tearDown(void) { reg_free(&r); }

static void test_add_sub_and_duplicate(void)
{
    TEST_ASSERT_EQUAL_INT(0, reg_add_sub(&r, 10, "news"));
    TEST_ASSERT_EQUAL_INT(1, reg_add_sub(&r, 10, "news"));
    TEST_ASSERT_EQUAL_UINT(1, r.nsubs);
}

static void test_one_pid_many_topics(void)
{
    TEST_ASSERT_EQUAL_INT(0, reg_add_sub(&r, 10, "a"));
    TEST_ASSERT_EQUAL_INT(0, reg_add_sub(&r, 10, "b"));
    TEST_ASSERT_EQUAL_UINT(2, r.nsubs);
}

static void test_del_sub(void)
{
    reg_add_sub(&r, 10, "a");
    reg_add_sub(&r, 11, "a");
    reg_add_sub(&r, 10, "b");

    TEST_ASSERT_EQUAL_INT(1, reg_del_sub(&r, 10, "a"));
    TEST_ASSERT_EQUAL_INT(0, reg_del_sub(&r, 10, "a"));   /* второй раз уже нечего удалять */
    TEST_ASSERT_EQUAL_INT(0, reg_del_sub(&r, 99, "a"));   /* чужого pid нет */
    TEST_ASSERT_EQUAL_UINT(2, r.nsubs);
    /* остались именно (11,a) и (10,b), а не что-то другое */
    TEST_ASSERT_EQUAL_INT(1, reg_add_sub(&r, 11, "a"));
    TEST_ASSERT_EQUAL_INT(1, reg_add_sub(&r, 10, "b"));
}

static void test_add_pub_and_duplicate(void)
{
    TEST_ASSERT_EQUAL_INT(0, reg_add_pub(&r, 5));
    TEST_ASSERT_EQUAL_INT(1, reg_add_pub(&r, 5));
    TEST_ASSERT_EQUAL_UINT(1, r.npubs);
}

static void test_growth_beyond_initial_capacity(void)
{
    int i;

    for (i = 1; i <= 100; i++) {
        TEST_ASSERT_EQUAL_INT(0, reg_add_sub(&r, i, "t"));
        TEST_ASSERT_EQUAL_INT(0, reg_add_pub(&r, 1000 + i));
    }
    TEST_ASSERT_EQUAL_UINT(100, r.nsubs);
    TEST_ASSERT_EQUAL_UINT(100, r.npubs);
}

static void test_all_pids_deduplicates(void)
{
    pid_t *p;
    size_t n;

    reg_add_sub(&r, 10, "a");
    reg_add_sub(&r, 10, "b");   /* тот же pid, другая тема */
    reg_add_sub(&r, 11, "a");
    reg_add_pub(&r, 11);        /* и подписчик, и издатель */
    reg_add_pub(&r, 12);

    n = reg_all_pids(&r, &p);
    TEST_ASSERT_EQUAL_UINT(3, n);
    TEST_ASSERT_EQUAL_INT(10, p[0]);
    TEST_ASSERT_EQUAL_INT(11, p[1]);
    TEST_ASSERT_EQUAL_INT(12, p[2]);
    free(p);
}

static void test_all_pids_empty(void)
{
    pid_t *p = (pid_t *)1;

    TEST_ASSERT_EQUAL_UINT(0, reg_all_pids(&r, &p));
    TEST_ASSERT_NULL(p);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_sub_and_duplicate);
    RUN_TEST(test_one_pid_many_topics);
    RUN_TEST(test_del_sub);
    RUN_TEST(test_add_pub_and_duplicate);
    RUN_TEST(test_growth_beyond_initial_capacity);
    RUN_TEST(test_all_pids_deduplicates);
    RUN_TEST(test_all_pids_empty);
    return UNITY_END();
}
