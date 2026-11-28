#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "unity.h"
#include "taxi.h"

static int tfd = -1;

void setUp(void) { tfd = timerfd_create(CLOCK_MONOTONIC, 0); }
void tearDown(void) { close(tfd); }

static void test_parse_commands(void)
{
    struct cmd c;

    TEST_ASSERT_EQUAL_INT(C_CREATE, parse_cmd("create_driver\n", &c));
    TEST_ASSERT_EQUAL_INT(C_TASK, parse_cmd("  send_task 123 10\n", &c));
    TEST_ASSERT_EQUAL_INT(123, c.pid);
    TEST_ASSERT_EQUAL_INT(10, c.timer);
    TEST_ASSERT_EQUAL_INT(C_STATUS, parse_cmd("get_status 77", &c));
    TEST_ASSERT_EQUAL_INT(77, c.pid);
    TEST_ASSERT_EQUAL_INT(C_LIST, parse_cmd("get_drivers", &c));
    TEST_ASSERT_EQUAL_INT(C_EXIT, parse_cmd("exit", &c));
    TEST_ASSERT_EQUAL_INT(C_NONE, parse_cmd("   \n", &c));
}

static void test_parse_rejects_bad_input(void)
{
    struct cmd c;

    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("fly_away", &c));
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("send_task 123", &c));          /* мало аргументов */
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("send_task 123 5 6", &c));      /* лишний */
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("send_task 123 0", &c));        /* таймер > 0 */
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("send_task 123 86401", &c));
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("send_task abc 5", &c));
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("get_status -1", &c));
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("get_status 5x", &c));
    TEST_ASSERT_EQUAL_INT(C_BAD, parse_cmd("get_drivers now", &c));
}

static void test_driver_busy_then_available(void)
{
    struct msg task = { .type = M_TASK, .value = 3 }, st = { .type = M_STATUS }, out;

    TEST_ASSERT_EQUAL_INT(1, driver_handle(tfd, &st, &out));
    TEST_ASSERT_EQUAL_INT(A_AVAILABLE, out.type);
    TEST_ASSERT_EQUAL_INT(getpid(), out.pid);
    TEST_ASSERT_EQUAL_INT(1, driver_handle(tfd, &task, &out));
    TEST_ASSERT_EQUAL_INT(A_OK, out.type);
    TEST_ASSERT_EQUAL_INT(3, out.value);
    task.value = 10;
    TEST_ASSERT_EQUAL_INT(1, driver_handle(tfd, &task, &out));      /* занят: новую задачу не берёт */
    TEST_ASSERT_EQUAL_INT(A_BUSY, out.type);
    TEST_ASSERT_EQUAL_INT(3, out.value);                            /* 2.99... округлено вверх */
    TEST_ASSERT_EQUAL_INT(3, remaining(tfd));                       /* таймер не перезапущен */
    TEST_ASSERT_EQUAL_INT(1, driver_handle(tfd, &st, &out));
    TEST_ASSERT_EQUAL_INT(A_BUSY, out.type);
}

static void test_timer_expiry_frees_driver(void)
{
    struct msg task = { .type = M_TASK, .value = 1 }, st = { .type = M_STATUS }, out;
    struct timespec t0, t1;
    uint64_t fired;

    driver_handle(tfd, &task, &out);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    TEST_ASSERT_EQUAL(sizeof fired, read(tfd, &fired, sizeof fired));   /* блокирующий: спим до срабатывания */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    TEST_ASSERT_EQUAL_UINT64(1, fired);
    TEST_ASSERT_TRUE((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 > 0.9);
    TEST_ASSERT_EQUAL_INT(0, remaining(tfd));
    driver_handle(tfd, &st, &out);
    TEST_ASSERT_EQUAL_INT(A_AVAILABLE, out.type);
}

static void test_quit_and_unknown(void)
{
    struct msg q = { .type = M_QUIT }, junk = { .type = 99 }, out;

    TEST_ASSERT_EQUAL_INT(-1, driver_handle(tfd, &q, &out));
    TEST_ASSERT_EQUAL_INT(0, driver_handle(tfd, &junk, &out));
}

static int taxi_names(void)
{
    DIR *dir = opendir("/dev/mqueue");
    struct dirent *e;
    int n = 0;

    while (dir && (e = readdir(dir)))
        n += strncmp(e->d_name, "taxi_", 5) == 0;
    if (dir)
        closedir(dir);
    return n;
}

/* Имя удалено сразу, а очередь работает; полная очередь не блокирует (O_NONBLOCK) */
static void test_queue_is_anonymous_and_nonblocking(void)
{
    mqd_t q = queue_new();
    struct msg m = { .type = M_STATUS }, r;
    int sent = 0;

    TEST_ASSERT_TRUE(q != (mqd_t)-1);
    TEST_ASSERT_EQUAL_INT(0, taxi_names());
    while (mq_send(q, (const char *)&m, sizeof m, 0) == 0)
        sent++;
    TEST_ASSERT_EQUAL_INT(10, sent);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno);
    TEST_ASSERT_EQUAL(sizeof r, mq_receive(q, (char *)&r, sizeof r, NULL));
    TEST_ASSERT_EQUAL_INT(M_STATUS, r.type);
    mq_close(q);
}

/* Настоящий водитель в дочернем процессе: задача, занятость, освобождение по таймеру, выход по QUIT */
static void test_driver_process(void)
{
    mqd_t cmd = queue_new(), rep = queue_new();
    struct msg m, r;
    pid_t parent = getpid(), pid = fork();
    int st, got = 0;

    if (pid == 0)
        driver_run(cmd, rep, parent);
    m = (struct msg){ .type = M_TASK, .value = 1 };
    mq_send(cmd, (const char *)&m, sizeof m, 0);
    mq_send(cmd, (const char *)&m, sizeof m, 0);
    for (int i = 0; i < 300 && got < 3; i++) {      /* ждём OK, BUSY и DONE до 3 с */
        if (mq_receive(rep, (char *)&r, sizeof r, NULL) == sizeof r) {
            TEST_ASSERT_EQUAL_INT(pid, r.pid);
            TEST_ASSERT_EQUAL_INT(got == 0 ? A_OK : got == 1 ? A_BUSY : A_DONE, r.type);
            got++;
        }
        usleep(10000);
    }
    TEST_ASSERT_EQUAL_INT(3, got);
    m.type = M_QUIT;
    mq_send(cmd, (const char *)&m, sizeof m, 0);
    TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &st, 0));
    TEST_ASSERT_TRUE(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    mq_close(cmd);
    mq_close(rep);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_commands);
    RUN_TEST(test_parse_rejects_bad_input);
    RUN_TEST(test_driver_busy_then_available);
    RUN_TEST(test_timer_expiry_frees_driver);
    RUN_TEST(test_quit_and_unknown);
    RUN_TEST(test_queue_is_anonymous_and_nonblocking);
    RUN_TEST(test_driver_process);
    return UNITY_END();
}
