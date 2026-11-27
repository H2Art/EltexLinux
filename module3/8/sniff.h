#ifndef SNIFF_H
#define SNIFF_H

#include <stddef.h>
#include <stdint.h>

#define CHAT_PORT 50999     /* порт чата из задания 6 */
#define DNS_PORT  53
#define INFO_LEN  96

enum filter { F_CHAT, F_DNS };

/* Один пойманный UDP-сегмент: всё, что нужно вывести после захвата */
struct pkt {
    double t;                   /* секунды с начала захвата */
    uint8_t smac[6], dmac[6];
    uint32_t sip, dip;          /* в сетевом порядке: так их ждёт inet_ntop */
    uint16_t sport, dport;      /* в порядке хоста */
    uint16_t len;               /* длина данных UDP */
    char info[INFO_LEN];        /* разбор данных: сообщение чата или имя из DNS-запроса */
};

int pkt_parse(const uint8_t *f, size_t n, struct pkt *p, const uint8_t **data, size_t *dlen);
int filter_parse(const char *s, enum filter *f);
int filter_match(enum filter f, struct pkt *p, const uint8_t *data, size_t dlen);
int pkt_format(const struct pkt *p, int no, char *buf, size_t size);

#endif
