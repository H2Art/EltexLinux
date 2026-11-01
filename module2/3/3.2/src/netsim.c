#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "netsim.h"

int ipConverter(const char *s, uint32_t *ip) {
    unsigned a, b, c, d;
    char extra;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4)
        return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return -1;
    *ip = ((uint32_t)a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

int maskConverter(const char *s, uint32_t *mask) {
    if (s[0] != '/') return -1;

    char *end;
    long n = strtol(s + 1, &end, 10);
    if (end == s + 1 || *end != '\0') return -1;
    if (n < 0 || n > 32) return -1;

    *mask = (n == 0) ? 0 : (0xFFFFFFFFu << (32 - n));
    return 0;
}

uint32_t generateRandomIp() {
    uint32_t ip = 0;
    for (int i = 0; i < 4; i++) {
        ip = (ip << 8) | (uint32_t)(rand() & 0xFF);
    }
    return ip;
}

uint32_t getSubnetIp(uint32_t destIp, uint32_t mask) {
    return (destIp & mask); 
}

int isInSameSubnet(uint32_t randIpSubnet, uint32_t destIpSubnet) {
    return (destIpSubnet ^ randIpSubnet);
}
