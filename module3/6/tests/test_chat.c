#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "chat.h"

#define TEST_PORT 50997     /* не порт по умолчанию, чтобы не мешать настоящему чату */
#define LOOP_BCAST "127.255.255.255"

static struct chat a, b;

void setUp(void)
{
    a.sock = b.sock = -1;
}

void tearDown(void)
{
    chat_close(&a);
    chat_close(&b);
}

static void test_name_valid(void)
{
    TEST_ASSERT_TRUE(chat_name_valid("alice"));
    TEST_ASSERT_FALSE(chat_name_valid(""));
    TEST_ASSERT_FALSE(chat_name_valid("two words"));
    TEST_ASSERT_FALSE(chat_name_valid("0123456789012345678901234567890123"));
}

static void test_format_and_parse_round_trip(void)
{
    char buf[CHAT_DGRAM_MAX];
    struct chat_msg m;
    int n;

    n = chat_format(buf, sizeof buf, MSG_TEXT, 0x1a2b3c4d, "bob", "hello, all of you");
    TEST_ASSERT_EQUAL_STRING("M 1a2b3c4d bob hello, all of you", buf);
    TEST_ASSERT_EQUAL_INT(0, chat_parse(buf, n, &m));
    TEST_ASSERT_EQUAL_CHAR(MSG_TEXT, m.type);
    TEST_ASSERT_EQUAL_HEX32(0x1a2b3c4d, m.id);
    TEST_ASSERT_EQUAL_STRING("bob", m.name);
    TEST_ASSERT_EQUAL_STRING("hello, all of you", m.text);   /* пробелы в тексте не режут его */

    n = chat_format(buf, sizeof buf, MSG_JOIN, 7, "bob", "");
    TEST_ASSERT_EQUAL_STRING("J 00000007 bob", buf);
    TEST_ASSERT_EQUAL_INT(0, chat_parse(buf, n, &m));
    TEST_ASSERT_EQUAL_STRING("", m.text);
}

static void test_format_rejects_bad_input(void)
{
    char buf[CHAT_DGRAM_MAX], big[CHAT_TEXT_MAX + 1];

    memset(big, 'x', CHAT_TEXT_MAX);
    big[CHAT_TEXT_MAX] = '\0';
    TEST_ASSERT_EQUAL_INT(-1, chat_format(buf, sizeof buf, MSG_TEXT, 1, "two words", "x"));
    TEST_ASSERT_EQUAL_INT(-1, chat_format(buf, sizeof buf, MSG_TEXT, 1, "bob", big));
}

/* На порт может прийти что угодно: всё чужое и битое отбрасывается */
static void test_parse_rejects_garbage(void)
{
    struct chat_msg m;
    const char *bad[] = {
        "", "hello", "X 1a2b3c4d bob",       /* неизвестный тип */
        "M1a2b3c4d bob hi",                   /* нет пробела после типа */
        "M 1a2b3c4 bob hi",                   /* id короче 8 цифр */
        "M zzzzzzzz bob hi",                  /* id не шестнадцатеричный */
        "M 1a2b3c4d ",                        /* нет имени */
        "M 1a2b3c4d 012345678901234567890123456789012 hi",   /* имя длиннее CHAT_NAME_MAX - 1 */
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE(-1, chat_parse(bad[i], strlen(bad[i]), &m), bad[i]);
}

static void test_open_rejects_bad_address(void)
{
    TEST_ASSERT_EQUAL_INT(-1, chat_open(&a, "bob", "300.1.1.1", TEST_PORT));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

/* recv с таймаутом, чтобы потерянная датаграмма не повесила тест */
static int recv_wait(struct chat *c, struct chat_msg *m)
{
    struct pollfd p = { .fd = c->sock, .events = POLLIN };
    struct sockaddr_in src;

    if (poll(&p, 1, 1000) != 1)
        return -2;
    return chat_recv(c, m, &src);
}

/* Два клиента на одном порту: оба получают broadcast, отправитель узнаёт своё эхо по id */
static void test_broadcast_reaches_others_and_echo_is_skipped(void)
{
    struct chat_msg m;

    TEST_ASSERT_EQUAL_INT(0, chat_open(&a, "alice", LOOP_BCAST, TEST_PORT));
    TEST_ASSERT_EQUAL_INT(0, chat_open(&b, "bob", LOOP_BCAST, TEST_PORT));
    TEST_ASSERT_NOT_EQUAL(a.id, b.id);
    TEST_ASSERT_EQUAL_INT(0, chat_send(&a, MSG_TEXT, "hi bob"));
    TEST_ASSERT_EQUAL_INT(1, recv_wait(&b, &m));
    TEST_ASSERT_EQUAL_STRING("alice", m.name);
    TEST_ASSERT_EQUAL_STRING("hi bob", m.text);
    TEST_ASSERT_EQUAL_INT(0, recv_wait(&a, &m));      /* своё эхо пришло, но пропущено */
}

/* Совпавший id при разных именах не прячет собеседника */
static void test_same_id_different_name_is_not_echo(void)
{
    struct chat_msg m;

    TEST_ASSERT_EQUAL_INT(0, chat_open(&a, "alice", LOOP_BCAST, TEST_PORT));
    TEST_ASSERT_EQUAL_INT(0, chat_open(&b, "bob", LOOP_BCAST, TEST_PORT));
    b.id = a.id;
    TEST_ASSERT_EQUAL_INT(0, chat_send(&b, MSG_TEXT, "can you see me"));
    TEST_ASSERT_EQUAL_INT(1, recv_wait(&a, &m));
    TEST_ASSERT_EQUAL_STRING("bob", m.name);
    TEST_ASSERT_EQUAL_INT(0, recv_wait(&b, &m));      /* своё эхо bob по-прежнему пропущено */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_name_valid);
    RUN_TEST(test_format_and_parse_round_trip);
    RUN_TEST(test_format_rejects_bad_input);
    RUN_TEST(test_parse_rejects_garbage);
    RUN_TEST(test_open_rejects_bad_address);
    RUN_TEST(test_broadcast_reaches_others_and_echo_is_skipped);
    RUN_TEST(test_same_id_different_name_is_not_echo);
    return UNITY_END();
}
