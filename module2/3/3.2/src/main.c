#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "netsim.h"

int printBinary(uint32_t ip) {
    char buff[33];
    for (int i = 31; i >= 0; i--) {
        buff[31 - i] = (ip >> i) & 1 ? '1' : '0';
    }
    buff[32] = '\0';
    printf("%s\n", buff);
}

int main(int argc, char *argv[]) {
    //seed gen
    srand(time(NULL));

    if (argc != 4) {
        printf("Usage: <gateway> <mask> <N>\n");
        return -1;
    }
    uint32_t ipGateway;
    uint32_t mask;
    if (ipConverter(argv[1], &ipGateway) != 0) {
        printf("Invalid Gateway!\n");
        return -1;
    }
    if (maskConverter(argv[2], &mask) != 0) {
        printf("Invalid Mask!\n");
        return -1;
    }
    char *end;
    long N = strtol(argv[3], &end, 10);
    if (N < 1) {
        printf("Invalid N!\n");
        return -1;
    }
    int total = 0;
    uint32_t gatewayIpSubnet = getSubnetIp(ipGateway, mask);
    for (int i = 0; i < N; i++) {
        uint32_t randIp = generateRandomIp();
        uint32_t randIpSubnet = getSubnetIp(randIp, mask);
        if (isInSameSubnet(randIpSubnet, gatewayIpSubnet) == 0) { total++; }
    }
    printf("%f%% of packets reached gateway.\n", (double)total / (double)N * 100);
    return 0;
}