#include "taxi.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <unistd.h>

/* Ровно want целых чисел через пробелы и ничего после них */
static int args(const char *s, long *v, int want)
{
    char *end;

    for (int i = 0; i < want; i++, s = end) {
        errno = 0;
        v[i] = strtol(s, &end, 10);
        if (end == s || errno)
            return -1;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    return *s ? -1 : 0;
}

int parse_cmd(const char *line, struct cmd *c)
{
    static const struct { const char *name; int type, nargs; } t[] = {
        { "create_driver", C_CREATE, 0 }, { "send_task", C_TASK, 2 }, { "get_status", C_STATUS, 1 },
        { "get_drivers", C_LIST, 0 }, { "help", C_HELP, 0 }, { "exit", C_EXIT, 0 },
    };
    char w[32];
    long v[2] = { 0, 0 };
    int n = 0;

    memset(c, 0, sizeof *c);
    if (sscanf(line, " %31s%n", w, &n) != 1)
        return c->type = C_NONE;
    c->type = C_BAD;
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (strcmp(w, t[i].name) == 0) {
            if (args(line + n, v, t[i].nargs) < 0 || (t[i].nargs && v[0] <= 0) ||
                (t[i].type == C_TASK && (v[1] <= 0 || v[1] > TASK_MAX)))
                return C_BAD;
            c->type = t[i].type;
            c->pid = v[0];
            c->timer = v[1];
        }
    return c->type;
}

/* Очередь создаём и сразу удаляем имя: дескриптор живёт и наследуется через fork,
 * а в /dev/mqueue ничего не останется, даже если CLI убьют kill -9.
 * O_NONBLOCK — общий для родителя и ребёнка (одно описание открытой очереди):
 * обе стороны читают только после select, а полная очередь даёт EAGAIN, а не зависание. */
mqd_t queue_new(void)
{
    static unsigned seq;
    struct mq_attr a = { .mq_maxmsg = 10, .mq_msgsize = sizeof(struct msg) };
    char name[64];
    mqd_t q;

    snprintf(name, sizeof name, "/taxi_%d_%u", (int)getpid(), seq++);
    q = mq_open(name, O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK, 0600, &a);
    if (q != (mqd_t)-1)
        mq_unlink(name);
    return q;
}

/* Сколько секунд осталось, с округлением вверх; 0 — таймер не взведён, водитель свободен.
 * Отдельного флага «занят» нет: состояние хранит сам таймер в ядре. */
int remaining(int tfd)
{
    struct itimerspec it;

    if (timerfd_gettime(tfd, &it) < 0)
        return 0;
    return it.it_value.tv_sec + (it.it_value.tv_nsec > 0);
}

/* 1 — ответ в out, 0 — отвечать нечего, -1 — выйти */
int driver_handle(int tfd, const struct msg *in, struct msg *out)
{
    struct itimerspec it = { .it_value.tv_sec = in->value };     /* it_interval 0: сработает один раз */
    int left = remaining(tfd);

    out->pid = getpid();
    out->value = left;
    switch (in->type) {
    case M_TASK:
        if (left > 0) {
            out->type = A_BUSY;
        } else if (in->value > 0 && timerfd_settime(tfd, 0, &it, NULL) == 0) {
            out->type = A_OK;
            out->value = in->value;
        } else {
            return 0;
        }
        return 1;
    case M_STATUS:
        out->type = left > 0 ? A_BUSY : A_AVAILABLE;
        return 1;
    case M_QUIT:
        return -1;
    }
    return 0;
}

void driver_run(mqd_t cmd, mqd_t rep, pid_t parent)
{
    struct msg in, out;
    uint64_t fired;
    int tfd, r = 0;

    /* Умрёт CLI — ядро пришлёт SIGTERM. Если CLI умер ещё до prctl, родителем уже стал другой процесс */
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) < 0 || getppid() != parent)
        _exit(1);
    if ((tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) < 0)
        _exit(1);
    while (r >= 0) {
        fd_set rd;

        FD_ZERO(&rd);
        FD_SET(cmd, &rd);           /* в Linux mqd_t — обычный fd (man 7 mq_overview) */
        FD_SET(tfd, &rd);
        /* Без таймаута: процесс спит в ядре, пока не придёт команда или не сработает таймер */
        if (select((cmd > tfd ? cmd : tfd) + 1, &rd, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (FD_ISSET(tfd, &rd) && read(tfd, &fired, sizeof fired) == sizeof fired) {
            out = (struct msg){ .type = A_DONE, .pid = getpid() };
            mq_send(rep, (const char *)&out, sizeof out, 0);
        }
        if (FD_ISSET(cmd, &rd))
            while (r >= 0 && mq_receive(cmd, (char *)&in, sizeof in, NULL) == sizeof in)
                if ((r = driver_handle(tfd, &in, &out)) > 0)
                    mq_send(rep, (const char *)&out, sizeof out, 0);    /* EAGAIN: CLI не читает, ответ теряем */
    }
    close(tfd);
    _exit(0);
}
