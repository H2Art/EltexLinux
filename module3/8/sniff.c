#include "sniff.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Формат сообщений задания 6 (module3/6/chat.h, chat.c). Скопирован, чтобы задание собиралось само;
 * при изменении формата в чате менять и здесь. */
#define CHAT_NAME_MAX  32
#define CHAT_TEXT_MAX  512
#define CHAT_DGRAM_MAX (CHAT_TEXT_MAX + CHAT_NAME_MAX + 16)
#define MSG_JOIN  'J'
#define MSG_TEXT  'M'
#define MSG_LEAVE 'L'

struct chat_msg {
    char type;
    uint32_t id;
    char name[CHAT_NAME_MAX];
    char text[CHAT_TEXT_MAX];
};

/* Та же функция, что chat_parse в module3/6/chat.c, без изменений */
static int chat_parse(const char *buf, size_t len, struct chat_msg *m)
{
    char tmp[CHAT_DGRAM_MAX + 1], *p, *end;
    size_t n;

    if (len < 12 || len > CHAT_DGRAM_MAX)
        return -1;
    memcpy(tmp, buf, len);
    tmp[len] = '\0';
    m->type = tmp[0];
    if ((m->type != MSG_JOIN && m->type != MSG_TEXT && m->type != MSG_LEAVE) || tmp[1] != ' ')
        return -1;
    errno = 0;
    m->id = strtoul(tmp + 2, &end, 16);
    if (errno || end != tmp + 10 || *end != ' ')
        return -1;
    p = end + 1;
    n = strcspn(p, " ");
    if (n == 0 || n >= CHAT_NAME_MAX)
        return -1;
    memcpy(m->name, p, n);
    m->name[n] = '\0';
    p += n;
    if (*p == ' ')
        p++;
    if (strlen(p) >= CHAT_TEXT_MAX)
        return -1;
    strcpy(m->text, p);
    return 0;
}

/* Заголовки копируем через memcpy, а не приводим указатель: IP начинается с 14-го байта,
 * и чтение uint32_t по такому адресу — невыровненный доступ (UB, его ловит UBSan). */
int pkt_parse(const uint8_t *f, size_t n, struct pkt *p, const uint8_t **data, size_t *dlen)
{
    struct ether_header eh;
    struct iphdr ip;
    struct udphdr uh;
    size_t ihl, tot, ulen;

    if (n < sizeof eh + sizeof ip)
        return -1;
    memcpy(&eh, f, sizeof eh);
    memcpy(&ip, f + sizeof eh, sizeof ip);
    f += sizeof eh;
    n -= sizeof eh;
    ihl = ip.ihl * 4u;                  /* длина IP-заголовка в 4-байтовых словах: бывают опции */
    tot = ntohs(ip.tot_len);            /* по ней, а не по n: короткий кадр Ethernet дополняется нулями */
    if (ntohs(eh.ether_type) != ETHERTYPE_IP || ip.version != 4 || ip.protocol != IPPROTO_UDP ||
        ihl < sizeof ip || tot < ihl + sizeof uh || tot > n)
        return -1;
    /* Заголовок UDP есть только в первом фрагменте, а данные неполные во всех: такие пропускаем */
    if (ntohs(ip.frag_off) & (IP_MF | IP_OFFMASK))
        return -1;
    memcpy(&uh, f + ihl, sizeof uh);
    ulen = ntohs(uh.len);
    if (ulen < sizeof uh || ulen > tot - ihl)
        return -1;
    memcpy(p->dmac, eh.ether_dhost, 6);
    memcpy(p->smac, eh.ether_shost, 6);
    p->sip = ip.saddr;
    p->dip = ip.daddr;
    p->sport = ntohs(uh.source);
    p->dport = ntohs(uh.dest);
    p->len = ulen - sizeof uh;
    p->info[0] = '\0';
    *data = f + ihl + sizeof uh;
    *dlen = p->len;
    return 0;
}

int filter_parse(const char *s, enum filter *f)
{
    if (strcmp(s, "chat") == 0)
        *f = F_CHAT;
    else if (strcmp(s, "dns") == 0)
        *f = F_DNS;
    else
        return -1;
    return 0;
}

/* Имя из первого вопроса DNS: метки "\3www\7example\3com\0" → "www.example.com".
 * Данные чужие: длину каждой метки сверяем с концом пакета и с размером out. */
static int dns_name(const uint8_t *d, size_t n, char *out, size_t size)
{
    size_t i = 12, o = 0, len;

    while (i < n && (len = d[i]) != 0) {
        if (len > 63 || i + 1 + len > n || o + len + 2 > size)
            return -1;              /* > 63 — это сжатие (указатель), в вопросе его не бывает */
        if (o)
            out[o++] = '.';
        for (size_t k = 0; k < len; k++) {
            uint8_t c = d[i + 1 + k];
            out[o++] = c > ' ' && c < 0x7f ? c : '?';
        }
        i += 1 + len;
    }
    if (i >= n)
        return -1;
    if (!o)
        out[o++] = '.';             /* корневая зона */
    out[o] = '\0';
    return 0;
}

/* 1 — сегмент подходит (в p->info разбор данных), 0 — нет */
int filter_match(enum filter f, struct pkt *p, const uint8_t *data, size_t dlen)
{
    struct chat_msg m;
    char name[INFO_LEN - 8];

    if (f == F_CHAT) {
        /* Порта мало: на 50999 может слать кто угодно. Сверяем ещё и формат сообщения */
        if (p->dport != CHAT_PORT || chat_parse((const char *)data, dlen, &m) < 0)
            return 0;
        snprintf(p->info, sizeof p->info, "%c %.31s%s%.50s", m.type, m.name, *m.text ? ": " : "", m.text);
        return 1;
    }
    if ((p->sport != DNS_PORT && p->dport != DNS_PORT) || dlen < 12 || dns_name(data, dlen, name, sizeof name) < 0)
        return 0;
    snprintf(p->info, sizeof p->info, "%s %s", data[2] & 0x80 ? "reply" : "query", name);   /* бит QR */
    return 1;
}

static void mac_str(const uint8_t *m, char *buf)
{
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

int pkt_format(const struct pkt *p, int no, char *buf, size_t size)
{
    char sm[18], dm[18], si[INET_ADDRSTRLEN], di[INET_ADDRSTRLEN];

    mac_str(p->smac, sm);
    mac_str(p->dmac, dm);
    inet_ntop(AF_INET, &p->sip, si, sizeof si);
    inet_ntop(AF_INET, &p->dip, di, sizeof di);
    return snprintf(buf, size, "%4d %11.6f  %s > %s  %s:%u > %s:%u  len %u  %s",
                    no, p->t, sm, dm, si, p->sport, di, p->dport, p->len, p->info);
}
