#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "rawudp.h"

static int usage(void)
{
    fprintf(stderr, "usage: client [-p server_port] [-b my_port] [-k alive_seconds] server_ip\n");
    return 2;
}

/* 1 — ввод кончился, 0 — продолжаем, -1 — ошибка отправки */
static int on_stdin(int raw, struct in_addr srv, uint16_t me, uint16_t sport)
{
    char line[TEXT_MAX + 2];
    size_t len;

    errno = 0;
    if (!fgets(line, sizeof line, stdin)) {
        if (errno == EINTR) {
            clearerr(stdin);
            return 0;               /* сигнал: цикл увидит g_stop */
        }
        return 1;
    }
    len = strcspn(line, "\n");
    if (len == 0)
        return 0;                   /* пустая датаграмма означает закрытие, пустую строку не шлём */
    return udp_send(raw, srv, me, sport, line, len);
}

static void on_socket(int raw, struct in_addr srv, uint16_t me, uint16_t sport)
{
    static uint8_t buf[PKT_MAX];
    struct udp_in u;
    ssize_t n = recv(raw, buf, sizeof buf, 0);

    /* RAW-сокет получает весь UDP машины: ответ сервера узнаём по адресу и обоим портам */
    if (n > 0 && udp_parse(buf, n, &u) == 0 && u.src.s_addr == srv.s_addr &&
        u.sport == sport && u.dport == me && u.len > 0)
        printf("%.*s\n", (int)u.len, (const char *)u.data);
}

int main(int argc, char **argv)
{
    struct in_addr srv;
    uint16_t sport = ECHO_PORT, me = 0;
    long alive = 5, v;
    double last;
    int raw, hold, opt, r, rc = 0;

    while ((opt = getopt(argc, argv, "p:b:k:")) != -1) {
        if (opt == '?' || (v = num(optarg)) < 0)
            return usage();
        if (opt == 'p') sport = v;
        else if (opt == 'b') me = v;
        else alive = v;
    }
    if (optind != argc - 1 || inet_pton(AF_INET, argv[optind], &srv) != 1)
        return usage();
    setvbuf(stdin, NULL, _IONBF, 0);    /* иначе fgets заберёт в буфер больше строки, и poll о ней не узнает */
    setvbuf(stdout, NULL, _IOLBF, 0);
    if ((raw = raw_open()) < 0) {
        fprintf(stderr, "client: socket: %s%s\n", strerror(errno), errno == EPERM ? " (needs CAP_NET_RAW)" : "");
        return 1;
    }
    if ((hold = udp_reserve(&me)) < 0) {
        fprintf(stderr, "client: port %u: %s\n", me, strerror(errno));
        close(raw);
        return 1;
    }
    install_signals();
    fprintf(stderr, "client: port %u -> %s:%u\n", me, argv[optind], sport);

    last = now();
    while (!g_stop) {
        struct pollfd fds[2] = { { .fd = 0, .events = POLLIN }, { .fd = raw, .events = POLLIN } };
        /* Спим до ввода, ответа или до момента, когда пора сказать «я жив» */
        double left = last + alive - now();

        r = poll(fds, 2, left > 0 ? (int)(left * 1000) + 1 : 0);
        if (r < 0 && errno != EINTR) {
            perror("client: poll");
            rc = 1;
            break;
        }
        if (now() - last >= alive) {
            udp_send(raw, srv, me, sport, "", 1);       /* один байт \0 */
            last = now();
        }
        if (r <= 0)
            continue;
        if (fds[1].revents & POLLIN)
            on_socket(raw, srv, me, sport);
        if (fds[0].revents & (POLLERR | POLLNVAL))
            break;
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            if ((r = on_stdin(raw, srv, me, sport)) > 0)
                break;
            if (r < 0)
                perror("client: sendto");       /* сеть может вернуться: не выходим */
            last = now();
        }
    }
    /* Штатный выход, в том числе по Ctrl+C и SIGTERM: сервер обнуляет наш счётчик */
    if (udp_send(raw, srv, me, sport, NULL, 0) < 0)
        perror("client: sendto close");
    close(hold);
    close(raw);
    return rc;
}
