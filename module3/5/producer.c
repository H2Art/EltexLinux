#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "pc.h"

#define DEFAULT_SIZE 1024
#define ADD_DELAY_US 100000     /* пауза между наборами */
#define WAIT_US      500000     /* пауза между проверками «всё ли обработано» */

/* Генерирует наборы, пока есть место. Возвращает 0 или -1 при ошибке семафора. */
static int produce(struct pc *p)
{
    int vals[PC_MAX_COUNT], n, i;
    long off;

    while (!g_stop) {
        n = 1 + rand() % PC_MAX_COUNT;
        for (i = 0; i < n; i++)
            vals[i] = rand() % 2001 - 1000;
        if (pc_lock(p) < 0)
            return -1;
        off = pc_add(p, vals, n);
        if (off < 0)
            pc_hdr(p)->done = 1;    /* под тем же замком: потребители увидят done вместе с последним блоком */
        if (pc_unlock(p) < 0)
            return -1;
        if (off < 0) {
            printf("producer: memory is full, waiting for consumers\n");
            return 0;
        }
        printf("producer: set of %d at offset %ld\n", n, off);
        usleep(ADD_DELAY_US);
    }
    return 0;
}

/* Ждёт, пока потребители обработают все наборы. 1 если всё обработано. */
static int wait_processed(struct pc *p)
{
    int pending;

    while (!g_stop) {
        if (pc_lock(p) < 0)
            return -1;
        pending = pc_pending(p);
        if (pc_unlock(p) < 0)
            return -1;
        if (!pending)
            return 1;
        usleep(WAIT_US);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct pc p;
    size_t size = DEFAULT_SIZE;
    char *end;
    int rc;

    setvbuf(stdout, NULL, _IOLBF, 0);   /* построчно, чтобы при выводе в файл строки не застревали */
    if (argc > 2 || (argc == 2 && ((size = strtoul(argv[1], &end, 10)) == 0 || *end))) {
        fprintf(stderr, "usage: %s [bytes]   (shared memory size, default %d)\n", argv[0], DEFAULT_SIZE);
        return 2;
    }
    if (pc_create(&p, PC_NAME, size) < 0) {
        perror("producer: create (already running? see ls /dev/shm)");
        return 1;
    }
    if (pc_install_signals() < 0) {
        perror("sigaction");
        pc_detach(&p);
        pc_remove(&p);
        return 1;
    }
    printf("producer: %s, %zu bytes\n", PC_NAME, size);
    srand(time(NULL) ^ getpid());

    rc = produce(&p);
    if (rc == 0)
        rc = wait_processed(&p);
    if (rc == 1)
        printf("producer: all sets processed\n");
    else if (rc < 0)
        perror("producer: sem_wait");
    else {
        printf("producer: interrupted\n");
        /* EIDRM, как в System V, потребители не получат: без done они ждали бы вечно */
        if (pc_lock(&p) == 0) {
            pc_hdr(&p)->done = 1;
            pc_unlock(&p);
        }
    }
    pc_detach(&p);
    pc_remove(&p);
    return rc < 0;
}
