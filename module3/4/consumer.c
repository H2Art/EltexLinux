#include <errno.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <unistd.h>

#include "pc.h"

#define TAKE_DELAY_US 300000    /* «засыпает» после каждого набора и когда наборов нет */

int main(int argc, char *argv[])
{
    struct pc p;
    key_t key;
    size_t off;
    int n, min, max, done, rc = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc != 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return 2;
    }
    key = ftok(PC_KEY_PATH, PC_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return 1;
    }
    if (pc_attach(&p, key) < 0) {
        perror("consumer: attach (is the producer running?)");
        pc_detach(&p);
        return 1;
    }
    if (pc_install_signals() < 0) {
        perror("sigaction");
        pc_detach(&p);
        return 1;
    }

    while (!g_stop) {
        if (pc_lock(&p) < 0) {
            /* Производитель удалил семафор: работа окончена, это не ошибка */
            if (errno != EIDRM && errno != EINVAL) {
                perror("consumer: semop");
                rc = 1;
            }
            break;
        }
        n = pc_take(&p, &min, &max, &off);
        done = pc_hdr(&p)->done;
        if (pc_unlock(&p) < 0)
            break;      /* семафор удалили между lock и unlock: набор уже обработан */
        if (n > 0)
            printf("consumer %d: set of %d at offset %zu: min %d, max %d\n",
                   (int)getpid(), n, off, min, max);
        else if (done)
            break;      /* память кончилась, и всё разобрано */
        usleep(TAKE_DELAY_US);
    }
    pc_detach(&p);
    return rc;
}
