#ifndef PC_H
#define PC_H

#include <semaphore.h>
#include <signal.h>
#include <stddef.h>

#define PC_NAME      "/eltex_pc"   /* одно имя на память и семафор: в /dev/shm это eltex_pc и sem.eltex_pc */
#define PC_NAME_SIZE 64
#define PC_MAX_COUNT 20            /* наибольшая длина набора */

/* Заголовок в начале памяти. Ссылки хранятся смещениями от начала:
 * mmap у каждого процесса возвращает свой адрес, а смещение у всех одно. */
struct pc_hdr {
    int done;       /* производитель больше ничего не добавит */
    size_t last;    /* смещение последнего блока, 0 если блоков нет */
    size_t end;     /* смещение первого свободного байта */
};

struct pc_block {
    int count;      /* 0 значит «обработан» */
    size_t next;    /* смещение следующего блока, 0 у последнего */
    int data[];
};

struct pc {
    char name[PC_NAME_SIZE];
    sem_t *sem;
    char *base;     /* адрес памяти в этом процессе */
    size_t size;    /* размер отображения: нужен для munmap */
};

extern volatile sig_atomic_t g_stop;

int pc_create(struct pc *p, const char *name, size_t size);
int pc_attach(struct pc *p, const char *name);
void pc_detach(struct pc *p);
void pc_remove(struct pc *p);
int pc_lock(struct pc *p);
int pc_unlock(struct pc *p);
struct pc_hdr *pc_hdr(struct pc *p);
long pc_add(struct pc *p, const int *vals, int n);
int pc_take(struct pc *p, int *min, int *max, size_t *off);
int pc_pending(struct pc *p);
int pc_install_signals(void);

#endif
