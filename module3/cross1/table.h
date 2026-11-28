#ifndef TABLE_H
#define TABLE_H

#include <stddef.h>
#include <stdint.h>

#define TABLE_BITS 8
#define TABLE_SIZE (1u << TABLE_BITS)

/* Счётчик сообщений одного клиента; клиент — это пара ip:port */
struct client {
    uint32_t ip;                    /* в сетевом порядке, как в IP-заголовке */
    uint16_t port;
    unsigned count;
    double seen;                    /* когда последний раз что-то присылал */
    struct client *next;            /* цепочка в корзине */
};

/* Хеш-таблица с цепочками: корзина = односвязный список клиентов с одинаковым хешем */
struct table {
    struct client *b[TABLE_SIZE];
    size_t n;
};

struct client *table_get(struct table *t, uint32_t ip, uint16_t port, int create);
int table_remove(struct table *t, uint32_t ip, uint16_t port);
size_t table_expire(struct table *t, double now, double ttl, void (*gone)(const struct client *));
void table_free(struct table *t);

#endif
