#ifndef REGISTRY_H
#define REGISTRY_H

#include <stddef.h>
#include <sys/types.h>

#include "common.h"

/* Подписка: процесс pid хочет получать сообщения темы topic */
struct sub {
    pid_t pid;
    char topic[TOPIC_MAX];
};

/* Что брокер знает о клиентах: подписки и издатели */
struct registry {
    struct sub *subs;
    size_t nsubs, capsubs;
    pid_t *pubs;
    size_t npubs, cappubs;
};

void reg_init(struct registry *r);
void reg_free(struct registry *r);

/* 0 = добавлено, 1 = уже было, -1 = нет памяти */
int reg_add_sub(struct registry *r, pid_t pid, const char *topic);

/* 1 = удалено, 0 = такой подписки не было */
int reg_del_sub(struct registry *r, pid_t pid, const char *topic);

/* 0 = добавлен, 1 = уже был, -1 = нет памяти */
int reg_add_pub(struct registry *r, pid_t pid);

/*
 * Все известные pid (подписчики и издатели) без повторов: процесс может быть
 * и тем и другим, а сигнал ему нужен один раз. Возвращает количество,
 * *out освобождает вызывающий (free). При нуле или ошибке *out == NULL.
 */
size_t reg_all_pids(const struct registry *r, pid_t **out);

#endif
