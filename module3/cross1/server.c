#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "rawudp.h"
#include "table.h"

static void log_client(const char *what, uint32_t ip, uint16_t port)
{
    char s[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip, s, sizeof s);
    printf("server: %s:%u %s\n", s, port, what);
}

static int usage(void)
{
    fprintf(stderr, "usage: server [-p port] [-T seconds_without_packets]\n");
    return 2;
}

static void on_gone(const struct client *c)
{
    log_client("expired (no packets)", c->ip, c->port);
}

static void handle(struct table *t, int raw, uint16_t port, const struct udp_in *u)
{
    char out[TEXT_MAX + 16];
    struct client *c;
    int n;

    if (IS_CLOSE(u->len, u->data)) {
        if (table_remove(t, u->src.s_addr, u->sport) == 0)
            log_client("closed", u->src.s_addr, u->sport);
        return;
    }
    if (!(c = table_get(t, u->src.s_addr, u->sport, 1))) {
        perror("server: calloc");
        return;
    }
    if (c->seen == 0)
        log_client("new client", c->ip, c->port);
    c->seen = now();
    if (IS_ALIVE(u->len, u->data))
        return;
    /* Ответ — тот же текст и номер; порты в UDP-заголовке меняются местами */
    n = snprintf(out, sizeof out, "%.*s %u", (int)(u->len > TEXT_MAX ? TEXT_MAX : u->len), u->data, ++c->count);
    if (udp_send(raw, u->src, port, u->sport, out, n) < 0)
        perror("server: sendto");
}

int main(int argc, char **argv)
{
    static uint8_t buf[PKT_MAX];
    struct table t = { 0 };
    struct udp_in u;
    uint16_t port = ECHO_PORT;
    double ttl = 30, swept;
    int raw, hold, opt, rc = 0;

    while ((opt = getopt(argc, argv, "p:T:")) != -1) {
        long v = opt == '?' ? -1 : num(optarg);
        if (v < 0)
            return usage();
        if (opt == 'p') port = v;
        else ttl = v;
    }
    if (optind != argc)
        return usage();
    setvbuf(stdout, NULL, _IOLBF, 0);
    if ((raw = raw_open()) < 0) {
        fprintf(stderr, "server: socket: %s%s\n", strerror(errno), errno == EPERM ? " (needs CAP_NET_RAW)" : "");
        return 1;
    }
    if ((hold = udp_reserve(&port)) < 0) {
        fprintf(stderr, "server: port %u: %s\n", port, strerror(errno));
        close(raw);
        return 1;
    }
    install_signals();
    printf("server: listening on %u, forget clients after %.0f s\n", port, ttl);

    swept = now();
    while (!g_stop) {
        struct pollfd pfd = { .fd = raw, .events = POLLIN };
        /* Таймаут 1 с: молчащих клиентов надо вычёркивать, даже когда пакетов нет вообще */
        int r = poll(&pfd, 1, 1000);
        ssize_t n;

        if (r < 0 && errno != EINTR) {
            perror("server: poll");
            rc = 1;
            break;
        }
        if (now() - swept >= 1) {
            table_expire(&t, now(), ttl, on_gone);
            swept = now();
        }
        if (r <= 0)
            continue;
        if ((n = recv(raw, buf, sizeof buf, 0)) < 0)
            continue;
        /* Сюда приходит весь UDP машины, в том числе наши же ответы на lo: берём только свой порт */
        if (udp_parse(buf, n, &u) == 0 && u.dport == port)
            handle(&t, raw, port, &u);
    }
    printf("server: stopped, %zu clients forgotten\n", t.n);
    table_free(&t);
    close(hold);
    close(raw);
    return rc;
}
