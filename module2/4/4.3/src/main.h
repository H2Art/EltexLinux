#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include "Contacts.h"

typedef struct AppState {
    ContactTree tree;
    Node       *current;   /* выбранный контакт или NULL */
    int         running;
} AppState;

typedef enum {
    UO_ADD = 1,
    UO_LIST = 2,
    UO_EDIT = 3,
    UO_DELETE = 4,
    UO_PRINT = 5,
    UO_SELECT = 6,
    UO_TREE = 7,
    UO_EXIT = 9
} UserOption;

typedef enum {
    EO_PHONES = 1,
    EO_EMAILS = 2,
    EO_LINKS = 3,
    EO_NAME = 4,
    EO_WORK = 5,
    EO_BACK = 6
} EditOption;

int  readLine(FILE *in, char *buf, size_t size);   /* 1 = ok, 0 = EOF */
int  readInt(FILE *in, int *out);                  /* 1 = ok, 0 = EOF */

void printMainOptions(void);
void printEditOptions(void);
void printContact(const Contact *c);
void printList(const ContactTree *t);
void printTree(const ContactTree *t);

int  processMainChoice(AppState *st, int choice, FILE *in);
int  processEditChoice(AppState *st, int choice, FILE *in);

int  askName(Contact *c, FILE *in);
int  askWork(Contact *c, FILE *in);
int  askItems(Contact *c, ItemType type, FILE *in);

#endif