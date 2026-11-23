#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include "unity.h"
#include "common.h"

void setUp(void) { g_stop = 0; }
void tearDown(void) {}

static void test_format_send_with_payload(void)
{
    struct msg m;
    int n = format_cmd(&m, CMD_SEND, 42, "news", "hello, world");

    TEST_ASSERT_EQUAL_INT((int)strlen("send,42,news\nhello, world"), n);
    TEST_ASSERT_EQUAL_STRING("send,42,news\nhello, world", m.mtext);
    TEST_ASSERT_EQUAL_INT64(BROKER_TYPE, m.mtype);
}

static void test_format_subscribe_without_payload(void)
{
    struct msg m;

    TEST_ASSERT_GREATER_THAN_INT(0, format_cmd(&m, CMD_SUBSCRIBE, 7, "t", NULL));
    TEST_ASSERT_EQUAL_STRING("subscribe,7,t", m.mtext);
}

static void test_format_rejects_bad_topic(void)
{
    struct msg m;

    TEST_ASSERT_EQUAL_INT(-1, format_cmd(&m, CMD_SEND, 1, "", "x"));
    TEST_ASSERT_EQUAL_INT(-1, format_cmd(&m, CMD_SEND, 1, "a,b", "x"));
    TEST_ASSERT_EQUAL_INT(-1, format_cmd(&m, CMD_SEND, 1, "a\nb", "x"));
}

static void test_format_rejects_too_long(void)
{
    struct msg m;
    char big[MSG_TEXT_MAX + 1];

    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(-1, format_cmd(&m, CMD_SEND, 1, "t", big));
}

static void test_parse_roundtrip(void)
{
    struct msg m;
    struct cmd c;

    format_cmd(&m, CMD_SEND, 123, "sport", "a,b,c\nline2");
    TEST_ASSERT_EQUAL_INT(0, parse_cmd(m.mtext, &c));
    TEST_ASSERT_EQUAL_INT(CMD_SEND, c.kind);
    TEST_ASSERT_EQUAL_INT(123, c.pid);
    TEST_ASSERT_EQUAL_STRING("sport", c.topic);
    TEST_ASSERT_EQUAL_STRING("a,b,c\nline2", c.payload);
}

static void test_parse_unsubscribe_no_payload(void)
{
    struct cmd c;

    TEST_ASSERT_EQUAL_INT(0, parse_cmd("unsubscribe,5,x", &c));
    TEST_ASSERT_EQUAL_INT(CMD_UNSUBSCRIBE, c.kind);
    TEST_ASSERT_EQUAL_STRING("", c.payload);
}

static void test_parse_rejects_malformed(void)
{
    struct cmd c;

    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("", &c));
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send", &c));
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,1", &c));
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("hello,1,t", &c));       /* неизвестная команда */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,abc,t", &c));      /* pid не число */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,0,t", &c));        /* pid должен быть > 0 */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,-3,t", &c));
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,1,", &c));         /* пустая тема */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,1,a,b", &c));      /* запятая в теме */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send\n,1,t", &c));      /* перевод строки внутри команды */
    TEST_ASSERT_EQUAL_INT(-1, parse_cmd("send,\n1,t", &c));      /* вторая запятая уже в payload */
}

static void test_send_cmd_goes_to_broker_type(void)
{
    int q = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    struct msg m;
    ssize_t n;

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, q);
    TEST_ASSERT_EQUAL_INT(0, send_cmd(q, CMD_SUBSCRIBE, "news", NULL));
    n = msgrcv(q, &m, sizeof m.mtext - 1, BROKER_TYPE, IPC_NOWAIT);
    TEST_ASSERT_GREATER_THAN_INT(0, (int)n);
    m.mtext[n] = '\0';
    msgctl(q, IPC_RMID, NULL);

    TEST_ASSERT_EQUAL_STRING_LEN("subscribe,", m.mtext, 10);
    TEST_ASSERT_NOT_NULL(strstr(m.mtext, ",news"));
}

static void test_send_cmd_bad_topic_is_emsgsize(void)
{
    int q = msgget(IPC_PRIVATE, IPC_CREAT | 0600);

    TEST_ASSERT_EQUAL_INT(-1, send_cmd(q, CMD_SEND, "a,b", "x"));
    TEST_ASSERT_EQUAL_INT(EMSGSIZE, errno);
    msgctl(q, IPC_RMID, NULL);
}

static void test_send_cmd_on_removed_queue(void)
{
    int q = msgget(IPC_PRIVATE, IPC_CREAT | 0600);

    msgctl(q, IPC_RMID, NULL);
    TEST_ASSERT_EQUAL_INT(-1, send_cmd(q, CMD_SEND, "t", "x"));
    TEST_ASSERT_TRUE(queue_gone(errno));
}

static void test_queue_gone(void)
{
    TEST_ASSERT_TRUE(queue_gone(EIDRM));
    TEST_ASSERT_TRUE(queue_gone(EINVAL));
    TEST_ASSERT_FALSE(queue_gone(EINTR));
    TEST_ASSERT_FALSE(queue_gone(EAGAIN));
}

static void test_stop_handler_sets_flag(void)
{
    TEST_ASSERT_EQUAL_INT(0, install_stop_handlers());
    TEST_ASSERT_EQUAL_INT(0, g_stop);
    raise(SIGINT);
    TEST_ASSERT_EQUAL_INT(1, g_stop);
    g_stop = 0;
    raise(SIGTERM);
    TEST_ASSERT_EQUAL_INT(1, g_stop);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_format_send_with_payload);
    RUN_TEST(test_format_subscribe_without_payload);
    RUN_TEST(test_format_rejects_bad_topic);
    RUN_TEST(test_format_rejects_too_long);
    RUN_TEST(test_parse_roundtrip);
    RUN_TEST(test_parse_unsubscribe_no_payload);
    RUN_TEST(test_parse_rejects_malformed);
    RUN_TEST(test_send_cmd_goes_to_broker_type);
    RUN_TEST(test_send_cmd_bad_topic_is_emsgsize);
    RUN_TEST(test_send_cmd_on_removed_queue);
    RUN_TEST(test_queue_gone);
    RUN_TEST(test_stop_handler_sets_flag);
    return UNITY_END();
}
