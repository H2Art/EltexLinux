#include <stdio.h>
#include <unistd.h>

#include "broker.h"
#include "publisher.h"
#include "subscriber.h"

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s -b | -p topic | -s topic...\n", prog);
}

int main(int argc, char *argv[])
{
    int opt, role = 0, rest;

    /* "+" останавливает разбор на первом не-ключе: тема, начинающаяся с '-', не станет ключом */
    while ((opt = getopt(argc, argv, "+bps")) != -1) {
        if (opt == '?' || role) {
            usage(argv[0]);
            return 2;
        }
        role = opt;
    }
    rest = argc - optind;

    switch (role) {
    case 'b':
        if (rest == 0)
            return broker_run();
        break;
    case 'p':
        if (rest == 1)
            return publisher_run(argv[optind]);
        break;
    case 's':
        if (rest >= 1)
            return subscriber_run(argv + optind, rest);
        break;
    }
    usage(argv[0]);
    return 2;
}
