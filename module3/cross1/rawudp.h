#ifndef RAWUDP_H
#define RAWUDP_H

#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#define ECHO_PORT   51500
#define TEXT_MAX    1024
#define PKT_MAX     65536           /* больше IP-пакета не бывает */

/* Типы по длине данных: пустая датаграмма — закрытие, один байт \0 — «я жив», иначе текст */
#define IS_CLOSE(n, d)  ((n) == 0)
#define IS_ALIVE(n, d)  ((n) == 1 && (d)[0] == '\0')

struct udp_in {
    struct in_addr src, dst;
    uint16_t sport, dport;          /* в порядке хоста */
    const uint8_t *data;
    size_t len;
};

extern volatile sig_atomic_t g_stop;

int raw_open(void);
int udp_reserve(uint16_t *port);
size_t udp_build(uint8_t *buf, size_t size, uint16_t sport, uint16_t dport, const void *d, size_t n);
int udp_parse(const uint8_t *pkt, size_t n, struct udp_in *u);
int udp_send(int raw, struct in_addr dst, uint16_t sport, uint16_t dport, const void *d, size_t n);
int install_signals(void);
double now(void);
long num(const char *s);

#endif
