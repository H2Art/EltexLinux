#ifndef COPY_H
#define COPY_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define BLOCK_SIZE  4096
#define COPY_SUFFIX ".copy"

enum msg_type { MSG_READY = 1, MSG_FILE, MSG_QUIT };

struct msg_hdr {
    uint32_t type;
    uint32_t name_len;  /* bytes of file name following the header */
    uint64_t size;      /* file size in bytes (MSG_FILE only) */
};

ssize_t read_full(int fd, void *buf, size_t n);
int write_full(int fd, const void *buf, size_t n);

/* Child side: returns process exit status (0 = ok) */
int child_run(int rfd, int wfd);

/* Parent side: 0 = ok, 1 = some files skipped, -1 = fatal error */
int parent_run(int wfd, int rfd, char *const files[], int n);

#endif