#ifndef MYQUEUE_H
#define MYQUEUE_H

#include <stddef.h>

// Priority Queue: 0 - high priority, 255 - low priority.
// Inside we have Sorted linked list, where head has the highest priority (head.priority->0)
// If priorities are equal, we follow FIFO

typedef struct Node {
    int id;
    char *text;
    int prio;
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    size_t size;
} PQueue;

void pq_init(PQueue *q);
void pq_clear(PQueue *q);
void node_free(Node *n);

// Adding returns zero (success), wrong prio or no memory returns -1
int pq_push(PQueue *q, int id, const char *text, int prio);

// pop (NULL if nothing fits in)
Node *pq_pop_first(PQueue *q);                // first in queue
Node *pq_pop_exact(PQueue *q, int prio);      // with given prio
Node *pq_pop_atleast(PQueue *q, int prio);    // closest to prio and at least prio

#endif