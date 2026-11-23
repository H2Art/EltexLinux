#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 8

/* Удваиваем массив, когда он заполнен: число клиентов заранее неизвестно */
static int grow(void **buf, size_t *cap, size_t elem, size_t used)
{
    size_t ncap;
    void *p;

    if (used < *cap)
        return 0;
    ncap = *cap ? *cap * 2 : INITIAL_CAP;
    p = realloc(*buf, ncap * elem);
    if (!p)
        return -1;
    *buf = p;
    *cap = ncap;
    return 0;
}

static int has_pid(const pid_t *a, size_t n, pid_t pid)
{
    size_t i;

    for (i = 0; i < n; i++)
        if (a[i] == pid)
            return 1;
    return 0;
}

void reg_init(struct registry *r)
{
    memset(r, 0, sizeof *r);
}

void reg_free(struct registry *r)
{
    free(r->subs);
    free(r->pubs);
    reg_init(r);
}

int reg_add_sub(struct registry *r, pid_t pid, const char *topic)
{
    size_t i;

    for (i = 0; i < r->nsubs; i++)
        if (r->subs[i].pid == pid && !strcmp(r->subs[i].topic, topic))
            return 1;
    if (grow((void **)&r->subs, &r->capsubs, sizeof *r->subs, r->nsubs) < 0)
        return -1;
    r->subs[r->nsubs].pid = pid;
    snprintf(r->subs[r->nsubs].topic, TOPIC_MAX, "%s", topic);
    r->nsubs++;
    return 0;
}

int reg_del_sub(struct registry *r, pid_t pid, const char *topic)
{
    size_t i;

    for (i = 0; i < r->nsubs; i++) {
        if (r->subs[i].pid == pid && !strcmp(r->subs[i].topic, topic)) {
            /* порядок подписок не важен, поэтому закрываем дыру последним элементом */
            r->subs[i] = r->subs[--r->nsubs];
            return 1;
        }
    }
    return 0;
}

int reg_add_pub(struct registry *r, pid_t pid)
{
    if (has_pid(r->pubs, r->npubs, pid))
        return 1;
    if (grow((void **)&r->pubs, &r->cappubs, sizeof *r->pubs, r->npubs) < 0)
        return -1;
    r->pubs[r->npubs++] = pid;
    return 0;
}

size_t reg_all_pids(const struct registry *r, pid_t **out)
{
    size_t i, n = 0;
    pid_t *a;

    *out = NULL;
    if (r->nsubs + r->npubs == 0)
        return 0;
    a = malloc((r->nsubs + r->npubs) * sizeof *a);
    if (!a)
        return 0;
    for (i = 0; i < r->nsubs; i++)
        if (!has_pid(a, n, r->subs[i].pid))
            a[n++] = r->subs[i].pid;
    for (i = 0; i < r->npubs; i++)
        if (!has_pid(a, n, r->pubs[i]))
            a[n++] = r->pubs[i];
    *out = a;
    return n;
}
