#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include "taxi.h"

struct driver {
    pid_t pid;
    mqd_t cmd, rep;         /* команды ему и ответы от него: своя пара очередей у каждого */
};

static struct driver d[MAX_DRIVERS];
static int nd;
static volatile sig_atomic_t g_stop;

/* SIGCHLD тоже нужен обработчик: при SIG_DFL сигнал игнорируется и pselect не прервётся */
static void on_sig(int sig)
{
    if (sig != SIGCHLD)
        g_stop = 1;
}

static struct driver *find(pid_t pid)
{
    for (int i = 0; i < nd; i++)
        if (d[i].pid == pid)
            return &d[i];
    printf("no driver %d\n", (int)pid);
    return NULL;
}

static void send_cmd(const struct driver *x, int type, int value)
{
    struct msg m = { .type = type, .value = value };

    if (mq_send(x->cmd, (const char *)&m, sizeof m, 0) < 0)
        printf("driver %d: %s\n", (int)x->pid, errno == EAGAIN ? "queue is full, try later" : strerror(errno));
}

static void create(const sigset_t *orig)
{
    pid_t parent = getpid(), pid;
    mqd_t cmd, rep = (mqd_t)-1;

    if (nd == MAX_DRIVERS) {
        printf("too many drivers (%d)\n", MAX_DRIVERS);
        return;
    }
    if ((cmd = queue_new()) == (mqd_t)-1 || (rep = queue_new()) == (mqd_t)-1 || (pid = fork()) < 0) {
        perror("create_driver");
        if (cmd != (mqd_t)-1) mq_close(cmd);
        if (rep != (mqd_t)-1) mq_close(rep);
        return;
    }
    if (pid == 0) {
        /* Ctrl+C терминал шлёт всей группе процессов: водитель его игнорирует и уходит по QUIT от CLI */
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        sigprocmask(SIG_SETMASK, orig, NULL);
        for (int i = 0; i < nd; i++) {      /* очереди других водителей ему не нужны */
            mq_close(d[i].cmd);
            mq_close(d[i].rep);
        }
        driver_run(cmd, rep, parent);
    }
    d[nd++] = (struct driver){ pid, cmd, rep };
    printf("driver %d created\n", (int)pid);
}

static void on_reply(const struct driver *x)
{
    struct msg m;

    while (mq_receive(x->rep, (char *)&m, sizeof m, NULL) == sizeof m) {
        if (m.type == A_OK)
            printf("driver %d: got a task for %d s\n", (int)m.pid, m.value);
        else if (m.type == A_BUSY)
            printf("driver %d: Busy %d\n", (int)m.pid, m.value);
        else if (m.type == A_AVAILABLE)
            printf("driver %d: Available\n", (int)m.pid);
        else if (m.type == A_DONE)
            printf("driver %d: task done, Available\n", (int)m.pid);
    }
}

/* Забрать завершившихся водителей: иначе они останутся зомби, а их очереди открытыми */
static void reap(void)
{
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        for (int i = 0; i < nd; i++)
            if (d[i].pid == pid) {
                on_reply(&d[i]);            /* что успел ответить перед смертью */
                mq_close(d[i].cmd);
                mq_close(d[i].rep);
                d[i] = d[--nd];
                printf("driver %d exited\n", (int)pid);
                break;
            }
}

/* 1 — выход из CLI */
static int do_line(const char *line, const sigset_t *orig)
{
    struct cmd c;
    struct driver *x;

    switch (parse_cmd(line, &c)) {
    case C_CREATE:
        create(orig);
        break;
    case C_TASK:
        if ((x = find(c.pid)))
            send_cmd(x, M_TASK, c.timer);
        break;
    case C_STATUS:
        if ((x = find(c.pid)))
            send_cmd(x, M_STATUS, 0);
        break;
    case C_LIST:
        printf("%d drivers\n", nd);
        for (int i = 0; i < nd; i++)
            send_cmd(&d[i], M_STATUS, 0);   /* ответы придут асинхронно, каждый со своим pid */
        break;
    case C_HELP:
        printf("create_driver | send_task <pid> <seconds> | get_status <pid> | get_drivers | exit\n");
        break;
    case C_EXIT:
        return 1;
    case C_BAD:
        printf("bad command, try help\n");
        break;
    }
    return 0;
}

int main(void)
{
    struct sigaction sa = { .sa_handler = on_sig };
    sigset_t block, orig;
    char line[256];
    int tty = isatty(0), rc = 0;

    setvbuf(stdin, NULL, _IONBF, 0);    /* иначе fgets заберёт в буфер несколько строк, и select о них не узнает */
    setvbuf(stdout, NULL, _IOLBF, 0);
    /* Сигналы заблокированы всё время, кроме ожидания в pselect: он снимает блокировку атомарно.
     * С обычным select сигнал между проверкой g_stop и вызовом select ждал бы следующего события. */
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTERM);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &orig);
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    while (!g_stop) {
        fd_set rd;
        int maxfd = 0;

        reap();
        if (tty) {
            printf("> ");
            fflush(stdout);
        }
        FD_ZERO(&rd);
        FD_SET(0, &rd);
        for (int i = 0; i < nd; i++) {
            FD_SET(d[i].rep, &rd);
            if (d[i].rep > maxfd)
                maxfd = d[i].rep;
        }
        if (pselect(maxfd + 1, &rd, NULL, NULL, NULL, &orig) < 0) {
            if (errno == EINTR)
                continue;
            perror("pselect");
            rc = 1;
            break;
        }
        for (int i = 0; i < nd; i++)
            if (FD_ISSET(d[i].rep, &rd))
                on_reply(&d[i]);
        if (FD_ISSET(0, &rd) && (!fgets(line, sizeof line, stdin) || do_line(line, &orig)))
            break;
    }
    for (int i = 0; i < nd; i++) {
        struct msg m = { .type = M_QUIT };
        if (mq_send(d[i].cmd, (const char *)&m, sizeof m, 0) < 0)
            kill(d[i].pid, SIGTERM);        /* очередь полна — не ждём, пока разберёт */
    }
    for (int i = 0; i < nd; i++) {
        waitpid(d[i].pid, NULL, 0);
        mq_close(d[i].cmd);
        mq_close(d[i].rep);
    }
    printf("cli: stopped, %d drivers finished\n", nd);
    return rc;
}
