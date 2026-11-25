#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "unity.h"
#include "pc.h"

static char name[PC_NAME_SIZE];
static struct pc prod, cons;

/* Своё имя на процесс, чтобы не задеть настоящий запуск с /eltex_pc */
void setUp(void)
{
    snprintf(name, sizeof name, "/test_pc_%d", (int)getpid());
    prod.sem = cons.sem = SEM_FAILED;
    prod.base = cons.base = NULL;
}

void tearDown(void)
{
    pc_detach(&cons);
    pc_detach(&prod);
    shm_unlink(name);
    sem_unlink(name);
}

static struct pc_block *at(struct pc *p, long off)
{
    return (struct pc_block *)(p->base + off);
}

static void test_blocks_are_linked_by_offsets(void)
{
    int a[] = { 1, 2, 3 }, b[] = { 4 }, c[] = { 5, 6 };
    long o1, o2, o3;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 1024));
    o1 = pc_add(&prod, a, 3);
    o2 = pc_add(&prod, b, 1);
    o3 = pc_add(&prod, c, 2);
    TEST_ASSERT_TRUE(o1 > 0);              /* смещение 0 занято заголовком, значит next = 0 однозначно «конец» */
    TEST_ASSERT_TRUE(o2 > o1 && o3 > o2);
    TEST_ASSERT_EQUAL(o2, at(&prod, o1)->next);
    TEST_ASSERT_EQUAL(o3, at(&prod, o2)->next);
    TEST_ASSERT_EQUAL(0, at(&prod, o3)->next);
    TEST_ASSERT_EQUAL_INT(6, at(&prod, o3)->data[1]);
}

static void test_take_finds_min_max_and_marks_processed(void)
{
    int a[] = { 7, -3, 12, 0 }, b[] = { 5 }, min, max;
    size_t off;
    long o1, o2;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 1024));
    TEST_ASSERT_EQUAL_INT(0, pc_take(&prod, &min, &max, &off));   /* блоков ещё нет */
    o1 = pc_add(&prod, a, 4);
    o2 = pc_add(&prod, b, 1);
    TEST_ASSERT_EQUAL_INT(4, pc_take(&prod, &min, &max, &off));
    TEST_ASSERT_EQUAL_INT(-3, min);
    TEST_ASSERT_EQUAL_INT(12, max);
    TEST_ASSERT_EQUAL(o1, off);
    TEST_ASSERT_EQUAL_INT(0, at(&prod, o1)->count);
    TEST_ASSERT_EQUAL_INT(1, pc_take(&prod, &min, &max, &off));   /* обработанный пропущен */
    TEST_ASSERT_EQUAL(o2, off);
    TEST_ASSERT_EQUAL_INT(0, pc_pending(&prod));
}

static void test_add_stops_when_memory_is_full(void)
{
    int vals[PC_MAX_COUNT] = { 0 }, added = 0;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 256));
    while (pc_add(&prod, vals, PC_MAX_COUNT) >= 0)
        added++;
    TEST_ASSERT_TRUE(added > 0);
    TEST_ASSERT_TRUE(pc_hdr(&prod)->end <= 256);
    TEST_ASSERT_EQUAL(-1, pc_add(&prod, vals, 0));      /* пустой набор неотличим от обработанного */
}

static void test_consumer_learns_size_and_sees_data(void)
{
    int a[] = { 42, 8 }, min, max;
    size_t off;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 1000));
    pc_add(&prod, a, 2);
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, name));
    TEST_ASSERT_EQUAL(1000, cons.size);                /* размер взят через fstat */
    TEST_ASSERT_EQUAL_INT(0, pc_lock(&cons));
    TEST_ASSERT_EQUAL_INT(2, pc_take(&cons, &min, &max, &off));
    TEST_ASSERT_EQUAL_INT(0, pc_unlock(&cons));
    TEST_ASSERT_EQUAL_INT(42, max);
    TEST_ASSERT_EQUAL_INT(0, pc_pending(&prod));       /* пометку видно через адрес производителя */
}

static void test_second_producer_and_lonely_consumer_fail(void)
{
    struct pc other;

    TEST_ASSERT_EQUAL_INT(-1, pc_attach(&cons, name));
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 1024));
    TEST_ASSERT_EQUAL_INT(-1, pc_create(&other, name, 1024));
    TEST_ASSERT_EQUAL_INT(EEXIST, errno);
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, name));  /* неудачный create не удалил чужие объекты */
}

/* Остался чужой семафор (например, после kill -9): create не удался и не оставил память */
static void test_failed_create_leaves_no_memory(void)
{
    sem_t *s = sem_open(name, O_CREAT, 0600, 1);

    TEST_ASSERT_TRUE(s != SEM_FAILED);
    sem_close(s);
    TEST_ASSERT_EQUAL_INT(-1, pc_create(&prod, name, 1024));
    TEST_ASSERT_EQUAL_INT(EEXIST, errno);
    TEST_ASSERT_EQUAL_INT(-1, shm_open(name, O_RDWR, 0));
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
}

/* После unlink новые не подключатся, а уже подключённые работают дальше */
static void test_unlink_keeps_attached_consumer_working(void)
{
    int a[] = { 3 }, min, max;
    size_t off;

    TEST_ASSERT_EQUAL_INT(0, pc_create(&prod, name, 1024));
    pc_add(&prod, a, 1);
    TEST_ASSERT_EQUAL_INT(0, pc_attach(&cons, name));
    pc_detach(&prod);
    pc_remove(&prod);
    TEST_ASSERT_EQUAL_INT(0, pc_lock(&cons));
    TEST_ASSERT_EQUAL_INT(1, pc_take(&cons, &min, &max, &off));
    TEST_ASSERT_EQUAL_INT(0, pc_unlock(&cons));
    TEST_ASSERT_EQUAL_INT(-1, pc_attach(&prod, name));
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
}

static void test_name_too_long(void)
{
    char big[PC_NAME_SIZE + 8];

    memset(big, 'x', sizeof big - 1);
    big[0] = '/';
    big[sizeof big - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(-1, pc_create(&prod, big, 1024));
    TEST_ASSERT_EQUAL_INT(ENAMETOOLONG, errno);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blocks_are_linked_by_offsets);
    RUN_TEST(test_take_finds_min_max_and_marks_processed);
    RUN_TEST(test_add_stops_when_memory_is_full);
    RUN_TEST(test_consumer_learns_size_and_sees_data);
    RUN_TEST(test_second_producer_and_lonely_consumer_fail);
    RUN_TEST(test_failed_create_leaves_no_memory);
    RUN_TEST(test_unlink_keeps_attached_consumer_working);
    RUN_TEST(test_name_too_long);
    return UNITY_END();
}
