#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

/* Роль подписчика. Коды: 0 = штатно, 1 = ошибка, 2 = неверные аргументы */
int subscriber_run(char *const topics[], int n);

#endif
