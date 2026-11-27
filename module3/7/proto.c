#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "proto.h"

volatile sig_atomic_t g_stop;

int buf_append(struct buf *b, const void *p, size_t n)
{
    size_t cap = b->cap ? b->cap : 4096;
    char *d;

    if (n == 0)
        return 0;
    while (cap < b->len + n)
        cap *= 2;
    if (cap != b->cap) {
        d = realloc(b->data, cap);
        if (!d)
            return -1;
        b->data = d;
        b->cap = cap;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return 0;
}

/* Убирает n обработанных или отправленных байт из начала */
void buf_drop(struct buf *b, size_t n)
{
    memmove(b->data, b->data + n, b->len - n);
    b->len -= n;
}

void buf_free(struct buf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* Дописывает в out кадр: заголовок, затем части a и b подряд (b может быть пустой).
 * Две части нужны серверу: он приклеивает имя отправителя, не копируя присланное. */
int frame_put(struct buf *out, char type, const void *a, size_t alen, const void *b, size_t blen)
{
    char hdr[HDR_SIZE];
    uint32_t len = htonl(alen + blen);      /* длина в сетевом порядке: машины могут быть разные */

    if (alen + blen > FRAME_MAX) {
        errno = EMSGSIZE;
        return -1;
    }
    hdr[0] = type;
    memcpy(hdr + 1, &len, 4);               /* memcpy: hdr + 1 не выровнен для uint32_t */
    if (buf_append(out, hdr, HDR_SIZE) < 0 || buf_append(out, a, alen) < 0 ||
        (blen && buf_append(out, b, blen) < 0))
        return -1;
    return 0;
}

/* 1: в начале in целый кадр (payload указывает внутрь in), 0: ещё не всё пришло,
 * -1: длина больше допустимой (битый или враждебный собеседник). */
int frame_get(const struct buf *in, char *type, const char **payload, size_t *len)
{
    uint32_t n;

    if (in->len < HDR_SIZE)
        return 0;
    memcpy(&n, in->data + 1, 4);
    n = ntohl(n);
    if (n > FRAME_MAX)
        return -1;
    if (in->len < HDR_SIZE + n)
        return 0;
    *type = in->data[0];
    *payload = in->data + HDR_SIZE;
    *len = n;
    return 1;
}

/* Имя попадает в имя файла у получателя и в список через ", ": без '/', пробелов и управляющих */
int name_valid(const char *s, size_t n)
{
    size_t i;

    if (n == 0 || n >= NAME_LEN)
        return 0;
    for (i = 0; i < n; i++)
        if ((unsigned char)s[i] <= ' ' || s[i] == '/' || s[i] == ',' || s[i] == 0x7f)
            return 0;
    return 1;
}

/* Имя файла пришло по сети: берём только последнюю часть пути, иначе "../../.bashrc"
 * записал бы файл за пределами текущей папки. NULL, если от имени ничего не осталось. */
const char *safe_basename(const char *path)
{
    const char *p = strrchr(path, '/');

    p = p ? p + 1 : path;
    if (!*p || strcmp(p, ".") == 0 || strcmp(p, "..") == 0 || strlen(p) > 200)
        return NULL;
    return p;
}

/* Блокирующая отправка всего буфера: send может отдать в ядро только часть.
 * MSG_NOSIGNAL: если собеседник закрыл соединение, вернётся EPIPE, а не SIGPIPE,
 * который по умолчанию убивает процесс. */
int send_all(int fd, const void *p, size_t n)
{
    const char *c = p;
    ssize_t k;

    while (n > 0) {
        k = send(fd, c, n, MSG_NOSIGNAL);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        c += k;
        n -= k;
    }
    return 0;
}

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* Без SA_RESTART: poll/epoll_wait вернутся с EINTR, и цикл заметит флаг */
int install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;
    return 0;
}
