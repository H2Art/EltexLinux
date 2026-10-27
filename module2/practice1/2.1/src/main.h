#include <stdio.h>
#include "Contacts.h"

#ifndef MAIN_H
#define MAIN_H

typedef struct AppState {
    Contact     currentContact;
    ContactList contactList;
    int         currentIndex;
    int         running;
} AppState;

int printMainOptions(void);
int printEditOptions(void);

int readChoice(FILE *in);

int processMainChoice(AppState *st, int c, FILE *in);
int processEditChoice(AppState *st, int c, FILE *in);

int checkContactAtIndex(AppState *st, int i);

int fullNameInvite(AppState *st, FILE *in);
int workInfoInvite(AppState *st, FILE *in);
int itemInvite    (AppState *st, ItemType type, FILE *in);

int printContact(const Contact *c);

int main(void);


typedef enum UserOptions {
    UO_CREATE_CONTACT = 1,
    UO_ADD_CURR_CONTACT = 2,
    UO_EDIT_CURR_CONTACT = 3,
    UO_DELETE_CURR_CONTACT = 4,
    UO_PRINT_CURR_CONTACT = 5,
    UO_CHOOSE_CONTACT_BY_IND = 6,
    UO_EXIT = 9,
} UserOptions;


#endif