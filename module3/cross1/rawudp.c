#include "rawudp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* Без SA_RESTART: poll вернётся с EINTR, и цикл увидит флаг */
int install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0 ? -1 : 0;
}

double now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Порт или число секунд: 1..65535, вся строка — число; иначе -1 */
long num(const char *s)
{
    char *end;
    long v = strtol(s, &end, 10);

    return *s && !*end && v > 0 && v <= 65535 ? v : -1;
}

/* Ядро отдаёт сюда копию каждого входящего UDP-пакета машины вместе с IP-заголовком,
 * а при отправке само строит IP-заголовок: UDP-заголовок пишем мы */
int raw_open(void)
{
    return socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
}

/* У RAW-сокета нет порта. Обычный UDP-сокет на том же порту держит его занятым для других программ,
 * и ядро видит получателя: на наши пакеты оно не ответит ICMP port unreachable.
 * Из этого сокета не читаем: его очередь заполнится, и ядро будет молча отбрасывать копии для него. */
int udp_reserve(uint16_t *port)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(*port), .sin_addr.s_addr = htonl(INADDR_ANY) };
    socklen_t alen = sizeof a;
    int fd = socket(AF_INET, SOCK_DGRAM, 0), e;

    if (fd < 0)
        return -1;
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0 || getsockname(fd, (struct sockaddr *)&a, &alen) < 0) {
        e = errno;
        close(fd);
        errno = e;
        return -1;
    }
    *port = ntohs(a.sin_port);      /* при *port == 0 ядро выдало свободный */
    return fd;
}

/* Контрольная сумма 0: в IPv4 это «не считалась», получатель её не проверяет */
size_t udp_build(uint8_t *buf, size_t size, uint16_t sport, uint16_t dport, const void *d, size_t n)
{
    struct udphdr uh = { .source = htons(sport), .dest = htons(dport), .len = htons(sizeof uh + n), .check = 0 };

    if (n > PKT_MAX - sizeof(struct iphdr) - sizeof uh || sizeof uh + n > size)
        return 0;
    memcpy(buf, &uh, sizeof uh);
    if (n)
        memcpy(buf + sizeof uh, d, n);
    return sizeof uh + n;
}

/* Разбор пакета с RAW-сокета: IP-заголовок переменной длины (ihl), затем UDP */
int udp_parse(const uint8_t *pkt, size_t n, struct udp_in *u)
{
    struct iphdr ip;
    struct udphdr uh;
    size_t ihl, tot, ulen;

    if (n < sizeof ip)
        return -1;
    memcpy(&ip, pkt, sizeof ip);
    ihl = ip.ihl * 4u;
    tot = ntohs(ip.tot_len);
    if (ip.version != 4 || ip.protocol != IPPROTO_UDP || ihl < sizeof ip || tot < ihl + sizeof uh || tot > n)
        return -1;
    memcpy(&uh, pkt + ihl, sizeof uh);
    ulen = ntohs(uh.len);
    if (ulen < sizeof uh || ulen > tot - ihl)
        return -1;
    u->src.s_addr = ip.saddr;
    u->dst.s_addr = ip.daddr;
    u->sport = ntohs(uh.source);
    u->dport = ntohs(uh.dest);
    u->data = pkt + ihl + sizeof uh;
    u->len = ulen - sizeof uh;
    return 0;
}

int udp_send(int raw, struct in_addr dst, uint16_t sport, uint16_t dport, const void *d, size_t n)
{
    static uint8_t buf[PKT_MAX];
    struct sockaddr_in to = { .sin_family = AF_INET, .sin_addr = dst };   /* sin_port для RAW не используется */
    size_t len = udp_build(buf, sizeof buf, sport, dport, d, n);

    if (!len) {
        errno = EMSGSIZE;
        return -1;
    }
    return sendto(raw, buf, len, 0, (struct sockaddr *)&to, sizeof to) < 0 ? -1 : 0;
}
