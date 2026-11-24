#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "pc.h"

/* Блоки выравниваем по 8 байт: в блоке есть size_t, а невыровненный доступ
 * к нему UBSan считает ошибкой (и на части архитектур это падение). */
#define ALIGN8(x) (((x) + 7) & ~(size_t)7)
#define PC_FIRST  ALIGN8(sizeof(struct pc_hdr))   /* смещение первого блока */

/* union semun программа объявляет сама: в заголовках glibc его нет (man 2 semctl) */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

volatile sig_atomic_t g_stop;

struct pc_hdr *pc_hdr(struct pc *p)
{
    return (struct pc_hdr *)p->base;
}

static struct pc_block *blk(struct pc *p, size_t off)
{
    return (struct pc_block *)(p->base + off);
}

int pc_create(struct pc *p, key_t key, size_t size)
{
    union semun arg = { .val = 1 };
    struct pc_hdr *h;
    int err;

    if (size < PC_FIRST) {
        errno = EINVAL;
        return -1;
    }
    p->shmid = -1;
    p->base = NULL;
    p->size = size;
    /* Семафор создаём первым. Пока он не выставлен в 1, это закрытый замок
     * (Linux создаёт семафоры с 0; POSIX значение не гарантирует, поэтому SETVAL ниже обязателен).
     * Потребитель, пришедший в этот момент, подождёт в semop, пока разметка не готова. */
    p->semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
    if (p->semid < 0)
        return -1;
    p->shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
    if (p->shmid < 0)
        goto fail;
    p->base = shmat(p->shmid, NULL, 0);
    if (p->base == (void *)-1) {
        p->base = NULL;
        goto fail;
    }
    h = pc_hdr(p);
    h->done = 0;
    h->last = 0;
    h->end = PC_FIRST;
    if (semctl(p->semid, 0, SETVAL, arg) < 0)
        goto fail;
    return 0;
fail:
    err = errno;
    pc_detach(p);
    pc_remove(p);
    errno = err;
    return -1;
}

int pc_attach(struct pc *p, key_t key)
{
    p->size = 0;
    p->base = NULL;
    p->semid = semget(key, 0, 0);       /* без IPC_CREAT: создаёт только производитель */
    if (p->semid < 0)
        return -1;
    p->shmid = shmget(key, 0, 0);
    if (p->shmid < 0)
        return -1;
    p->base = shmat(p->shmid, NULL, 0);
    if (p->base == (void *)-1) {
        p->base = NULL;
        return -1;
    }
    return 0;
}

void pc_detach(struct pc *p)
{
    if (p->base)
        shmdt(p->base);
    p->base = NULL;
}

/* Сегмент реально исчезнет после shmdt последнего процесса,
 * а семафор сразу: ждущие в semop проснутся с EIDRM. */
void pc_remove(struct pc *p)
{
    if (p->shmid >= 0)
        shmctl(p->shmid, IPC_RMID, NULL);
    if (p->semid >= 0)
        semctl(p->semid, 0, IPC_RMID);
    p->shmid = p->semid = -1;
}

/* SEM_UNDO: если процесс убьют внутри замка, ядро само вернёт +1,
 * иначе семафор остался бы 0 и все остальные зависли бы навсегда.
 * EINTR повторяем: замок держат недолго, а флаг остановки проверит вызывающий. */
static int sem_add(struct pc *p, short delta)
{
    struct sembuf op = { .sem_num = 0, .sem_op = delta, .sem_flg = SEM_UNDO };

    while (semop(p->semid, &op, 1) < 0)
        if (errno != EINTR)
            return -1;
    return 0;
}

int pc_lock(struct pc *p)   { return sem_add(p, -1); }
int pc_unlock(struct pc *p) { return sem_add(p, 1); }

/* Дописывает набор в конец ленты и цепляет к предыдущему блоку.
 * Вызывать под замком. Возвращает смещение блока или -1, если не влез. */
long pc_add(struct pc *p, const int *vals, int n)
{
    struct pc_hdr *h = pc_hdr(p);
    size_t need = ALIGN8(sizeof(struct pc_block) + n * sizeof(int));
    struct pc_block *b;
    size_t off = h->end;

    if (n < 1 || off + need > p->size)   /* count 0 выглядел бы как уже обработанный */
        return -1;
    b = blk(p, off);
    b->count = n;
    b->next = 0;
    memcpy(b->data, vals, n * sizeof(int));
    /* Цепляем последним шагом: до этого блок не виден при обходе списка */
    if (h->last)
        blk(p, h->last)->next = off;
    h->last = off;
    h->end = off + need;
    return (long)off;
}

static struct pc_block *first_pending(struct pc *p, size_t *off)
{
    size_t o = PC_FIRST;
    struct pc_block *b;

    if (!pc_hdr(p)->last)
        return NULL;
    for (;;) {
        b = blk(p, o);
        if (b->count > 0) {
            if (off)
                *off = o;
            return b;
        }
        if (!b->next)
            return NULL;
        o = b->next;
    }
}

/* Обрабатывает первый необработанный набор: min, max и пометка count = 0.
 * Вызывать под замком. Возвращает длину набора или 0, если обрабатывать нечего. */
int pc_take(struct pc *p, int *min, int *max, size_t *off)
{
    struct pc_block *b = first_pending(p, off);
    int i, n;

    if (!b)
        return 0;
    *min = *max = b->data[0];
    for (i = 1; i < b->count; i++) {
        if (b->data[i] < *min)
            *min = b->data[i];
        if (b->data[i] > *max)
            *max = b->data[i];
    }
    n = b->count;
    b->count = 0;
    return n;
}

int pc_pending(struct pc *p)
{
    return first_pending(p, NULL) != NULL;
}

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* Без SA_RESTART: usleep прервётся по сигналу, и цикл сразу заметит флаг */
int pc_install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;
    return 0;
}
