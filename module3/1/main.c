#include "copy.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-p fifo_name] file...\n", prog);
}

int main(int argc, char *argv[])
{
    const char *fifo = NULL;
    char p2c_name[PATH_MAX], c2p_name[PATH_MAX];
    int p2c[2] = { -1, -1 }, c2p[2] = { -1, -1 };
    int opt, rfd, wfd, rc, status;
    pid_t pid;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt != 'p') {
            usage(argv[0]);
            return 2;
        }
        fifo = optarg;
    }
    if (optind >= argc || (fifo && *fifo == '\0')) {
        usage(argv[0]);
        return 2;
    }

    signal(SIGPIPE, SIG_IGN);

    if (fifo) {
        if (snprintf(p2c_name, sizeof p2c_name, "%s.p2c", fifo) >= (int)sizeof p2c_name ||
            snprintf(c2p_name, sizeof c2p_name, "%s.c2p", fifo) >= (int)sizeof c2p_name) {
            fprintf(stderr, "fifo name too long\n");
            return 2;
        }
        if (mkfifo(p2c_name, 0600) < 0) {
            perror(p2c_name);
            return 1;
        }
        if (mkfifo(c2p_name, 0600) < 0) {
            perror(c2p_name);
            unlink(p2c_name);
            return 1;
        }
    } else if (pipe(p2c) < 0 || pipe(c2p) < 0) {
        perror("pipe");
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        if (fifo) {
            unlink(p2c_name);
            unlink(c2p_name);
        }
        return 1;
    }

    if (pid == 0) {
        /* child: reads p2c, writes c2p */
        if (fifo) {
            rfd = open(p2c_name, O_RDONLY);
            wfd = open(c2p_name, O_WRONLY);
            if (rfd < 0 || wfd < 0) {
                perror("child open fifo");
                _exit(1);
            }
        } else {
            close(p2c[1]);
            close(c2p[0]);
            rfd = p2c[0];
            wfd = c2p[1];
        }
        rc = child_run(rfd, wfd);
        close(rfd);
        close(wfd);
        _exit(rc);
    }

    /* parent: writes p2c, reads c2p (same open order as child) */
    if (fifo) {
        wfd = open(p2c_name, O_WRONLY);
        rfd = open(c2p_name, O_RDONLY);
        if (rfd < 0 || wfd < 0) {
            perror("parent open fifo");
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            unlink(p2c_name);
            unlink(c2p_name);
            return 1;
        }
    } else {
        close(p2c[0]);
        close(c2p[1]);
        wfd = p2c[1];
        rfd = c2p[0];
    }

    rc = parent_run(wfd, rfd, argv + optind, argc - optind);
    close(wfd);
    close(rfd);

    if (waitpid(pid, &status, 0) < 0)
        perror("waitpid");
    if (fifo) {
        unlink(p2c_name);
        unlink(c2p_name);
    }

    if (rc != 0)
        return 1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : 1;
}