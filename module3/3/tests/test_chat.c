#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "chat.h"

static struct chat a, b;

/* Уникальное имя на процесс, чтобы тест не задел чужие очереди */
static void names_for(struct chat *c)
{
    char base[64];

    snprintf(base, sizeof base, "test_chat_%d", (int)getpid());
    chat_names(base, c->name1, c->name2, sizeof c->name1);
}

void setUp(void)
{
    names_for(&a);
    names_for(&b);
    a.in = a.out = b.in = b.out = (mqd_t)-1;
}

void tearDown(void)
{
    mq_unlink(a.name1);
    mq_unlink(a.name2);
}

static void test_names_ok(void)
{
    char n1[CHAT_NAME_SIZE], n2[CHAT_NAME_SIZE];

    TEST_ASSERT_EQUAL_INT(0, chat_names("talk", n1, n2, sizeof n1));
    TEST_ASSERT_EQUAL_STRING("/talk_1", n1);
    TEST_ASSERT_EQUAL_STRING("/talk_2", n2);
    TEST_ASSERT_EQUAL_INT(0, chat_names("/talk", n1, n2, sizeof n1));   /* ведущий слэш допустим */
    TEST_ASSERT_EQUAL_STRING("/talk_1", n1);
}

static void test_names_rejected(void)
{
    char n1[CHAT_NAME_SIZE], n2[CHAT_NAME_SIZE], big[300];

    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(-1, chat_names("", n1, n2, sizeof n1));
    TEST_ASSERT_EQUAL_INT(-1, chat_names("/", n1, n2, sizeof n1));
    TEST_ASSERT_EQUAL_INT(-1, chat_names("a/b", n1, n2, sizeof n1));
    TEST_ASSERT_EQUAL_INT(-1, chat_names(big, n1, n2, sizeof n1));
}

static void test_first_creates_second_joins(void)
{
    TEST_ASSERT_EQUAL_INT(0, chat_open(&a));
    TEST_ASSERT_EQUAL_INT(1, a.creator);
    TEST_ASSERT_EQUAL_INT(0, chat_open(&b));
    TEST_ASSERT_EQUAL_INT(0, b.creator);
    chat_close(&b);
    chat_close(&a);
}

/* Направления должны быть встречными: что пишет один, читает другой */
static void test_directions_are_crossed(void)
{
    char buf[CHAT_MSG_SIZE];
    unsigned prio;

    chat_open(&a);
    chat_open(&b);

    TEST_ASSERT_EQUAL_INT(0, mq_send(a.out, "ping", 4, PRIO_TEXT));
    TEST_ASSERT_EQUAL_INT(4, (int)mq_receive(b.in, buf, sizeof buf, &prio));
    TEST_ASSERT_EQUAL_MEMORY("ping", buf, 4);

    TEST_ASSERT_EQUAL_INT(0, mq_send(b.out, "pong", 4, PRIO_TEXT));
    TEST_ASSERT_EQUAL_INT(4, (int)mq_receive(a.in, buf, sizeof buf, &prio));
    TEST_ASSERT_EQUAL_MEMORY("pong", buf, 4);

    chat_close(&b);
    chat_close(&a);
}

/* "Ухожу" с низким приоритетом не обгоняет обычные сообщения */
static void test_bye_comes_after_text(void)
{
    char buf[CHAT_MSG_SIZE];
    unsigned prio;

    chat_open(&a);
    chat_open(&b);
    mq_send(a.out, "1", 1, PRIO_TEXT);
    mq_send(a.out, "2", 1, PRIO_TEXT);
    mq_send(a.out, "", 0, PRIO_BYE);

    mq_receive(b.in, buf, sizeof buf, &prio);
    TEST_ASSERT_EQUAL_UINT(PRIO_TEXT, prio);
    TEST_ASSERT_EQUAL_CHAR('1', buf[0]);
    mq_receive(b.in, buf, sizeof buf, &prio);
    TEST_ASSERT_EQUAL_CHAR('2', buf[0]);
    TEST_ASSERT_EQUAL_INT(0, (int)mq_receive(b.in, buf, sizeof buf, &prio));
    TEST_ASSERT_EQUAL_UINT(PRIO_BYE, prio);

    chat_close(&b);
    chat_close(&a);
}

static void test_only_creator_unlinks(void)
{
    mqd_t q;

    chat_open(&a);
    chat_open(&b);

    chat_close(&b);   /* второй уходит: очереди остаются */
    q = mq_open(a.name1, O_RDONLY);
    TEST_ASSERT_NOT_EQUAL_INT(-1, (int)q);
    mq_close(q);

    chat_close(&a);   /* создатель уходит: имён больше нет */
    TEST_ASSERT_EQUAL_INT(-1, (int)mq_open(a.name1, O_RDONLY));
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
    TEST_ASSERT_EQUAL_INT(-1, (int)mq_open(a.name2, O_RDONLY));
}

static void test_joiner_creates_second_queue_if_missing(void)
{
    struct mq_attr attr = { .mq_maxmsg = CHAT_MAX_MSGS, .mq_msgsize = CHAT_MSG_SIZE };
    mqd_t q1;

    /* как будто создатель успел сделать только первую очередь */
    q1 = mq_open(a.name1, O_CREAT | O_EXCL | O_RDONLY, 0600, &attr);
    TEST_ASSERT_NOT_EQUAL_INT(-1, (int)q1);

    TEST_ASSERT_EQUAL_INT(0, chat_open(&b));
    TEST_ASSERT_EQUAL_INT(0, b.creator);
    chat_close(&b);
    mq_close(q1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_names_ok);
    RUN_TEST(test_names_rejected);
    RUN_TEST(test_first_creates_second_joins);
    RUN_TEST(test_directions_are_crossed);
    RUN_TEST(test_bye_comes_after_text);
    RUN_TEST(test_only_creator_unlinks);
    RUN_TEST(test_joiner_creates_second_queue_if_missing);
    return UNITY_END();
}
