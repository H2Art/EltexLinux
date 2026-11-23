#ifndef COMMON_H
#define COMMON_H

#include <signal.h>
#include <stddef.h>
#include <sys/ipc.h>
#include <sys/types.h>

/* ftok() требует существующий файл; /tmp есть в любой системе */
#define KEY_PATH   "/tmp"
#define KEY_ID     'B'

/* Всё, что адресовано брокеру, отправляется с этим mtype */
#define BROKER_TYPE 1L

#define TOPIC_MAX    64
#define MSG_TEXT_MAX 1024

/* Ядро требует, чтобы первым полем шло "long mtype" */
struct msg {
    long mtype;
    char mtext[MSG_TEXT_MAX];
};

enum cmd_kind { CMD_SEND = 1, CMD_SUBSCRIBE, CMD_UNSUBSCRIBE };

struct cmd {
    enum cmd_kind kind;
    pid_t pid;
    char topic[TOPIC_MAX];
    const char *payload;   /* указывает внутрь разобранного текста; "" если payload нет */
};

/*
 * Записывает в m->mtext "<verb>,<pid>,<topic>[\n<payload>]" и выставляет mtype.
 * Возвращает длину текста (она же размер для msgsnd) или -1, если не влезло.
 */
int format_cmd(struct msg *m, enum cmd_kind kind, pid_t pid,
               const char *topic, const char *payload);

/* text должен заканчиваться на \0. Возвращает 0 или -1 для битого сообщения. */
int parse_cmd(const char *text, struct cmd *c);

/* Тема допустима, если не пустая, короче TOPIC_MAX и без ',' и '\n' (это разделители) */
int topic_valid(const char *topic);

key_t queue_key(void);

/* Флаг "просили завершиться": его выставляет обработчик SIGINT/SIGTERM, а проверяют циклы ролей */
extern volatile sig_atomic_t g_stop;

/* Ставит обработчики SIGINT и SIGTERM, которые поднимают g_stop. 0 или -1. */
int install_stop_handlers(void);

/*
 * Отправляет брокеру команду от имени текущего процесса (pid берётся из getpid()).
 * При -1 errno равен EMSGSIZE, если команда не влезла или тема некорректна,
 * иначе это ошибка msgsnd.
 */
int send_cmd(int qid, enum cmd_kind kind, const char *topic, const char *payload);

/* Ошибка errno значит "очередь удалили" (EIDRM) или её id уже недействителен (EINVAL) */
int queue_gone(int err);

#endif
