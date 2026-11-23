#include "common.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

static const char *const VERBS[] = {
    [CMD_SEND] = "send",
    [CMD_SUBSCRIBE] = "subscribe",
    [CMD_UNSUBSCRIBE] = "unsubscribe",
};

key_t queue_key(void)
{
    return ftok(KEY_PATH, KEY_ID);
}

int topic_valid(const char *topic)
{
    size_t len = strlen(topic);

    return len > 0 && len < TOPIC_MAX && !strpbrk(topic, ",\n");
}

int format_cmd(struct msg *m, enum cmd_kind kind, pid_t pid,
               const char *topic, const char *payload)
{
    int n;

    if (kind < CMD_SEND || kind > CMD_UNSUBSCRIBE || !topic_valid(topic))
        return -1;
    if (payload && *payload)
        n = snprintf(m->mtext, sizeof m->mtext, "%s,%d,%s\n%s",
                     VERBS[kind], (int)pid, topic, payload);
    else
        n = snprintf(m->mtext, sizeof m->mtext, "%s,%d,%s",
                     VERBS[kind], (int)pid, topic);
    if (n < 0 || (size_t)n >= sizeof m->mtext)
        return -1;
    m->mtype = BROKER_TYPE;
    return n;
}

int parse_cmd(const char *text, struct cmd *c)
{
    const char *p1 = strchr(text, ',');
    const char *p2 = p1 ? strchr(p1 + 1, ',') : NULL;
    const char *nl = strchr(text, '\n');
    const char *topic_end = nl ? nl : text + strlen(text);
    char *end;
    long pid;
    size_t verb_len, topic_len;
    int k;

    /* запятая внутри payload не должна приниматься за разделитель заголовка */
    if (!p2 || p2 > topic_end)
        return -1;

    verb_len = (size_t)(p1 - text);
    for (k = CMD_SEND; k <= CMD_UNSUBSCRIBE; k++)
        if (strlen(VERBS[k]) == verb_len && !strncmp(text, VERBS[k], verb_len))
            break;
    if (k > CMD_UNSUBSCRIBE)
        return -1;

    pid = strtol(p1 + 1, &end, 10);
    if (end != p2 || pid <= 0)
        return -1;

    topic_len = (size_t)(topic_end - (p2 + 1));
    if (topic_len == 0 || topic_len >= TOPIC_MAX || memchr(p2 + 1, ',', topic_len))
        return -1;

    c->kind = (enum cmd_kind)k;
    c->pid = (pid_t)pid;
    memcpy(c->topic, p2 + 1, topic_len);
    c->topic[topic_len] = '\0';
    c->payload = nl ? nl + 1 : "";
    return 0;
}

/* В обработчике сигнала безопасно только выставить флаг */
volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

int install_stop_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART не ставим: блокирующий вызов (msgrcv, чтение stdin) должен вернуться с EINTR,
     * иначе процесс не заметит флаг, пока не придут данные */
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;
    return 0;
}

int send_cmd(int qid, enum cmd_kind kind, const char *topic, const char *payload)
{
    struct msg m;
    int n = format_cmd(&m, kind, getpid(), topic, payload);

    if (n < 0) {
        errno = EMSGSIZE;   /* чтобы вызывающий отличал это от ошибок ядра */
        return -1;
    }
    return msgsnd(qid, &m, (size_t)n, 0);
}

int queue_gone(int err)
{
    return err == EIDRM || err == EINVAL;
}
