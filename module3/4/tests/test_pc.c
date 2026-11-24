#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include "unity.h"
#include "pc.h"

static char keyfile[] = "/tmp/test_pc_XXXXXX";
static key_t key;
static struct pc prod, cons;

/* Свой файл для ftok, чтобы не задеть настоящий запуск с ключом от /tmp */
void setUp(void)
{
    int fd;

    strcpy(keyfile, "/tmp/test_pc_XXXXXX");
    fd = mkstemp(keyfile);
    TEST_ASSERT_TRUE(fd >= 0);
    close(fd);
    key = ftok(keyfile, 'T');
    prod.shmid = prod.semid = cons.shmid = cons.semid = -1;
    prod.base = cons.base = NULL;
}

void tearDown(void)
{
    pc_detach(&cons);
    pc_detach(&prod);
    pc_remove(&prod);
    unlink(keyfile);
}

static struct pc_block *at(struct pc *p, long off)
{
    return (struct pc_block *)(p->base + off);
}

static void test_blocks_are_linked_by_offsets(void)
{
    int a[] = { 1, 2, 3 }, b[] = { 4 }, c[] = { 5, 6 };
    long o1, o2, o3;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    o1 = pc_add(&prod, a, 3);
    o2 = pc_add(&prod, b, 1);
    o3 = pc_add(&prod, c, 2);
    TEST_ASSERT_TRUE(o1 > 0);              /* смещение 0 занято заголовком, значит next = 0 однозначно «конец» */
    TEST_ASSERT_TRUE(o2 > o1 && o3 > o2);
    TEST_ASSERT_EQUAL(o2, at(&prod, o1)->next);
    TEST_ASSERT_EQUAL(o3, at(&prod, o2)->next);
    TEST_ASSERT_EQUAL(0, at(&prod, o3)->next);
    TEST_ASSERT_EQUAL_INT(3, at(&prod, o1)->count);
    TEST_ASSERT_EQUAL_INT(6, at(&prod, o3)->data[1]);
}

static void test_take_finds_min_max_and_marks_processed(void)
{
    int a[] = { 7, -3, 12, 0 };
    int min, max;
    size_t off;
    long o;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    o = pc_add(&prod, a, 4);
    TEST_ASSERT_EQUAL_INT(1, pc_pending(&prod));
    TEST_ASSERT_EQUAL_INT(4, pc_take(&prod, &min, &max, &off));
    TEST_ASSERT_EQUAL_INT(-3, min);
    TEST_ASSERT_EQUAL_INT(12, max);
    TEST_ASSERT_EQUAL(o, off);
    TEST_ASSERT_EQUAL_INT(0, at(&prod, o)->count);
    TEST_ASSERT_EQUAL_INT(0, pc_pending(&prod));
    TEST_ASSERT_EQUAL_INT(0, pc_take(&prod, &min, &max, &off));
}

static void test_take_goes_in_order_and_skips_processed(void)
{
    int a[] = { 1 }, b[] = { 2 }, min, max;
    size_t off;
    long o1, o2;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    TEST_ASSERT_EQUAL_INT(0, pc_take(&prod, &min, &max, &off));   /* блоков ещё нет */
    o1 = pc_add(&prod, a, 1);
    o2 = pc_add(&prod, b, 1);
    pc_take(&prod, &min, &max, &off);
    TEST_ASSERT_EQUAL(o1, off);
    pc_take(&prod, &min, &max, &off);
    TEST_ASSERT_EQUAL(o2, off);
    TEST_ASSERT_EQUAL_INT(2, min);
}

static void test_add_stops_when_memory_is_full(void)
{
    int vals[PC_MAX_COUNT] = { 0 }, added = 0;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 256));
    while (pc_add(&prod, vals, PC_MAX_COUNT) >= 0)
        added++;
    TEST_ASSERT_TRUE(added > 0);
    TEST_ASSERT_TRUE(pc_hdr(&prod)->end <= 256);
    TEST_ASSERT_EQUAL(-1, pc_add(&prod, vals, 0));      /* пустой набор неотличим от обработанного */
}

static void test_consumer_sees_producer_data(void)
{
    int a[] = { 42, 8 }, min, max;
    size_t off;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    pc_add(&prod, a, 2);
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, key));
    TEST_ASSERT_EQUAL_INT(2, pc_take(&cons, &min, &max, &off));
    TEST_ASSERT_EQUAL_INT(42, max);
    TEST_ASSERT_EQUAL_INT(0, pc_pending(&prod));       /* пометку видно через адрес производителя */
}

static void test_second_producer_and_lonely_consumer_fail(void)
{
    struct pc other;

    TEST_ASSERT_EQUAL_INT(-1, pc_attach(&cons, key));
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    TEST_ASSERT_EQUAL_INT(-1, pc_create(&other, key, 1024));
    TEST_ASSERT_EQUAL_INT(EEXIST, errno);
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, key));   /* неудачный create не удалил чужие объекты */
}

static void test_lock_of_dead_process_is_released(void)
{
    pid_t pid;
    int st;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        pc_lock(&prod);
        _exit(0);       /* умираем внутри замка */
    }
    waitpid(pid, &st, 0);
    TEST_ASSERT_EQUAL_INT(1, semctl(prod.semid, 0, GETVAL));   /* SEM_UNDO вернул +1 */
}

static void test_lock_fails_after_remove(void)
{
    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, key, 1024));
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, key));
    pc_remove(&prod);
    TEST_ASSERT_EQUAL_INT(-1, pc_lock(&cons));
    TEST_ASSERT_TRUE(errno == EINVAL || errno == EIDRM);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blocks_are_linked_by_offsets);
    RUN_TEST(test_take_finds_min_max_and_marks_processed);
    RUN_TEST(test_take_goes_in_order_and_skips_processed);
    RUN_TEST(test_add_stops_when_memory_is_full);
    RUN_TEST(test_consumer_sees_producer_data);
    RUN_TEST(test_second_producer_and_lonely_consumer_fail);
    RUN_TEST(test_lock_of_dead_process_is_released);
    RUN_TEST(test_lock_fails_after_remove);
    return UNITY_END();
}
