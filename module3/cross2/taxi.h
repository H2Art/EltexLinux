#ifndef TAXI_H
#define TAXI_H

#include <mqueue.h>
#include <sys/types.h>

#define MAX_DRIVERS 64      /* по 2 очереди на водителя, а в системе их не больше 256 (fs.mqueue.queues_max) */
#define TASK_MAX    86400

/* Команды CLI → driver и ответы driver → CLI. Двоичная структура, а не текст:
 * обе стороны — один и тот же бинарник на одной машине, порядок байт и выравнивание совпадают. */
enum { M_TASK = 1, M_STATUS, M_QUIT, A_OK, A_BUSY, A_AVAILABLE, A_DONE };

struct msg {
    int type;
    pid_t pid;              /* кто ответил */
    int value;              /* секунды: длина задачи или сколько осталось */
};

/* Разобранная строка CLI */
enum { C_NONE, C_CREATE, C_TASK, C_STATUS, C_LIST, C_HELP, C_EXIT, C_BAD };

struct cmd {
    int type;
    pid_t pid;
    int timer;
};

int parse_cmd(const char *line, struct cmd *c);
mqd_t queue_new(void);
int remaining(int tfd);
int driver_handle(int tfd, const struct msg *in, struct msg *out);
void driver_run(mqd_t cmd, mqd_t rep, pid_t parent);

#endif
