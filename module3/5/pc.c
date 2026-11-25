#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pc.h"

/* Блоки выравниваем по 8 байт: в блоке есть size_t, а невыровненный доступ
 * к нему UBSan считает ошибкой (и на части архитектур это падение). */
#define ALIGN8(x) (((x) + 7) & ~(size_t)7)
#define PC_FIRST  ALIGN8(sizeof(struct pc_hdr))   /* смещение первого блока */

volatile sig_atomic_t g_stop;

struct pc_hdr *pc_hdr(struct pc *p)
{
    return (struct pc_hdr *)p->base;
}

static struct pc_block *blk(struct pc *p, size_t off)
{
    return (struct pc_block *)(p->base + off);
}

static int set_name(struct pc *p, const char *name)
{
    p->sem = SEM_FAILED;
    p->base = NULL;
    p->size = 0;
    if (snprintf(p->name, sizeof p->name, "%s", name) >= (int)sizeof p->name) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int map(struct pc *p, int fd, size_t size)
{
    void *a = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (a == MAP_FAILED)
        return -1;
    p->base = a;
    p->size = size;
    return 0;
}

int pc_create(struct pc *p, const char *name, size_t size)
{
    struct pc_hdr *h;
    int fd, err;

    if (set_name(p, name) < 0)
        return -1;
    if (size < PC_FIRST) {
        errno = EINVAL;
        return -1;
    }
    /* O_EXCL: второй производитель упадёт здесь, ничего не тронув */
    fd = shm_open(p->name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0)
        return -1;
    /* Новый объект имеет длину 0, ftruncate задаёт размер (новые байты нулевые) */
    if (ftruncate(fd, size) < 0 || map(p, fd, size) < 0)
        goto fail;
    close(fd);      /* отображение живёт и без дескриптора */
    fd = -1;
    h = pc_hdr(p);
    h->done = 0;
    h->last = 0;
    h->end = PC_FIRST;
    /* Семафор создаём последним и сразу открытым (1): потребитель ищет его первым,
     * и если нашёл, память уже нужного размера и размечена. */
    p->sem = sem_open(p->name, O_CREAT | O_EXCL, 0600, 1);
    if (p->sem == SEM_FAILED)
        goto fail;
    return 0;
fail:
    err = errno;
    if (fd >= 0)
        close(fd);
    pc_detach(p);
    shm_unlink(p->name);    /* только память: её создали мы, а семафор с этим именем мог быть чужой */
    errno = err;
    return -1;
}

int pc_attach(struct pc *p, const char *name)
{
    struct stat st;
    int fd, rc;

    if (set_name(p, name) < 0)
        return -1;
    p->sem = sem_open(p->name, 0);      /* без O_CREAT: создаёт только производитель */
    if (p->sem == SEM_FAILED)
        return -1;
    fd = shm_open(p->name, O_RDWR, 0);
    if (fd < 0)
        return -1;
    /* Размер знает только производитель, а отображению он нужен заранее: берём у объекта */
    rc = fstat(fd, &st) < 0 ? -1 : map(p, fd, st.st_size);
    close(fd);
    return rc;
}

void pc_detach(struct pc *p)
{
    if (p->base)
        munmap(p->base, p->size);
    if (p->sem != SEM_FAILED)
        sem_close(p->sem);
    p->base = NULL;
    p->sem = SEM_FAILED;
}

/* Как rm для файла: имена пропадают сразу, а память и семафор живут,
 * пока их держат открытыми потребители. Поэтому EIDRM, как в System V, здесь нет. */
void pc_remove(struct pc *p)
{
    shm_unlink(p->name);
    sem_unlink(p->name);
}

/* SEM_UNDO в POSIX нет: процесс, убитый kill -9 внутри замка, подвесит остальных.
 * EINTR повторяем: замок держат недолго, а флаг остановки проверит вызывающий. */
int pc_lock(struct pc *p)
{
    while (sem_wait(p->sem) < 0)
        if (errno != EINTR)
            return -1;
    return 0;
}

int pc_unlock(struct pc *p)
{
    return sem_post(p->sem);
}

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
