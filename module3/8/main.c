#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "sniff.h"

static volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int usage(void)
{
    fprintf(stderr, "usage: sniffer -f chat|dns [-i iface] [-c count] [-t seconds] [-o file]\n");
    return 2;
}

static long num(const char *s)
{
    char *end;
    long v = strtol(s, &end, 10);

    return *s && !*end && v > 0 && v < 1000000 ? v : -1;
}

static double now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);    /* не REALTIME: системное время может прыгнуть (NTP) */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* AF_PACKET, а не AF_INET/SOCK_RAW: только здесь кадр приходит с заголовком Ethernet, то есть с MAC.
 * ETH_P_ALL: сокеты на конкретный протокол (ETH_P_IP) не получают исходящие кадры этой машины. */
static int open_sock(const char *ifname)
{
    /* sll_protocol в bind заменяет протокол сокета, поэтому и здесь ETH_P_ALL */
    struct sockaddr_ll a = { .sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_ALL) };
    struct timeval tv = { .tv_usec = 200000 };
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (fd < 0)
        return -1;
    /* Таймаут чтения: раз в 200 мс проверяем флаг и время, даже если пакетов нет.
     * Сигнал, пришедший между проверкой флага и recvfrom, иначе ждал бы следующего пакета. */
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0 ||
        (ifname && (!(a.sll_ifindex = if_nametoindex(ifname)) || bind(fd, (struct sockaddr *)&a, sizeof a) < 0))) {
        int e = errno;
        close(fd);
        errno = e;
        return -1;
    }
    return fd;
}

static void print_all(FILE *out, const struct pkt *v, size_t n, const char *filter, const char *ifname)
{
    char line[256];

    fprintf(out, "captured %zu UDP segments, filter %s, interface %s\n", n, filter, ifname ? ifname : "any");
    fprintf(out, "   #        time  src MAC           > dst MAC            src IP:port > dst IP:port\n");
    for (size_t i = 0; i < n; i++) {
        pkt_format(&v[i], i + 1, line, sizeof line);
        fprintf(out, "%s\n", line);
    }
}

int main(int argc, char **argv)
{
    const char *fname = NULL, *ifname = NULL, *outname = NULL;
    long count = 0, secs = 0;
    enum filter f;
    struct sigaction sa = { .sa_handler = on_stop };
    static uint8_t frame[65536 + 64];
    struct pkt *v = NULL, p;
    size_t np = 0, cap = 0, dlen;
    const uint8_t *data;
    double start;
    int fd, opt, rc = 0;

    while ((opt = getopt(argc, argv, "f:i:c:t:o:")) != -1) {
        switch (opt) {
        case 'f': fname = optarg; break;
        case 'i': ifname = optarg; break;
        case 'o': outname = optarg; break;
        case 'c': if ((count = num(optarg)) < 0) return usage(); break;
        case 't': if ((secs = num(optarg)) < 0) return usage(); break;
        default: return usage();
        }
    }
    if (!fname || optind != argc || filter_parse(fname, &f) < 0)
        return usage();
    if ((fd = open_sock(ifname)) < 0) {
        fprintf(stderr, "sniffer: %s: %s%s\n", ifname ? ifname : "socket", strerror(errno),
                errno == EPERM ? " (needs CAP_NET_RAW: sudo or setcap)" : "");
        return 1;
    }
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    fprintf(stderr, "sniffer: capturing on %s, filter %s, Ctrl+C to stop\n", ifname ? ifname : "any", fname);

    start = now();
    while (!g_stop && (!count || (long)np < count) && (!secs || now() - start < secs)) {
        struct sockaddr_ll from;
        socklen_t alen = sizeof from;
        ssize_t n = recvfrom(fd, frame, sizeof frame, 0, (struct sockaddr *)&from, &alen);
        double t = now() - start;

        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("recvfrom");
            rc = 1;
            break;
        }
        /* У VPN (tun) заголовка Ethernet нет, разбирать его как Ethernet нельзя.
         * На lo каждый пакет виден дважды, исходящим и входящим: берём только входящий, как tcpdump. */
        if ((from.sll_hatype != ARPHRD_ETHER && from.sll_hatype != ARPHRD_LOOPBACK) ||
            (from.sll_hatype == ARPHRD_LOOPBACK && from.sll_pkttype == PACKET_OUTGOING))
            continue;
        if (pkt_parse(frame, n, &p, &data, &dlen) < 0 || !filter_match(f, &p, data, dlen))
            continue;
        if (np == cap) {
            struct pkt *nv = realloc(v, (cap = cap ? cap * 2 : 64) * sizeof *v);
            if (!nv) {
                perror("realloc");
                rc = 1;
                break;
            }
            v = nv;
        }
        p.t = t;
        v[np++] = p;
    }
    close(fd);

    print_all(stdout, v, np, fname, ifname);
    if (outname) {
        FILE *out = fopen(outname, "w");
        if (out)
            print_all(out, v, np, fname, ifname);
        if (!out || fclose(out) != 0) {     /* fclose сбрасывает буфер: ошибка записи видна только здесь */
            perror(outname);
            rc = 1;
        }
    }
    free(v);
    return rc;
}
