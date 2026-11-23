#include "broker.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "registry.h"

#define SHUTDOWN_TIMEOUT_MS 3000
#define SHUTDOWN_POLL_MS    100

/* Рассылаем сообщение всем, кто подписан на его тему */
static void forward(int qid, const struct registry *reg, const struct cmd *c)
{
    struct msg out;
    size_t i;
    int len;

    len = snprintf(out.mtext, sizeof out.mtext, "%s\n%s", c->topic, c->payload);
    if (len < 0 || (size_t)len >= sizeof out.mtext) {
        fprintf(stderr, "broker: message too long, dropped\n");
        return;
    }
    for (i = 0; i < reg->nsubs; i++) {
        if (strcmp(reg->subs[i].topic, c->topic) != 0)
            continue;
        out.mtype = reg->subs[i].pid;
        /*
         * IPC_NOWAIT: если очередь полна из-за медленного подписчика, лучше потерять
         * одно сообщение, чем заблокировать брокера и остановить рассылку всем.
         */
        if (msgsnd(qid, &out, (size_t)len, IPC_NOWAIT) < 0) {
            if (errno == EAGAIN)
                fprintf(stderr, "broker: queue full, message for %d dropped\n",
                        (int)reg->subs[i].pid);
            else
                perror("broker: msgsnd");
        }
    }
}

static void handle(int qid, struct registry *reg, const char *text)
{
    struct cmd c;
    int r;

    if (parse_cmd(text, &c) < 0) {
        fprintf(stderr, "broker: malformed message ignored\n");
        return;
    }
    switch (c.kind) {
    case CMD_SUBSCRIBE:
        r = reg_add_sub(reg, c.pid, c.topic);
        if (r < 0)
            fprintf(stderr, "broker: out of memory\n");
        else if (r == 0)
            printf("broker: %d subscribed to '%s'\n", (int)c.pid, c.topic);
        break;
    case CMD_UNSUBSCRIBE:
        if (reg_del_sub(reg, c.pid, c.topic))
            printf("broker: %d unsubscribed from '%s'\n", (int)c.pid, c.topic);
        break;
    case CMD_SEND:
        /* издателя узнаём по первому его сообщению: отдельной регистрации у него нет */
        if (reg_add_pub(reg, c.pid) < 0) {
            fprintf(stderr, "broker: out of memory\n");
            break;
        }
        printf("broker: message from %d on '%s'\n", (int)c.pid, c.topic);
        forward(qid, reg, &c);
        break;
    }
}

/* Выбрасываем всё, что ещё лежит для брокера: при остановке пересылать это уже не нужно */
static void drain_broker_queue(int qid)
{
    struct msg m;

    while (msgrcv(qid, &m, sizeof m.mtext, BROKER_TYPE, IPC_NOWAIT | MSG_NOERROR) >= 0)
        ;
}

static void shutdown_broker(int qid, const struct registry *reg)
{
    struct timespec pause = { 0, SHUTDOWN_POLL_MS * 1000000L };
    struct msqid_ds ds;
    pid_t *pids;
    size_t n, i;
    int waited;

    n = reg_all_pids(reg, &pids);
    for (i = 0; i < n; i++)
        kill(pids[i], SIGINT);   /* ESRCH игнорируем: клиент мог уже завершиться */
    free(pids);

    /*
     * Ждём, пока очередь опустеет. Уходя, подписчики шлют unsubscribe с типом 1,
     * поэтому очищаем его на каждом шаге. Сообщения для уже умерших подписчиков
     * никто не заберёт, их закроет таймаут.
     */
    for (waited = 0; waited < SHUTDOWN_TIMEOUT_MS; waited += SHUTDOWN_POLL_MS) {
        drain_broker_queue(qid);
        if (msgctl(qid, IPC_STAT, &ds) < 0 || ds.msg_qnum == 0)
            break;
        nanosleep(&pause, NULL);
    }
    if (waited >= SHUTDOWN_TIMEOUT_MS)
        fprintf(stderr, "broker: queue not empty after timeout, removing anyway\n");
    /* очередь могли удалить снаружи (ipcrm): тогда удалять уже нечего */
    if (msgctl(qid, IPC_RMID, NULL) < 0 && !queue_gone(errno))
        perror("broker: msgctl(IPC_RMID)");
}

int broker_run(void)
{
    struct registry reg;
    key_t key = queue_key();
    int qid, rc = 0;

    if (key == (key_t)-1) {
        perror("ftok");
        return 1;
    }
    /* IPC_EXCL: если очередь уже есть, значит брокер запущен другим процессом */
    qid = msgget(key, IPC_CREAT | IPC_EXCL | 0600);
    if (qid < 0) {
        if (errno == EEXIST)
            fprintf(stderr, "broker is already running\n");
        else
            perror("msgget");
        return 1;
    }
    if (install_stop_handlers() < 0) {
        perror("sigaction");
        msgctl(qid, IPC_RMID, NULL);
        return 1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    reg_init(&reg);
    printf("broker: started, queue id %d\n", qid);

    while (!g_stop) {
        struct msg m;
        /* размер на единицу меньше буфера: место под '\0', которого ядро не ставит */
        ssize_t n = msgrcv(qid, &m, sizeof m.mtext - 1, BROKER_TYPE, MSG_NOERROR);

        if (n < 0) {
            if (errno == EINTR)
                continue;   /* условие цикла проверит флаг */
            perror("broker: msgrcv");
            rc = 1;
            break;
        }
        m.mtext[n] = '\0';
        handle(qid, &reg, m.mtext);
    }

    printf("broker: stopping\n");
    shutdown_broker(qid, &reg);
    reg_free(&reg);
    return rc;
}
