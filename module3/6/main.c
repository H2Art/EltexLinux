#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "chat.h"

int main(int argc, char *argv[])
{
    const char *bcast = argc > 2 ? argv[2] : CHAT_BCAST;
    unsigned long port = CHAT_PORT;
    struct chat c;
    char *end;
    int rc;

    /* setvbuf допустим только до первой операции с потоком, поэтому в самом начале.
     * stdin без буфера: иначе fgets заберёт в буфер несколько строк сразу, а poll их не увидит.
     * stdout построчно: при выводе в файл или канал строки не копятся в буфере. */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc > 3)
        port = strtoul(argv[3], &end, 10);
    if (argc < 2 || argc > 4 || (argc > 3 && (*end || port == 0 || port > 65535))) {
        fprintf(stderr, "usage: %s name [broadcast_addr [port]]   (default %s %d)\n",
                argv[0], CHAT_BCAST, CHAT_PORT);
        return 2;
    }
    if (chat_open(&c, argv[1], bcast, port) < 0) {
        if (errno == EINVAL) {      /* так chat_open сообщает о неверном имени или адресе */
            fprintf(stderr, "invalid name (1..%d chars, no spaces) or address\n", CHAT_NAME_MAX - 1);
            return 2;
        }
        perror("chat_open");
        return 1;
    }
    if (chat_install_signals() < 0) {
        perror("sigaction");
        chat_close(&c);
        return 1;
    }
    printf("chat: %s (id %08x), broadcast %s:%lu\n", c.name, c.id, bcast, port);
    rc = chat_run(&c);
    chat_close(&c);
    return rc;
}
