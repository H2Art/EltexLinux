#include "copy.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Returns bytes read (less than n only on EOF) or -1 on error */
ssize_t read_full(int fd, void *buf, size_t n)
{
    size_t got = 0;

    while (got < n) {
        ssize_t r = read(fd, (char *)buf + got, n - got);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            break;
        got += (size_t)r;
    }
    return (ssize_t)got;
}

int write_full(int fd, const void *buf, size_t n)
{
    size_t done = 0;

    while (done < n) {
        ssize_t w = write(fd, (const char *)buf + done, n - done);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (size_t)w;
    }
    return 0;
}

static int send_msg(int fd, uint32_t type, const char *name, uint64_t size)
{
    struct msg_hdr h;

    h.type = type;
    h.name_len = name ? (uint32_t)strlen(name) : 0;
    h.size = size;
    if (write_full(fd, &h, sizeof h) < 0)
        return -1;
    if (h.name_len && write_full(fd, name, h.name_len) < 0)
        return -1;
    return 0;
}

static int recv_hdr(int fd, struct msg_hdr *h)
{
    return read_full(fd, h, sizeof *h) == (ssize_t)sizeof *h ? 0 : -1;
}

static int wait_ready(int fd)
{
    struct msg_hdr h;

    if (recv_hdr(fd, &h) < 0 || h.type != MSG_READY)
        return -1;
    return 0;
}

/* ---------------- child ---------------- */

/* 0 = ok, 1 = file error (stream drained), -1 = fatal */
static int child_save_file(int rfd, const struct msg_hdr *h)
{
    char buf[BLOCK_SIZE];
    char *name = malloc((size_t)h->name_len + sizeof COPY_SUFFIX);
    uint64_t left = h->size;
    int fd, rc = 0;

    if (!name) {
        perror("malloc");
        return -1;
    }
    if (read_full(rfd, name, h->name_len) != (ssize_t)h->name_len) {
        free(name);
        return -1;
    }
    memcpy(name + h->name_len, COPY_SUFFIX, sizeof COPY_SUFFIX);

    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(name);
        rc = 1;
    }

    while (left > 0) {
        size_t chunk = left < BLOCK_SIZE ? (size_t)left : BLOCK_SIZE;

        if (read_full(rfd, buf, chunk) != (ssize_t)chunk) {
            rc = -1;
            break;
        }
        if (fd >= 0 && write_full(fd, buf, chunk) < 0) {
            perror(name);
            close(fd);
            fd = -1;
            rc = 1;
        }
        left -= chunk;
    }

    if (fd >= 0 && close(fd) < 0) {
        perror(name);
        if (rc == 0)
            rc = 1;
    }
    free(name);
    return rc;
}

int child_run(int rfd, int wfd)
{
    struct msg_hdr h;
    int status = 0;

    if (send_msg(wfd, MSG_READY, NULL, 0) < 0)
        return 1;

    for (;;) {
        int r;

        if (recv_hdr(rfd, &h) < 0) {
            fprintf(stderr, "child: protocol error\n");
            return 1;
        }
        if (h.type == MSG_QUIT)
            return status;
        if (h.type != MSG_FILE || h.name_len == 0 || h.name_len > PATH_MAX) {
            fprintf(stderr, "child: bad message\n");
            return 1;
        }

        r = child_save_file(rfd, &h);
        if (r < 0)
            return 1;
        if (r > 0)
            status = 1;

        if (send_msg(wfd, MSG_READY, NULL, 0) < 0)
            return 1;
    }
}

/* ---------------- parent ---------------- */

/* 0 = ok, 1 = skipped, -1 = fatal */
static int send_file(int wfd, int rfd, const char *path)
{
    char buf[BLOCK_SIZE];
    struct stat st;
    uint64_t left;
    int fd;

    if (strlen(path) + sizeof COPY_SUFFIX > PATH_MAX) {
        fprintf(stderr, "%s: name too long\n", path);
        return 1;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        return 1;
    }
    if (fstat(fd, &st) < 0) {
        perror(path);
        close(fd);
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "%s: not a regular file\n", path);
        close(fd);
        return 1;
    }

    left = (uint64_t)st.st_size;
    if (send_msg(wfd, MSG_FILE, path, left) < 0) {
        perror("write to pipe");
        close(fd);
        return -1;
    }

    while (left > 0) {
        size_t chunk = left < BLOCK_SIZE ? (size_t)left : BLOCK_SIZE;
        ssize_t r = read(fd, buf, chunk);

        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0) {
            fprintf(stderr, "%s: read failed or file changed\n", path);
            close(fd);
            return -1;
        }
        if (write_full(wfd, buf, (size_t)r) < 0) {
            perror("write to pipe");
            close(fd);
            return -1;
        }
        left -= (uint64_t)r;
    }
    close(fd);

    if (wait_ready(rfd) < 0) {
        fprintf(stderr, "no READY from child after %s\n", path);
        return -1;
    }
    return 0;
}

int parent_run(int wfd, int rfd, char *const files[], int n)
{
    int i, skipped = 0;

    if (wait_ready(rfd) < 0) {
        fprintf(stderr, "no READY from child\n");
        return -1;
    }
    for (i = 0; i < n; i++) {
        int r = send_file(wfd, rfd, files[i]);
        if (r < 0)
            return -1;
        if (r > 0)
            skipped = 1;
    }
    if (send_msg(wfd, MSG_QUIT, NULL, 0) < 0) {
        perror("write to pipe");
        return -1;
    }
    return skipped;
}