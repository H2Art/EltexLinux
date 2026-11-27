#ifndef PROTO_H
#define PROTO_H

#include <signal.h>
#include <stddef.h>

#define PROTO_PORT   51000
#define NAME_LEN     32                 /* имя участника вместе с \0 */
#define TEXT_MAX     1024
#define FILE_MAX     (1 << 20)          /* 1 МиБ: файл целиком держим в памяти */
#define FRAME_MAX    (FILE_MAX + 512)   /* предел данных кадра: файл + имена */
#define HDR_SIZE     5                  /* тип (1 байт) + длина (4 байта, сетевой порядок) */

/* Типы кадров. От клиента: HELLO, TEXT, FILE. От сервера: все, кроме HELLO,
 * и в TEXT/FILE первым полем идёт имя отправителя с \0. */
#define T_HELLO 'H'     /* имя */
#define T_TEXT  'T'     /* [отправитель\0]текст */
#define T_FILE  'F'     /* [отправитель\0]имя_файла\0содержимое */
#define T_USERS 'U'     /* кто уже в сети, через ", " */
#define T_JOIN  'J'     /* имя */
#define T_LEAVE 'L'     /* имя */
#define T_ERROR 'E'     /* текст отказа, после него сервер закрывает соединение */

/* TCP — поток байтов без границ: принятое копим здесь, пока не соберётся целый кадр.
 * Та же структура служит очередью на отправку. */
struct buf {
    char *data;
    size_t len, cap;
};

extern volatile sig_atomic_t g_stop;

int buf_append(struct buf *b, const void *p, size_t n);
void buf_drop(struct buf *b, size_t n);
void buf_free(struct buf *b);
int frame_put(struct buf *out, char type, const void *a, size_t alen, const void *b, size_t blen);
int frame_get(const struct buf *in, char *type, const char **payload, size_t *len);
int name_valid(const char *s, size_t n);
const char *safe_basename(const char *path);
int send_all(int fd, const void *p, size_t n);
int install_signals(void);

#endif
