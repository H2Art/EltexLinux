#include <stdlib.h>
#include <string.h>
#include "myqueue.h"

void pq_init(PQueue *q)
{
    q->head = NULL;
    q->size = 0;
}

void node_free(Node *n)
{
    if (n) {
        free(n->text);
        free(n);
    }
}

int pq_push(PQueue *q, int id, const char *text, int prio)
{
    if (prio < 0 || prio > 255)
        return -1;

    Node *n = malloc(sizeof *n);
    if (!n)
        return -1;
    n->text = strdup(text);
    if (!n->text) {
        free(n);
        return -1;
    }
    n->id = id;
    n->prio = prio;

    /* до первого элемента с приоритетом строго больше нового (FIFO) */
    Node **pp = &q->head;
    while (*pp && (*pp)->prio <= prio)
        pp = &(*pp)->next;
    n->next = *pp;
    *pp = n;
    q->size++;
    return 0;
}

static Node *unlink_node(PQueue *q, Node **pp)
{
    Node *n = *pp;
    *pp = n->next;
    n->next = NULL;
    q->size--;
    return n;
}

Node *pq_pop_first(PQueue *q)
{
    return q->head ? unlink_node(q, &q->head) : NULL;
}

Node *pq_pop_exact(PQueue *q, int prio)
{
    Node **pp = &q->head;
    while (*pp && (*pp)->prio < prio)
        pp = &(*pp)->next;
    if (*pp && (*pp)->prio == prio)
        return unlink_node(q, pp);
    return NULL;
}

Node *pq_pop_atleast(PQueue *q, int prio)
{
    Node **best = NULL;
    for (Node **pp = &q->head; *pp && (*pp)->prio <= prio; pp = &(*pp)->next)
        if (!best || (*pp)->prio > (*best)->prio)
            best = pp;
    return best ? unlink_node(q, best) : NULL;
}

void pq_clear(PQueue *q)
{
    Node *n;
    while ((n = pq_pop_first(q)))
        node_free(n);
}