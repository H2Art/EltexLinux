#include <arpa/inet.h>
#include <string.h>
#include "unity.h"
#include "sniff.h"

static uint8_t f[2048];
static struct pkt p;
static const uint8_t *data;
static size_t dlen;

void setUp(void) { memset(f, 0, sizeof f); }
void tearDown(void) { }

static void put16(uint8_t *at, uint16_t v) { at[0] = v >> 8; at[1] = v & 0xff; }

/* Кадр Ethernet + IPv4 (ihl слов по 4 байта) + UDP + данные; возвращает длину кадра */
static size_t frame(int ihl, int proto, uint16_t sport, uint16_t dport, const void *d, size_t n)
{
    uint8_t *ip = f + 14, *udp = ip + ihl * 4;

    memcpy(f, "\xff\xff\xff\xff\xff\xff", 6);               /* получатель: broadcast */
    memcpy(f + 6, "\x02\x11\x22\x33\x44\x55", 6);           /* отправитель */
    put16(f + 12, 0x0800);
    ip[0] = 0x40 | ihl;
    put16(ip + 2, ihl * 4 + 8 + n);
    ip[8] = 64;
    ip[9] = proto;
    memcpy(ip + 12, "\x0a\x00\x00\x01", 4);                 /* 10.0.0.1 */
    memcpy(ip + 16, "\x0a\x00\x00\xff", 4);                 /* 10.0.0.255 */
    put16(udp, sport);
    put16(udp + 2, dport);
    put16(udp + 4, 8 + n);
    memcpy(udp + 8, d, n);
    return 14 + ihl * 4 + 8 + n;
}

static void test_parse_fields(void)
{
    size_t n = frame(5, 17, 50999, 50999, "hello", 5);
    char ip[INET_ADDRSTRLEN];

    TEST_ASSERT_EQUAL_INT(0, pkt_parse(f, n, &p, &data, &dlen));
    TEST_ASSERT_EQUAL_MEMORY("\x02\x11\x22\x33\x44\x55", p.smac, 6);
    TEST_ASSERT_EQUAL_MEMORY("\xff\xff\xff\xff\xff\xff", p.dmac, 6);
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", inet_ntop(AF_INET, &p.sip, ip, sizeof ip));
    TEST_ASSERT_EQUAL_STRING("10.0.0.255", inet_ntop(AF_INET, &p.dip, ip, sizeof ip));
    TEST_ASSERT_EQUAL_UINT16(50999, p.sport);       /* ntohs: в кадре байты C7 37 */
    TEST_ASSERT_EQUAL_UINT16(5, p.len);
    TEST_ASSERT_EQUAL(5, dlen);
    TEST_ASSERT_EQUAL_MEMORY("hello", data, 5);
}

static void test_ip_options_shift_udp_header(void)
{
    size_t n = frame(6, 17, 1111, 53, "xy", 2);     /* 24-байтовый IP-заголовок */

    TEST_ASSERT_EQUAL_INT(0, pkt_parse(f, n, &p, &data, &dlen));
    TEST_ASSERT_EQUAL_UINT16(1111, p.sport);
    TEST_ASSERT_EQUAL_UINT16(53, p.dport);
    TEST_ASSERT_EQUAL_PTR(f + 14 + 24 + 8, data);
}

static void test_ethernet_padding_is_not_data(void)
{
    size_t n = frame(5, 17, 1, 2, "a", 1);

    TEST_ASSERT_EQUAL_INT(0, pkt_parse(f, 60, &p, &data, &dlen));   /* минимальный кадр, хвост нулей */
    TEST_ASSERT_EQUAL(1, dlen);
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n - 1, &p, &data, &dlen)); /* обрезан: tot_len больше кадра */
}

static void test_rejects_other_and_broken_packets(void)
{
    size_t n = frame(5, 6, 1, 2, "a", 1);           /* TCP */

    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    n = frame(5, 17, 1, 2, "a", 1);
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, 20, &p, &data, &dlen));  /* короче заголовков */
    put16(f + 12, 0x0806);                          /* ARP */
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    put16(f + 12, 0x0800);
    f[14] = 0x44;                                   /* ihl 4 < 5 */
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    f[14] = 0x45;
    put16(f + 14 + 20 + 4, 100);                    /* длина UDP больше IP-пакета */
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    put16(f + 14 + 20 + 4, 9);
    put16(f + 14 + 6, 0x2000);                      /* флаг MF: первый фрагмент */
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    put16(f + 14 + 6, 0x0010);                      /* смещение фрагмента */
    TEST_ASSERT_EQUAL_INT(-1, pkt_parse(f, n, &p, &data, &dlen));
    put16(f + 14 + 6, 0x4000);                      /* DF — не фрагмент */
    TEST_ASSERT_EQUAL_INT(0, pkt_parse(f, n, &p, &data, &dlen));
}

static int match(enum filter flt, uint16_t sport, uint16_t dport, const void *d, size_t n)
{
    size_t len = frame(5, 17, sport, dport, d, n);

    TEST_ASSERT_EQUAL_INT(0, pkt_parse(f, len, &p, &data, &dlen));
    return filter_match(flt, &p, data, dlen);
}

static void test_chat_filter(void)
{
    TEST_ASSERT_EQUAL_INT(1, match(F_CHAT, 50999, 50999, "M 1a2b3c4d alice hi bob", 23));
    TEST_ASSERT_EQUAL_STRING("M alice: hi bob", p.info);
    TEST_ASSERT_EQUAL_INT(1, match(F_CHAT, 40000, 50999, "J 1a2b3c4d alice", 16));
    TEST_ASSERT_EQUAL_STRING("J alice", p.info);
    TEST_ASSERT_EQUAL_INT(0, match(F_CHAT, 50999, 50998, "M 1a2b3c4d alice hi", 19));  /* не тот порт */
    TEST_ASSERT_EQUAL_INT(0, match(F_CHAT, 50999, 50999, "hello world, not chat", 21)); /* не тот формат */
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 50999, 50999, "M 1a2b3c4d alice hi", 19));
}

/* Запрос A example.com: заголовок 12 байт, затем имя метками, тип и класс */
static const uint8_t dns_q[] = "\x12\x34\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00"
                               "\x07" "example" "\x03" "com" "\x00" "\x00\x01\x00\x01";

static void test_dns_filter(void)
{
    uint8_t d[sizeof dns_q];

    TEST_ASSERT_EQUAL_INT(1, match(F_DNS, 45000, 53, dns_q, sizeof dns_q - 1));
    TEST_ASSERT_EQUAL_STRING("query example.com", p.info);
    memcpy(d, dns_q, sizeof d);
    d[2] |= 0x80;                                   /* бит QR: ответ */
    TEST_ASSERT_EQUAL_INT(1, match(F_DNS, 53, 45000, d, sizeof d - 1));
    TEST_ASSERT_EQUAL_STRING("reply example.com", p.info);
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 45000, 5353, dns_q, sizeof dns_q - 1));   /* не порт 53 */
    TEST_ASSERT_EQUAL_INT(0, match(F_CHAT, 45000, 53, dns_q, sizeof dns_q - 1));
}

static void test_dns_rejects_malformed_names(void)
{
    uint8_t d[sizeof dns_q];

    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 1, 53, dns_q, 11));               /* меньше заголовка */
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 1, 53, dns_q, 12 + 5));           /* метка за концом */
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 1, 53, dns_q, 12 + 12));          /* нет нулевой метки */
    memcpy(d, dns_q, sizeof d);
    d[12] = 0xc0;                                   /* указатель сжатия вместо метки */
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 1, 53, d, sizeof d - 1));
    d[12] = 0;                                      /* корень: пустое имя */
    TEST_ASSERT_EQUAL_INT(1, match(F_DNS, 1, 53, d, 17));
    TEST_ASSERT_EQUAL_STRING("query .", p.info);
}

static void test_long_dns_name_does_not_overflow_info(void)
{
    uint8_t d[12 + 4 * 64 + 1] = { 0 };

    for (int i = 0; i < 4; i++) {                   /* 4 метки по 63 байта: имя длиннее info */
        d[12 + i * 64] = 63;
        memset(d + 13 + i * 64, 'a', 63);
    }
    TEST_ASSERT_EQUAL_INT(0, match(F_DNS, 1, 53, d, sizeof d));
}

static void test_format_and_filter_names(void)
{
    char line[256];
    enum filter flt;

    match(F_CHAT, 50999, 50999, "L 1a2b3c4d bob", 14);
    p.t = 1.5;
    pkt_format(&p, 7, line, sizeof line);
    TEST_ASSERT_EQUAL_STRING("   7    1.500000  02:11:22:33:44:55 > ff:ff:ff:ff:ff:ff  "
                             "10.0.0.1:50999 > 10.0.0.255:50999  len 14  L bob", line);
    TEST_ASSERT_EQUAL_INT(0, filter_parse("dns", &flt));
    TEST_ASSERT_EQUAL_INT(F_DNS, flt);
    TEST_ASSERT_EQUAL_INT(-1, filter_parse("tcp", &flt));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_fields);
    RUN_TEST(test_ip_options_shift_udp_header);
    RUN_TEST(test_ethernet_padding_is_not_data);
    RUN_TEST(test_rejects_other_and_broken_packets);
    RUN_TEST(test_chat_filter);
    RUN_TEST(test_dns_filter);
    RUN_TEST(test_dns_rejects_malformed_names);
    RUN_TEST(test_long_dns_name_does_not_overflow_info);
    RUN_TEST(test_format_and_filter_names);
    return UNITY_END();
}
