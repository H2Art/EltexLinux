#include "publisher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>

#include "common.h"

int publisher_run(const char *topic)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int qid, rc = 0;

    if (!topic_valid(topic)) {
        fprintf(stderr, "invalid topic '%s'\n", topic);
        return 2;
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

    while (!g_stop) {
        errno = 0;   /* getline не сбрасывает errno при конце ввода, а нам нужно отличить его от EINTR */
        len = getline(&line, &cap, stdin);
        if (len < 0) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;   /* условие цикла проверит флаг */
            }
            break;          /* конец ввода */
        }
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len == 0)
            continue;

        if (send_cmd(qid, CMD_SEND, topic, line) < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EMSGSIZE) {
                fprintf(stderr, "publisher: message too long, skipped\n");
                continue;
            }
            if (queue_gone(errno)) {
                fprintf(stderr, "publisher: queue is gone\n");
                break;
            }
            perror("publisher: msgsnd");
            rc = 1;
            break;
        }
    }
    free(line);
    return rc;
}
