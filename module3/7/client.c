#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "proto.h"

static int sock;
static int welcomed;      /* сервер ответил на HELLO списком участников */

/* getaddrinfo понимает и IP, и имя хоста ("localhost"); пробуем адреса по очереди */
static int connect_to(const char *host, const char *port)
{
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *res, *a;
    int fd = -1, e;

    e = getaddrinfo(host, port, &hints, &res);
    if (e) {
        fprintf(stderr, "client: %s: %s\n", host, gai_strerror(e));
        return -1;
    }
    for (a = res; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd >= 0 && connect(fd, a->ai_addr, a->ai_addrlen) == 0)
            break;
        if (fd >= 0)
            close(fd);
        fd = -1;
    }
    if (fd < 0)
        perror("client: connect");
    freeaddrinfo(res);
    return fd;
}

static int send_frame(char type, const void *a, size_t alen, const void *b, size_t blen)
{
    struct buf f = { 0 };
    int rc = frame_put(&f, type, a, alen, b, blen) < 0 ? -1 : send_all(sock, f.data, f.len);

    buf_free(&f);
    return rc;
}

/* Файл целиком в память и одним кадром: проще, чем куски, ценой предела FILE_MAX */
static void send_file(const char *path)
{
    const char *base = safe_basename(path);
    struct stat st;
    char *data;
    int fd = open(path, O_RDONLY);
    ssize_t got = 0, n;

    if (fd < 0 || fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size > FILE_MAX || !base) {
        fprintf(stderr, "client: cannot send '%s' (missing, not a regular file or over %d bytes)\n",
                path, FILE_MAX);
        if (fd >= 0)
            close(fd);
        return;
    }
    data = malloc(st.st_size + 1);
    while (data && got < st.st_size && (n = read(fd, data + got, st.st_size - got)) > 0)
        got += n;
    close(fd);
    /* Отправляем только последнюю часть пути: папки отправителя получателю ни к чему */
    if (data && got == st.st_size && send_frame(T_FILE, base, strlen(base) + 1, data, got) == 0)
        printf("*** sent file '%s' (%zd bytes)\n", base, got);
    else
        perror("client: send file");
    free(data);
}

/* Имя и путь пришли по сети: basename отсекает "../", O_EXCL не даёт перезаписать файл */
static void save_file(const char *from, const char *p, size_t len)
{
    const char *zero = memchr(p, '\0', len), *base;
    char path[256];
    int fd;

    if (!zero || !(base = safe_basename(p)) || !name_valid(from, strlen(from))) {
        printf("*** %s sent a file with an unsafe name, ignored\n", from);
        return;
    }
    len -= zero - p + 1;
    snprintf(path, sizeof path, "recv_%s_%s", from, base);
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0 || write(fd, zero + 1, len) != (ssize_t)len) {
        printf("*** %s sent '%s' (%zu bytes), not saved: %s\n", from, base, len, strerror(errno));
        if (fd >= 0)
            close(fd);
        return;
    }
    close(fd);
    printf("*** %s sent '%s' (%zu bytes), saved as %s\n", from, base, len, path);
}

/* -1 = сервер отказал или прислал чужое, 0 = продолжаем */
static int show(char type, const char *p, size_t len)
{
    const char *zero = memchr(p, '\0', len);
    size_t head = zero ? (size_t)(zero - p) + 1 : 0;   /* имя отправителя с \0 */

    switch (type) {
    case T_USERS:
        if (len)
            printf("*** online: %.*s\n", (int)len, p);
        else
            printf("*** online: nobody else\n");
        welcomed = 1;
        return 0;
    case T_JOIN:
        printf("*** %.*s joined\n", (int)len, p);
        return 0;
    case T_LEAVE:
        printf("*** %.*s left\n", (int)len, p);
        return 0;
    case T_TEXT:
        if (!zero)
            return -1;
        printf("%s: %.*s\n", p, (int)(len - head), p + head);
        return 0;
    case T_FILE:
        if (!zero)
            return -1;
        save_file(p, p + head, len - head);
        return 0;
    case T_ERROR:
        fprintf(stderr, "server: %.*s\n", (int)len, p);
        return -1;
    }
    return -1;
}

/* 0 = продолжаем, 1 = сервер закрыл соединение, -1 = ошибка или отказ сервера */
static int on_socket(struct buf *in)
{
    char tmp[65536], type;
    const char *p;
    size_t len;
    ssize_t n = recv(sock, tmp, sizeof tmp, 0);
    int r;

    if (n < 0)
        return errno == EINTR ? 0 : (perror("client: recv"), -1);
    if (n == 0) {
        printf("*** server closed the connection\n");
        return 1;
    }
    if (buf_append(in, tmp, n) < 0)
        return -1;
    while ((r = frame_get(in, &type, &p, &len)) == 1) {
        if (show(type, p, len) < 0)
            return -1;
        buf_drop(in, HDR_SIZE + len);
    }
    return r;   /* 0 или -1 при битой длине */
}

/* 1 = конец ввода или ошибка отправки */
static int on_stdin(void)
{
    char line[TEXT_MAX + 1];
    size_t len;

    errno = 0;      /* fgets не сбрасывает errno при конце ввода, а нам нужно отличить его от EINTR */
    if (!fgets(line, sizeof line, stdin)) {
        if (errno == EINTR) {
            clearerr(stdin);
            return 0;
        }
        return 1;
    }
    len = strcspn(line, "\n");
    line[len] = '\0';
    if (strncmp(line, "/file ", 6) == 0) {
        send_file(line + 6);
        return 0;
    }
    if (len > 0 && send_frame(T_TEXT, line, len, NULL, 0) < 0) {
        perror("client: send");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct pollfd fds[2] = { { .fd = STDIN_FILENO, .events = POLLIN }, { .events = POLLIN } };
    struct buf in = { 0 };
    const char *port = argc > 3 ? argv[3] : NULL;
    char def[8];
    int rc = 0, r;

    /* stdin без буфера: иначе fgets заберёт несколько строк сразу, а poll их не увидит */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3 || argc > 4 || !name_valid(argv[1], strlen(argv[1]))) {
        fprintf(stderr, "usage: %s name host [port]   (name: 1..%d chars, no spaces, '/' or ','; port %d)\n",
                argv[0], NAME_LEN - 1, PROTO_PORT);
        return 2;
    }
    if (!port) {
        snprintf(def, sizeof def, "%d", PROTO_PORT);
        port = def;
    }
    sock = connect_to(argv[2], port);
    if (sock < 0)
        return 1;
    if (install_signals() < 0 || send_frame(T_HELLO, argv[1], strlen(argv[1]), NULL, 0) < 0) {
        perror("client: start");
        close(sock);
        return 1;
    }
    /* Ждём ответа на HELLO до чтения клавиатуры: отказ ("name is taken") виден всегда,
     * даже если ввод уже кончился, а список участников печатается первым */
    while (!g_stop && !welcomed && (r = on_socket(&in)) == 0)
        ;
    if (!welcomed) {
        buf_free(&in);
        close(sock);
        return g_stop ? 0 : 1;
    }
    fds[1].fd = sock;
    while (!g_stop) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            perror("client: poll");
            rc = 1;
            break;
        }
        /* Сокет первым: сообщение об отказе сервера важнее очередной строки */
        if (fds[1].revents && (r = on_socket(&in)) != 0) {
            rc = r < 0;
            break;
        }
        if (fds[0].revents & (POLLERR | POLLNVAL))
            break;          /* stdin закрыт: иначе poll возвращался бы сразу и крутил цикл */
        if ((fds[0].revents & (POLLIN | POLLHUP)) && on_stdin())
            break;
    }
    buf_free(&in);
    close(sock);    /* сервер увидит recv = 0 и разошлёт LEAVE */
    return rc;
}
