#ifndef CHAT_H
#define CHAT_H

#include <mqueue.h>
#include <stddef.h>

#define CHAT_NAME_SIZE 256   /* имя очереди не длиннее NAME_MAX (255) вместе со слэшем */
#define CHAT_MAX_MSGS  10    /* потолок по умолчанию: /proc/sys/fs/mqueue/msg_max */
#define CHAT_MSG_SIZE  256

/* Приоритет служит меткой. "Ухожу" ниже обычных, поэтому приходит после всех
 * непрочитанных сообщений и не обгоняет их. */
#define PRIO_TEXT 1
#define PRIO_BYE  0

struct chat {
    char name1[CHAT_NAME_SIZE];   /* "/имя_1" */
    char name2[CHAT_NAME_SIZE];   /* "/имя_2" */
    int creator;                  /* 1, если очереди создали мы: тогда мы их и удаляем */
    mqd_t in, out;
};

/* Строит "/base_1" и "/base_2". Ведущий '/' в base допускается. 0 или -1 для плохого имени. */
int chat_names(const char *base, char *n1, char *n2, size_t size);

/* Открывает или создаёт очереди по c->name1/name2 и определяет роль. 0 или -1 (errno). */
int chat_open(struct chat *c);

/* Закрывает очереди; создатель ещё и удаляет их имена */
void chat_close(struct chat *c);

int chat_install_signals(void);

/* Цикл чата до Ctrl+C, конца ввода или ухода собеседника. Код завершения процесса.
 * stdin должен быть без буфера (см. main.c). */
int chat_run(struct chat *c);

#endif
