#ifndef PC_H
#define PC_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

#define PC_KEY_PATH  "/tmp"
#define PC_KEY_ID    'D'
#define PC_MAX_COUNT 20     /* наибольшая длина набора */

/* Заголовок в начале сегмента. Ссылки хранятся смещениями от начала сегмента:
 * shmat у каждого процесса возвращает свой адрес, а смещение у всех одно. */
struct pc_hdr {
    int done;       /* производитель закончил: память кончилась */
    size_t last;    /* смещение последнего блока, 0 если блоков нет */
    size_t end;     /* смещение первого свободного байта */
};

struct pc_block {
    int count;      /* 0 значит «обработан» */
    size_t next;    /* смещение следующего блока, 0 у последнего */
    int data[];
};

struct pc {
    int shmid, semid;
    char *base;     /* адрес сегмента в этом процессе */
    size_t size;    /* сколько байт сегмента размечаем (знает только производитель) */
};

extern volatile sig_atomic_t g_stop;

int pc_create(struct pc *p, key_t key, size_t size);
int pc_attach(struct pc *p, key_t key);
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
