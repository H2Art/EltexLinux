#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "unity.h"
#include "proto.h"

static struct buf b;

void setUp(void) { }
void tearDown(void) { buf_free(&b); }

static void test_frame_round_trip_with_two_parts(void)
{
    char type;
    const char *p;
    size_t len;

    TEST_ASSERT_EQUAL_INT(0, frame_put(&b, T_TEXT, "bob", 4, "hi", 2));   /* "bob\0" + "hi" */
    TEST_ASSERT_EQUAL(HDR_SIZE + 6, b.len);
    TEST_ASSERT_EQUAL_HEX8(0, b.data[1]);              /* длина 6 в сетевом порядке: 00 00 00 06 */
    TEST_ASSERT_EQUAL_HEX8(6, b.data[4]);
    TEST_ASSERT_EQUAL_INT(1, frame_get(&b, &type, &p, &len));
    TEST_ASSERT_EQUAL_CHAR(T_TEXT, type);
    TEST_ASSERT_EQUAL(6, len);
    TEST_ASSERT_EQUAL_MEMORY("bob\0hi", p, 6);
}

/* Поток режет данные как угодно: кадр готов только когда пришёл последний байт */
static void test_partial_frame_waits_for_the_rest(void)
{
    struct buf whole = { 0 };
    char type;
    const char *p;
    size_t len, i;

    frame_put(&whole, T_HELLO, "alice", 5, NULL, 0);
    for (i = 0; i < whole.len; i++) {
        TEST_ASSERT_EQUAL_INT(0, frame_get(&b, &type, &p, &len));
        buf_append(&b, whole.data + i, 1);
    }
    TEST_ASSERT_EQUAL_INT(1, frame_get(&b, &type, &p, &len));
    TEST_ASSERT_EQUAL_MEMORY("alice", p, 5);
    buf_free(&whole);
}

/* И наоборот: два кадра могут прийти одним recv */
static void test_two_frames_in_one_buffer(void)
{
    char type;
    const char *p;
    size_t len;

    frame_put(&b, T_JOIN, "a", 1, NULL, 0);
    frame_put(&b, T_USERS, "", 0, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, frame_get(&b, &type, &p, &len));
    TEST_ASSERT_EQUAL_CHAR(T_JOIN, type);
    buf_drop(&b, HDR_SIZE + len);
    TEST_ASSERT_EQUAL_INT(1, frame_get(&b, &type, &p, &len));
    TEST_ASSERT_EQUAL_CHAR(T_USERS, type);
    TEST_ASSERT_EQUAL(0, len);
    buf_drop(&b, HDR_SIZE + len);
    TEST_ASSERT_EQUAL(0, b.len);
}

static void test_oversized_frames_rejected(void)
{
    char hdr[HDR_SIZE] = { T_FILE, 0x7f, 0, 0, 0 }, type;   /* длина ~2 ГБ */
    const char *p;
    size_t len;

    buf_append(&b, hdr, HDR_SIZE);
    TEST_ASSERT_EQUAL_INT(-1, frame_get(&b, &type, &p, &len));
    TEST_ASSERT_EQUAL_INT(-1, frame_put(&b, T_FILE, "", FRAME_MAX, "x", 1));
    TEST_ASSERT_EQUAL_INT(EMSGSIZE, errno);
}

static void test_name_valid(void)
{
    TEST_ASSERT_TRUE(name_valid("alice", 5));
    TEST_ASSERT_TRUE(name_valid("Вася", strlen("Вася")));   /* UTF-8 байты > 0x7f допустимы */
    TEST_ASSERT_FALSE(name_valid("", 0));
    TEST_ASSERT_FALSE(name_valid("a b", 3));
    TEST_ASSERT_FALSE(name_valid("a/b", 3));
    TEST_ASSERT_FALSE(name_valid("a,b", 3));
    TEST_ASSERT_FALSE(name_valid("a\0b", 3));
    TEST_ASSERT_FALSE(name_valid("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 32));
}

static void test_safe_basename(void)
{
    TEST_ASSERT_EQUAL_STRING(".bashrc", safe_basename("../../.bashrc"));
    TEST_ASSERT_EQUAL_STRING("passwd", safe_basename("/etc/passwd"));
    TEST_ASSERT_EQUAL_STRING("pic.png", safe_basename("pic.png"));
    TEST_ASSERT_NULL(safe_basename(""));
    TEST_ASSERT_NULL(safe_basename("dir/"));
    TEST_ASSERT_NULL(safe_basename(".."));
    TEST_ASSERT_NULL(safe_basename("a/."));
}

/* Больше, чем влезает в буфер сокета: send отдаёт частями, send_all должен дослать всё */
static void test_send_all_delivers_everything(void)
{
    enum { N = 3 << 20 };
    char *data = malloc(N), *got = malloc(N);
    int sv[2], i, st;
    ssize_t n, total = 0;
    pid_t pid;

    for (i = 0; i < N; i++)
        data[i] = (char)(i * 7);
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    pid = fork();
    if (pid == 0) {
        close(sv[0]);
        _exit(send_all(sv[1], data, N) == 0 ? 0 : 1);
    }
    close(sv[1]);
    while ((n = read(sv[0], got + total, N - total)) > 0)
        total += n;
    waitpid(pid, &st, 0);
    close(sv[0]);
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(st));
    TEST_ASSERT_EQUAL(N, total);
    TEST_ASSERT_EQUAL_MEMORY(data, got, N);
    free(data);
    free(got);
}

/* Собеседник закрыл соединение: EPIPE, а не смерть от SIGPIPE */
static void test_send_to_closed_peer_is_epipe(void)
{
    int sv[2];

    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    close(sv[1]);
    TEST_ASSERT_EQUAL_INT(-1, send_all(sv[0], "x", 1));
    TEST_ASSERT_EQUAL_INT(EPIPE, errno);
    close(sv[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_round_trip_with_two_parts);
    RUN_TEST(test_partial_frame_waits_for_the_rest);
    RUN_TEST(test_two_frames_in_one_buffer);
    RUN_TEST(test_oversized_frames_rejected);
    RUN_TEST(test_name_valid);
    RUN_TEST(test_safe_basename);
    RUN_TEST(test_send_all_delivers_everything);
    RUN_TEST(test_send_to_closed_peer_is_epipe);
    return UNITY_END();
}
