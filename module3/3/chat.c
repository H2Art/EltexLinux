#include "chat.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
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

int chat_names(const char *base, char *n1, char *n2, size_t size)
{
    int a, b;

    if (*base == '/')
        base++;
    /* после ведущего слэша других слэшей в имени POSIX-очереди быть не может */
    if (*base == '\0' || strchr(base, '/'))
        return -1;
    a = snprintf(n1, size, "/%s_1", base);
    b = snprintf(n2, size, "/%s_2", base);
    if (a < 0 || b < 0 || (size_t)a >= size || (size_t)b >= size)
        return -1;
    return 0;
}

int chat_open(struct chat *c)
{
    struct mq_attr attr = { .mq_maxmsg = CHAT_MAX_MSGS, .mq_msgsize = CHAT_MSG_SIZE };
    mqd_t q1, q2;
    int e;

    c->in = c->out = (mqd_t)-1;

    /* O_EXCL: из двух одновременных запусков создать очередь сможет только один, он и создатель */
    q1 = mq_open(c->name1, O_CREAT | O_EXCL | O_RDONLY, 0600, &attr);
    if (q1 != (mqd_t)-1) {
        c->creator = 1;
    } else if (errno == EEXIST) {
        c->creator = 0;
        /* O_NONBLOCK на запись: если собеседник не читает, не зависаем, а сообщаем об этом */
        q1 = mq_open(c->name1, O_WRONLY | O_NONBLOCK);
    }
    if (q1 == (mqd_t)-1)
        return -1;

    /* O_CREAT без O_EXCL: создатель мог ещё не дойти до второй очереди, тогда её создаст второй */
    q2 = mq_open(c->name2, O_CREAT | (c->creator ? O_WRONLY | O_NONBLOCK : O_RDONLY), 0600, &attr);
    if (q2 == (mqd_t)-1) {
        e = errno;
        mq_close(q1);
        if (c->creator)
            mq_unlink(c->name1);
        errno = e;
        return -1;
    }
    c->in = c->creator ? q1 : q2;
    c->out = c->creator ? q2 : q1;
    return 0;
}

void chat_close(struct chat *c)
{
    if (c->in != (mqd_t)-1)
        mq_close(c->in);
    if (c->out != (mqd_t)-1)
        mq_close(c->out);
    /* Удаляем только имена: собеседник со своими дескрипторами ещё дочитает "ухожу" */
    if (c->creator) {
        mq_unlink(c->name1);
        mq_unlink(c->name2);
    }
}

/* 1 = собеседник ушёл, 0 = обычное сообщение, -1 = ошибка */
static int receive_one(mqd_t in)
{
    char buf[CHAT_MSG_SIZE];   /* буфер не меньше mq_msgsize, иначе mq_receive вернёт EMSGSIZE */
    unsigned prio;
    ssize_t n = mq_receive(in, buf, sizeof buf, &prio);

    if (n < 0)
        return errno == EINTR ? 0 : -1;
    if (prio == PRIO_BYE) {
        printf("peer left the chat\n");
        return 1;
    }
    printf("peer: %.*s\n", (int)n, buf);
    return 0;
}

/* 1 = ввод закончился, 0 = продолжаем, -1 = ошибка */
static int send_line(mqd_t out)
{
    char line[CHAT_MSG_SIZE];
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
    if (len == 0)
        return 0;
    if (mq_send(out, line, len, PRIO_TEXT) < 0) {
        if (errno == EAGAIN)
            fprintf(stderr, "peer's queue is full, message not sent\n");
        else if (errno != EINTR) {
            perror("mq_send");
            return -1;
        }
    }
    return 0;
}

int chat_run(struct chat *c)
{
    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = c->in, .events = POLLIN },   /* в Linux mqd_t это файловый дескриптор */
    };
    int rc = 0, peer_left = 0, r;

    while (!g_stop) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            rc = 1;
            break;
        }
        /* сначала очередь: если собеседник ушёл, читать клавиатуру уже незачем */
        if (fds[1].revents & POLLIN) {
            r = receive_one(c->in);
            if (r != 0) {
                if (r < 0) {
                    perror("mq_receive");
                    rc = 1;
                }
                peer_left = r > 0;
                break;
            }
        }
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            r = send_line(c->out);
            if (r != 0) {
                rc = r < 0;
                break;
            }
        }
    }

    /* Ctrl+C, конец ввода или ошибка: предупреждаем собеседника. Если ушёл он, отвечать некому. */
    if (!peer_left && mq_send(c->out, "", 0, PRIO_BYE) < 0 && errno != EAGAIN)
        perror("mq_send(bye)");
    return rc;
}
