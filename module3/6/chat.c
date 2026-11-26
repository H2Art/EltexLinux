#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>

/* В обработчике сигнала безопасно только выставить флаг */
static volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

int chat_install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART не ставим: poll и чтение stdin должны вернуться с EINTR, чтобы мы заметили флаг */
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;
    return 0;
}

/* Пробел разделяет поля датаграммы, поэтому в имени его быть не может */
int chat_name_valid(const char *name)
{
    size_t n = strlen(name);

    return n > 0 && n < CHAT_NAME_MAX && !strpbrk(name, " \n");
}

/* Датаграмма — текст "T 1a2b3c4d имя[ текст]". Текст, а не struct: не нужно думать
 * о порядке байт и выравнивании на разных машинах, и его видно в tcpdump. */
int chat_format(char *buf, size_t size, char type, uint32_t id, const char *name, const char *text)
{
    int n;

    if (!chat_name_valid(name) || strlen(text) >= CHAT_TEXT_MAX)
        return -1;
    n = snprintf(buf, size, *text ? "%c %08x %s %s" : "%c %08x %s", type, id, name, text);
    return n < 0 || (size_t)n >= size ? -1 : n;
}

/* recvfrom не ставит \0, и прийти может что угодно: проверяем каждое поле */
int chat_parse(const char *buf, size_t len, struct chat_msg *m)
{
    char tmp[CHAT_DGRAM_MAX + 1], *p, *end;
    size_t n;

    if (len < 12 || len > CHAT_DGRAM_MAX)
        return -1;
    memcpy(tmp, buf, len);
    tmp[len] = '\0';
    m->type = tmp[0];
    if ((m->type != MSG_JOIN && m->type != MSG_TEXT && m->type != MSG_LEAVE) || tmp[1] != ' ')
        return -1;
    errno = 0;
    m->id = strtoul(tmp + 2, &end, 16);
    if (errno || end != tmp + 10 || *end != ' ')
        return -1;
    p = end + 1;
    n = strcspn(p, " ");
    if (n == 0 || n >= CHAT_NAME_MAX)
        return -1;
    memcpy(m->name, p, n);
    m->name[n] = '\0';
    p += n;
    if (*p == ' ')
        p++;
    if (strlen(p) >= CHAT_TEXT_MAX)
        return -1;
    strcpy(m->text, p);
    return 0;
}

int chat_open(struct chat *c, const char *name, const char *bcast, uint16_t port)
{
    struct sockaddr_in me = { .sin_family = AF_INET, .sin_port = htons(port),
                              .sin_addr.s_addr = htonl(INADDR_ANY) };
    int on = 1, e;

    if (!chat_name_valid(name)) {
        errno = EINVAL;
        return -1;
    }
    snprintf(c->name, sizeof c->name, "%s", name);
    memset(&c->dst, 0, sizeof c->dst);
    c->dst.sin_family = AF_INET;
    c->dst.sin_port = htons(port);          /* в сети порядок байт big-endian, на x86 little-endian */
    if (inet_pton(AF_INET, bcast, &c->dst.sin_addr) != 1) {
        errno = EINVAL;
        return -1;
    }
    /* Случайный id из ядра: сервера, который выдал бы номер по порядку, у равноправных клиентов нет */
    if (getrandom(&c->id, sizeof c->id, 0) != sizeof c->id)
        return -1;
    c->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (c->sock < 0)
        return -1;
    /* SO_REUSEADDR: несколько клиентов на одной машине слушают один порт, broadcast получит каждый.
     * SO_BROADCAST: без него sendto на широковещательный адрес вернёт EACCES. */
    if (setsockopt(c->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0 ||
        setsockopt(c->sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) < 0 ||
        bind(c->sock, (struct sockaddr *)&me, sizeof me) < 0) {
        e = errno;
        close(c->sock);
        c->sock = -1;
        errno = e;
        return -1;
    }
    return 0;
}

void chat_close(struct chat *c)
{
    if (c->sock >= 0)
        close(c->sock);
    c->sock = -1;
}

int chat_send(struct chat *c, char type, const char *text)
{
    char buf[CHAT_DGRAM_MAX];
    int n = chat_format(buf, sizeof buf, type, c->id, c->name, text);

    if (n < 0) {
        errno = EMSGSIZE;
        return -1;
    }
    /* Один sendto = одна датаграмма: границы сообщений UDP сохраняет сам */
    return sendto(c->sock, buf, n, 0, (struct sockaddr *)&c->dst, sizeof c->dst) < 0 ? -1 : 0;
}

/* 1 = чужое сообщение в m, 0 = своё эхо или мусор (пропустить), -1 = ошибка */
int chat_recv(struct chat *c, struct chat_msg *m, struct sockaddr_in *src)
{
    char buf[CHAT_DGRAM_MAX];
    socklen_t slen = sizeof *src;
    ssize_t n = recvfrom(c->sock, buf, sizeof buf, 0, (struct sockaddr *)src, &slen);

    if (n < 0)
        return errno == EINTR ? 0 : -1;
    if (chat_parse(buf, n, m) < 0)
        return 0;       /* на порт может прийти что угодно, не только от нас */
    /* broadcast возвращается и отправителю. Своим считаем только при совпадении id и имени:
     * если у двоих случайно совпадёт id, они не станут молча невидимы друг для друга. */
    return m->id != c->id || strcmp(m->name, c->name) != 0;
}

static int show_one(struct chat *c)
{
    struct chat_msg m;
    struct sockaddr_in src;
    char ip[INET_ADDRSTRLEN];
    int r = chat_recv(c, &m, &src);

    if (r <= 0)
        return r;
    if (m.type == MSG_JOIN)
        printf("*** %s joined (%s)\n", m.name, inet_ntop(AF_INET, &src.sin_addr, ip, sizeof ip));
    else if (m.type == MSG_LEAVE)
        printf("*** %s left\n", m.name);
    else
        printf("%s: %s\n", m.name, m.text);
    return 0;
}

/* 1 = ввод закончился, 0 = продолжаем */
static int send_line(struct chat *c)
{
    char line[CHAT_TEXT_MAX];
    size_t len;

    errno = 0;   /* fgets не сбрасывает errno при конце ввода, а нам нужно отличить его от EINTR */
    if (!fgets(line, sizeof line, stdin)) {
        if (errno == EINTR) {
            clearerr(stdin);
            return 0;   /* условие цикла проверит флаг */
        }
        return 1;
    }
    len = strcspn(line, "\n");
    line[len] = '\0';
    /* Ошибка отправки (например, нет сети) не повод выходить: сообщаем и ждём дальше */
    if (len > 0 && chat_send(c, MSG_TEXT, line) < 0)
        perror("sendto");
    return 0;
}

int chat_run(struct chat *c)
{
    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = c->sock, .events = POLLIN },    /* сокет тоже файловый дескриптор */
    };
    int rc = 0;

    if (chat_send(c, MSG_JOIN, "") < 0)
        perror("sendto(join)");
    while (!g_stop) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            rc = 1;
            break;
        }
        if ((fds[1].revents & POLLIN) && show_one(c) < 0) {
            perror("recvfrom");
            rc = 1;
            break;
        }
        if ((fds[0].revents & (POLLIN | POLLHUP)) && send_line(c))
            break;
    }
    /* Подтверждения нет: если LEAVE потеряется, остальные о выходе не узнают */
    if (chat_send(c, MSG_LEAVE, "") < 0)
        perror("sendto(leave)");
    return rc;
}
