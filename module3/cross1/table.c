#include "table.h"

#include <stdlib.h>

/* Мультипликативное хеширование (Кнут): умножение на 2^32/φ перемешивает биты,
 * старшие TABLE_BITS бит произведения — номер корзины. Близкие порты попадают в разные корзины. */
static unsigned hash(uint32_t ip, uint16_t port)
{
    return ((ip ^ ((uint32_t)port << 16 | port)) * 2654435761u) >> (32 - TABLE_BITS);
}

/* Указатель на указатель: на ячейку корзины или на поле next предыдущего элемента.
 * Тогда удаление из головы и из середины цепочки — одна и та же операция. */
static struct client **find(struct table *t, uint32_t ip, uint16_t port)
{
    struct client **pp = &t->b[hash(ip, port)];

    while (*pp && ((*pp)->ip != ip || (*pp)->port != port))
        pp = &(*pp)->next;
    return pp;
}

/* NULL — нет такого (или create и не хватило памяти) */
struct client *table_get(struct table *t, uint32_t ip, uint16_t port, int create)
{
    struct client **pp = find(t, ip, port), *c;

    if (*pp || !create)
        return *pp;
    if (!(c = calloc(1, sizeof *c)))
        return NULL;
    c->ip = ip;
    c->port = port;
    *pp = c;                        /* в конец цепочки: *pp там был NULL */
    t->n++;
    return c;
}

int table_remove(struct table *t, uint32_t ip, uint16_t port)
{
    struct client **pp = find(t, ip, port), *c = *pp;

    if (!c)
        return -1;
    *pp = c->next;
    free(c);
    t->n--;
    return 0;
}

/* Удалить всех, от кого ничего не было дольше ttl секунд; gone вызывается до free */
size_t table_expire(struct table *t, double now, double ttl, void (*gone)(const struct client *))
{
    size_t removed = 0;

    for (unsigned i = 0; i < TABLE_SIZE; i++)
        for (struct client **pp = &t->b[i], *c; (c = *pp) != NULL;) {
            if (now - c->seen <= ttl) {
                pp = &c->next;
                continue;
            }
            if (gone)
                gone(c);
            *pp = c->next;
            free(c);
            t->n--;
            removed++;
        }
    return removed;
}

void table_free(struct table *t)
{
    for (unsigned i = 0; i < TABLE_SIZE; i++)
        while (t->b[i]) {
            struct client *c = t->b[i];
            t->b[i] = c->next;
            free(c);
        }
    t->n = 0;
}
