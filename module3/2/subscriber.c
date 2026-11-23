#include "subscriber.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

#include "common.h"

/* Брокер присылает "тема\nсодержимое"; выводим как "тема: содержимое" */
static void print_message(const char *text)
{
    const char *nl = strchr(text, '\n');

    if (nl)
        printf("%.*s: %s\n", (int)(nl - text), text, nl + 1);
    else
        printf("%s\n", text);
}

/* Читаем только сообщения со своим pid в роли типа: чужие нам не видны */
static int receive_loop(int qid, int *gone)
{
    long me = (long)getpid();
    struct msg m;

    while (!g_stop) {
        /* размер на единицу меньше буфера: место под '\0', которого ядро не ставит */
        ssize_t n = msgrcv(qid, &m, sizeof m.mtext - 1, me, MSG_NOERROR);

        if (n < 0) {
            if (errno == EINTR)
                continue;   /* условие цикла проверит флаг */
            if (queue_gone(errno)) {
                fprintf(stderr, "subscriber: queue is gone\n");
                *gone = 1;
                return 0;
            }
            perror("subscriber: msgrcv");
            return 1;
        }
        m.mtext[n] = '\0';
        print_message(m.mtext);
    }
    return 0;
}

int subscriber_run(char *const topics[], int n)
{
    int qid, i, rc = 0, gone = 0;

    for (i = 0; i < n; i++) {
        if (!topic_valid(topics[i])) {
            fprintf(stderr, "invalid topic '%s'\n", topics[i]);
            return 2;
        }
    }
    /* без IPC_CREAT: очередь создаёт только брокер */
    qid = msgget(queue_key(), 0);
    if (qid < 0) {
        if (errno == ENOENT)
            fprintf(stderr, "broker is not running\n");
        else
            perror("msgget");
        return 1;
    }
    if (install_stop_handlers() < 0) {
        perror("sigaction");
        return 1;
    }
    /* вывод в файл или канал иначе копился бы в буфере, а не показывался сразу */
    setvbuf(stdout, NULL, _IOLBF, 0);

    for (i = 0; i < n; i++) {
        if (send_cmd(qid, CMD_SUBSCRIBE, topics[i], NULL) < 0) {
            gone = queue_gone(errno);
            if (errno != EINTR) {   /* EINTR значит "просили завершиться", это не ошибка */
                perror("subscriber: subscribe");
                rc = 1;
            }
            break;
        }
    }
    if (rc == 0 && !gone && !g_stop)
        rc = receive_loop(qid, &gone);

    /* Брокеру нужно сообщить об уходе, пока очередь жива: при его остановке она ещё
     * существует, пока он ждёт, и отписка успевает дойти */
    for (i = 0; i < n && !gone; i++)
        if (send_cmd(qid, CMD_UNSUBSCRIBE, topics[i], NULL) < 0)
            gone = queue_gone(errno);
    return rc;
}
