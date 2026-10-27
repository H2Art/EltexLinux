#include <stdio.h>
#include "Contacts.h"

#ifndef MAIN_H
#define MAIN_H

int printOptions();
int fullNameInvite();
int printName(Contact*);
int main();

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