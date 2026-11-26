#ifndef CHAT_H
#define CHAT_H

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define CHAT_PORT      50999
#define CHAT_BCAST     "255.255.255.255"
#define CHAT_NAME_MAX  32
#define CHAT_TEXT_MAX  512
#define CHAT_DGRAM_MAX (CHAT_TEXT_MAX + CHAT_NAME_MAX + 16)   /* тип, id, пробелы и запас */

/* Тип сообщения: первый символ датаграммы */
#define MSG_JOIN  'J'
#define MSG_TEXT  'M'
#define MSG_LEAVE 'L'

struct chat_msg {
    char type;
    uint32_t id;                    /* случайный номер отправителя: отличает своё эхо от чужих */
    char name[CHAT_NAME_MAX];
    char text[CHAT_TEXT_MAX];
};

struct chat {
    int sock;
    struct sockaddr_in dst;         /* адрес рассылки */
    uint32_t id;
    char name[CHAT_NAME_MAX];
};

int chat_name_valid(const char *name);
int chat_format(char *buf, size_t size, char type, uint32_t id, const char *name, const char *text);
int chat_parse(const char *buf, size_t len, struct chat_msg *m);
int chat_open(struct chat *c, const char *name, const char *bcast, uint16_t port);
void chat_close(struct chat *c);
int chat_send(struct chat *c, char type, const char *text);
int chat_recv(struct chat *c, struct chat_msg *m, struct sockaddr_in *src);
int chat_install_signals(void);
int chat_run(struct chat *c);

#endif
