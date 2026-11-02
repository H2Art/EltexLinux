#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myqueue.h"

static void print_queue(const PQueue *q)
{
    printf("Queue (%zu):", q->size);
    for (const Node *n = q->head; n; n = n->next)
        printf(" [p=%d id=%d]", n->prio, n->id);
    printf("\n");
}

static void show(const char *what, Node *n)
{
    if (n)
        printf("%-18s -> id=%d prio=%d \"%s\"\n", what, n->id, n->prio, n->text);
    else
        printf("%-18s -> nothing found\n", what);
    node_free(n);
}

int main(void)
{
    PQueue q;
    char buf[64], label[64];
    pq_init(&q);
    srand(time(NULL));

    // generating random tasks with random prio
    for (int id = 1; id <= 50; id++) {
        snprintf(buf, sizeof buf, "task #%d", id);
        pq_push(&q, id, buf, rand() % 256);
    }
    // same prio, testing FIFO
    pq_push(&q, 100, "same prio A", 50);
    pq_push(&q, 101, "same prio B", 50);
    print_queue(&q);

    show("pop_first", pq_pop_first(&q));
    for (int i = 0; i < 3; i++)
        show("pop_exact(50)", pq_pop_exact(&q, 50));

    int tests[] = {0, 100, 200, 255, 34};
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        snprintf(label, sizeof label, "pop_atleast(%d)", tests[i]);
        show(label, pq_pop_atleast(&q, tests[i]));
    }
    print_queue(&q);

    pq_clear(&q);
    return 0;
}