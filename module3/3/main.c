#include <errno.h>
#include <stdio.h>

#include "chat.h"

int main(int argc, char *argv[])
{
    struct chat c;
    int rc;

    /* setvbuf допустим только до первой операции с потоком, поэтому в самом начале.
     * stdin без буфера: иначе fgets заберёт в буфер несколько строк сразу, а poll
     * по дескриптору их уже не увидит и будет ждать новых данных.
     * stdout построчно: при выводе в файл или канал строки не копятся в буфере. */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 2 || chat_names(argv[1], c.name1, c.name2, sizeof c.name1) < 0) {
        fprintf(stderr, "usage: %s name   (name without '/', queues /name_1 and /name_2)\n", argv[0]);
        return 2;
    }
    if (chat_open(&c) < 0) {
        perror("mq_open");
        return 1;
    }
    if (chat_install_signals() < 0) {
        perror("sigaction");
        chat_close(&c);
        return 1;
    }
    printf("chat: %s, reading %s, writing %s\n", c.creator ? "created" : "joined",
           c.creator ? c.name1 : c.name2, c.creator ? c.name2 : c.name1);
    rc = chat_run(&c);
    chat_close(&c);
    return rc;
}
