#define _GNU_SOURCE     /* accept4 — расширение Linux (man 2 accept) */
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proto.h"

#define MAX_CLIENTS 64
#define OUT_LIMIT   (8 << 20)   /* очередь больше 8 МиБ: клиент безнадёжно отстал */

struct client {
    int fd;                     /* -1: слот свободен */
    int dead;                   /* отключить после обработки текущих событий */
    int want_out;               /*  OUT сейчас включён */
    char name[NAME_LEN];        /* пусто, пока не пришёл HELLO */
    struct buf in, out;
};

static struct client clients[MAX_CLIENTS];
static int epfd;

static void set_events(struct client *c)
{
    struct epoll_event ev = { .events = EPOLLIN | (c->out.len ? EPOLLOUT : 0), .data.ptr = c };

    if ((c->out.len > 0) != c->want_out) {
        c->want_out = c->out.len > 0;
        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
    }
}

/* Отдаёт ядру сколько примет. Сокет неблокирующий: остаток ждёт EPOLLOUT,
 * а сервер тем временем обслуживает остальных. */
static void flush(struct client *c)
{
    ssize_t n;

    while (c->out.len > 0) {
        n = send(c->fd, c->out.data, c->out.len, MSG_NOSIGNAL);
        if (n > 0) {
            buf_drop(&c->out, n);
        } else if (errno == EINTR) {
            continue;
        } else {
            if (errno != EAGAIN)
                c->dead = 1;    /* EPIPE, ECONNRESET: клиента уже нет */
            break;
        }
    }
    set_events(c);
}

static void queue(struct client *c, const struct buf *frame)
{
    if (c->dead)
        return;
    if (c->out.len + frame->len > OUT_LIMIT || buf_append(&c->out, frame->data, frame->len) < 0) {
        fprintf(stderr, "server: dropping slow client %s\n", c->name);
        c->dead = 1;
        return;
    }
    flush(c);
}

/* Всем представившимся, кроме from. Кадр собираем один раз, копию кладём в очередь каждому. */
static void broadcast(struct client *from, char type, const void *a, size_t alen,
                      const void *b, size_t blen)
{
    struct buf f = { 0 };
    int i;

    if (frame_put(&f, type, a, alen, b, blen) == 0)
        for (i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd >= 0 && clients[i].name[0] && &clients[i] != from)
                queue(&clients[i], &f);
    buf_free(&f);
}

static void reply(struct client *c, char type, const char *text)
{
    struct buf f = { 0 };

    if (frame_put(&f, type, text, strlen(text), NULL, 0) == 0)
        queue(c, &f);
    buf_free(&f);
}

static int name_taken(const char *name)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd >= 0 && strcmp(clients[i].name, name) == 0)
            return 1;
    return 0;
}

static void hello(struct client *c, const char *p, size_t len)
{
    char name[NAME_LEN], list[MAX_CLIENTS * (NAME_LEN + 2)] = "";
    int i;

    if (!name_valid(p, len)) {
        reply(c, T_ERROR, "invalid name");
        c->dead = 1;
        return;
    }
    memcpy(name, p, len);       /* в кадре имя без \0 */
    name[len] = '\0';
    if (name_taken(name)) {     /* у самого c имя ещё пустое, с собой не совпадёт */
        reply(c, T_ERROR, "name is taken");
        c->dead = 1;
        return;
    }
    strcpy(c->name, name);
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd >= 0 && clients[i].name[0] && &clients[i] != c) {
            if (list[0])
                strcat(list, ", ");
            strcat(list, clients[i].name);
        }
    reply(c, T_USERS, list);
    broadcast(c, T_JOIN, c->name, strlen(c->name), NULL, 0);
    printf("server: %s joined\n", c->name);
}

static void handle(struct client *c, char type, const char *p, size_t len)
{
    const char *zero;

    if (!c->name[0]) {          /* первым кадром обязан быть HELLO */
        if (type == T_HELLO)
            hello(c, p, len);
        else
            c->dead = 1;
        return;
    }
    if (type == T_TEXT && len <= TEXT_MAX) {
        broadcast(c, T_TEXT, c->name, strlen(c->name) + 1, p, len);
    } else if (type == T_FILE && (zero = memchr(p, '\0', len)) != NULL) {
        printf("server: %s sent file '%s' (%zu bytes)\n", c->name, p, len - (zero - p) - 1);
        broadcast(c, T_FILE, c->name, strlen(c->name) + 1, p, len);
    } else {
        c->dead = 1;            /* чужой или битый протокол */
    }
}

/* Уровневый режим epoll (по умолчанию): одного recv на событие хватает,
 * если данные остались, epoll_wait сообщит снова. */
static void on_read(struct client *c)
{
    char tmp[65536], type;
    const char *p;
    size_t len;
    ssize_t n = recv(c->fd, tmp, sizeof tmp, 0);
    int r;

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
        c->dead = 1;            /* 0: клиент закрыл соединение */
        return;
    }
    if (n < 0 || buf_append(&c->in, tmp, n) < 0) {
        if (n > 0)
            c->dead = 1;
        return;
    }
    /* В буфере может оказаться и полкадра, и несколько кадров сразу */
    while (!c->dead && (r = frame_get(&c->in, &type, &p, &len)) != 0) {
        if (r < 0) {
            c->dead = 1;
            break;
        }
        handle(c, type, p, len);
        buf_drop(&c->in, HDR_SIZE + len);
    }
}

static void drop(struct client *c)
{
    char name[NAME_LEN];

    strcpy(name, c->name);
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);   /* close убрал бы и сам, но явно надёжнее (man 7 epoll) */
    close(c->fd);
    buf_free(&c->in);
    buf_free(&c->out);
    memset(c, 0, sizeof *c);
    c->fd = -1;
    if (name[0]) {
        printf("server: %s left\n", name);
        broadcast(NULL, T_LEAVE, name, strlen(name), NULL, 0);
    }
}

/* Отключаем после обработки всех событий пачки: так событие, пришедшее следом,
 * не обратится к уже освобождённому клиенту. LEAVE может пометить ещё кого-то, поэтому повторяем. */
static void reap(void)
{
    int i, again = 1;

    while (again) {
        again = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd >= 0 && clients[i].dead) {
                drop(&clients[i]);
                again = 1;
            }
    }
}

static void on_accept(int lfd)
{
    struct epoll_event ev = { .events = EPOLLIN };
    int fd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK), i;

    if (fd < 0)
        return;
    for (i = 0; i < MAX_CLIENTS && clients[i].fd >= 0; i++)
        ;
    if (i == MAX_CLIENTS) {
        close(fd);
        return;
    }
    clients[i].fd = fd;
    ev.data.ptr = &clients[i];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        clients[i].fd = -1;
    }
}

static int listen_on(unsigned port)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port),
                             .sin_addr.s_addr = htonl(INADDR_ANY) };
    int fd = socket(AF_INET, SOCK_STREAM, 0), on = 1;

    /* SO_REUSEADDR: иначе после перезапуска bind около минуты падает с EADDRINUSE
     * из-за соединений в TIME_WAIT. Флаг нужен и старому серверу, поэтому ставим всегда. */
    if (fd < 0 || setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0 ||
        bind(fd, (struct sockaddr *)&a, sizeof a) < 0 || listen(fd, 16) < 0) {
        if (fd >= 0)
            close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[])
{
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL }, evs[16];
    unsigned long port = PROTO_PORT;
    char *end;
    int lfd, n, i, rc = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc > 2 || (argc == 2 && ((port = strtoul(argv[1], &end, 10)) == 0 || *end || port > 65535))) {
        fprintf(stderr, "usage: %s [port]   (default %d)\n", argv[0], PROTO_PORT);
        return 2;
    }
    for (i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
    lfd = listen_on(port);
    if (lfd < 0) {
        perror("server: listen");
        return 1;
    }
    /* epoll_create1(0): у epoll_create размер игнорируется, но обязан быть > 0 */
    epfd = epoll_create1(0);
    if (epfd < 0 || epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev) < 0 || install_signals() < 0) {
        perror("server: epoll");
        return 1;
    }
    printf("server: listening on port %lu\n", port);

    while (!g_stop) {
        n = epoll_wait(epfd, evs, 16, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("server: epoll_wait");
            rc = 1;
            break;
        }
        for (i = 0; i < n; i++) {
            struct client *c = evs[i].data.ptr;

            if (!c) {               /* ptr = NULL у слушающего сокета */
                on_accept(lfd);
                continue;
            }
            if (c->fd < 0 || c->dead)
                continue;
            if (evs[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
                on_read(c);         /* при HUP/ERR recv вернёт 0 или ошибку */
            if (!c->dead && (evs[i].events & EPOLLOUT))
                flush(c);
        }
        reap();
    }
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd >= 0) {
            close(clients[i].fd);   /* клиенты увидят recv = 0 */
            buf_free(&clients[i].in);
            buf_free(&clients[i].out);
        }
    close(lfd);
    close(epfd);
    printf("server: stopped\n");
    return rc;
}
