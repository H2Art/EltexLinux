#include <stdio.h>
#include <stdint.h>

#ifndef NETSIM_H
#define NETSIM_H

int ipConverter(const char *s, uint32_t *ip);
int maskConverter(const char *s, uint32_t *mask);
uint32_t generateRandomIp();
uint32_t getSubnetIp(uint32_t destIp, uint32_t mask);
int isInSameSubnet(uint32_t randIpSubnet, uint32_t destIpSubnet);

#endif