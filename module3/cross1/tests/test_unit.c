#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "rawudp.h"
#include "table.h"

static struct table t;
static uint8_t pkt[2048];

void setUp(void) { memset(&t, 0, sizeof t); memset(pkt, 0, sizeof pkt); }
void tearDown(void) { table_free(&t); }

/* IP-заголовок (ihl слов) перед уже собранным UDP; возвращает длину пакета */
static size_t ip_wrap(int ihl, int proto, size_t udp_len)
{
    memmove(pkt + ihl * 4, pkt, udp_len);
    memset(pkt, 0, ihl * 4);
    pkt[0] = 0x40 | ihl;
    pkt[2] = (ihl * 4 + udp_len) >> 8;
    pkt[3] = (ihl * 4 + udp_len) & 0xff;
    pkt[9] = proto;
    memcpy(pkt + 12, "\x7f\x00\x00\x01", 4);
    memcpy(pkt + 16, "\x7f\x00\x00\x02", 4);
    return ihl * 4 + udp_len;
}

static void test_udp_build_header_in_network_order(void)
{
    size_t n = udp_build(pkt, sizeof pkt, 40001, 51500, "ping", 4);

    TEST_ASSERT_EQUAL(12, n);
    TEST_ASSERT_EQUAL_MEMORY("\x9c\x41" "\xc9\x2c" "\x00\x0c" "\x00\x00" "ping", pkt, 12);  /* 40001, 51500, 12, сумма 0 */
    TEST_ASSERT_EQUAL(8, udp_build(pkt, sizeof pkt, 1, 2, NULL, 0));    /* закрытие: только заголовок */
    TEST_ASSERT_EQUAL(0, udp_build(pkt, 10, 1, 2, "ping", 4));          /* не влезает в буфер */
}

static void test_udp_parse_round_trip(void)
{
    struct udp_in u;
    size_t n = ip_wrap(5, 17, udp_build(pkt, sizeof pkt, 40001, 51500, "ping", 4));
    char ip[INET_ADDRSTRLEN];

    TEST_ASSERT_EQUAL_INT(0, udp_parse(pkt, n, &u));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", inet_ntop(AF_INET, &u.src, ip, sizeof ip));
    TEST_ASSERT_EQUAL_STRING("127.0.0.2", inet_ntop(AF_INET, &u.dst, ip, sizeof ip));
    TEST_ASSERT_EQUAL_UINT16(40001, u.sport);
    TEST_ASSERT_EQUAL_UINT16(51500, u.dport);
    TEST_ASSERT_EQUAL(4, u.len);
    TEST_ASSERT_EQUAL_MEMORY("ping", u.data, 4);
}

static void test_udp_parse_ip_options_and_broken(void)
{
    struct udp_in u;
    size_t n = ip_wrap(7, 17, udp_build(pkt, sizeof pkt, 1, 2, "x", 1));

    TEST_ASSERT_EQUAL_INT(0, udp_parse(pkt, n, &u));
    TEST_ASSERT_EQUAL_PTR(pkt + 28 + 8, u.data);                        /* 28-байтовый IP-заголовок */
    TEST_ASSERT_EQUAL_INT(-1, udp_parse(pkt, n - 1, &u));               /* обрезан */
    TEST_ASSERT_EQUAL_INT(-1, udp_parse(pkt, 19, &u));
    pkt[9] = 6;                                                         /* TCP */
    TEST_ASSERT_EQUAL_INT(-1, udp_parse(pkt, n, &u));
    pkt[9] = 17;
    pkt[28 + 5] = 50;                                                   /* длина UDP больше пакета */
    TEST_ASSERT_EQUAL_INT(-1, udp_parse(pkt, n, &u));
    pkt[28 + 5] = 4;                                                    /* меньше заголовка */
    TEST_ASSERT_EQUAL_INT(-1, udp_parse(pkt, n, &u));
}

static void test_message_kinds(void)
{
    TEST_ASSERT_TRUE(IS_CLOSE(0, ""));
    TEST_ASSERT_TRUE(IS_ALIVE(1, ""));
    TEST_ASSERT_FALSE(IS_ALIVE(1, "a"));
    TEST_ASSERT_FALSE(IS_CLOSE(1, ""));
}

static void test_table_counts_per_ip_and_port(void)
{
    uint32_t a = htonl(0x7f000001), b = htonl(0x7f000002);

    TEST_ASSERT_NULL(table_get(&t, a, 1000, 0));
    table_get(&t, a, 1000, 1)->count = 5;
    table_get(&t, a, 1001, 1)->count = 7;               /* тот же ip, другой порт — другой клиент */
    table_get(&t, b, 1000, 1)->count = 9;
    TEST_ASSERT_EQUAL(3, t.n);
    TEST_ASSERT_EQUAL_UINT(5, table_get(&t, a, 1000, 1)->count);    /* create не создаёт второй */
    TEST_ASSERT_EQUAL_UINT(7, table_get(&t, a, 1001, 0)->count);
    TEST_ASSERT_EQUAL_UINT(9, table_get(&t, b, 1000, 0)->count);
    TEST_ASSERT_EQUAL(3, t.n);
}

/* 2000 клиентов на 256 корзин: цепочки длиной ~8, удаление из головы, середины и хвоста */
static void test_table_many_clients_and_remove(void)
{
    for (unsigned i = 0; i < 2000; i++)
        table_get(&t, htonl(0x0a000000 + i / 10), 40000 + i % 10, 1)->count = i;
    TEST_ASSERT_EQUAL(2000, t.n);
    for (unsigned i = 0; i < 2000; i += 3)
        TEST_ASSERT_EQUAL_INT(0, table_remove(&t, htonl(0x0a000000 + i / 10), 40000 + i % 10));
    TEST_ASSERT_EQUAL(2000 - 667, t.n);
    for (unsigned i = 0; i < 2000; i++) {
        struct client *c = table_get(&t, htonl(0x0a000000 + i / 10), 40000 + i % 10, 0);
        if (i % 3 == 0)
            TEST_ASSERT_NULL(c);
        else
            TEST_ASSERT_EQUAL_UINT(i, c->count);
    }
    TEST_ASSERT_EQUAL_INT(-1, table_remove(&t, 1, 1));
}

static int gone_calls;
static void gone(const struct client *c) { (void)c; gone_calls++; }

static void test_table_expire_silent_clients(void)
{
    for (unsigned i = 0; i < 100; i++)
        table_get(&t, htonl(i), 1, 1)->seen = i;        /* «молчат» с момента i */
    gone_calls = 0;
    TEST_ASSERT_EQUAL(40, table_expire(&t, 100, 60, gone));     /* seen < 40 */
    TEST_ASSERT_EQUAL_INT(40, gone_calls);
    TEST_ASSERT_EQUAL(60, t.n);
    TEST_ASSERT_NULL(table_get(&t, htonl(39), 1, 0));
    TEST_ASSERT_NOT_NULL(table_get(&t, htonl(40), 1, 0));
}

static void test_reserve_gives_free_port_and_blocks_it(void)
{
    uint16_t p = 0, q;
    int fd = udp_reserve(&p), fd2;

    TEST_ASSERT_TRUE(fd >= 0);
    TEST_ASSERT_TRUE(p >= 1024);                        /* ядро выдало эфемерный порт */
    q = p;
    fd2 = udp_reserve(&q);
    TEST_ASSERT_EQUAL_INT(-1, fd2);                     /* EADDRINUSE: порт занят, пока fd открыт */
    close(fd);
    TEST_ASSERT_EQUAL_INT(-1, num("0"));
    TEST_ASSERT_EQUAL_INT(-1, num("65536"));
    TEST_ASSERT_EQUAL_INT(-1, num("5s"));
    TEST_ASSERT_EQUAL_INT(51500, num("51500"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_udp_build_header_in_network_order);
    RUN_TEST(test_udp_parse_round_trip);
    RUN_TEST(test_udp_parse_ip_options_and_broken);
    RUN_TEST(test_message_kinds);
    RUN_TEST(test_table_counts_per_ip_and_port);
    RUN_TEST(test_table_many_clients_and_remove);
    RUN_TEST(test_table_expire_silent_clients);
    RUN_TEST(test_reserve_gives_free_port_and_blocks_it);
    return UNITY_END();
}
