#include "Contacts.h"
#include <string.h>
#include <stdlib.h>
#include "main.h"

//texts to print
int printMainOptions() { 
    printf("Choose an option:\n");
    printf("1. Create new contact.\n");
    printf("2. Add current contact to list.\n");
    printf("3. Edit current contact.\n");
    printf("4. Delete current contact.\n");
    printf("5. Print current contact.\n");
    printf("6. Choose contact by index.\n");
    printf("9. Exit.\n");
    return 0;
}

int printEditOptions() {
    printf("What field do you want to edit?\n");
    printf("1. Phones.\n");
    printf("2. Emails.\n");
    printf("3. Links.\n");
    printf("4. Name.\n");
    printf("5. Work Information.\n");
    printf("6. Exit.\n");
    return 0;
}

int readChoice(FILE *in) {
    char buff[16]; 
    if (!fgets(buff, sizeof buff, in)) return 0;
    if (!strchr(buff, '\n')) { 
        int c; 
        while ((c = getc(in)) != '\n' && c != EOF) { } 
    }
    return (int)strtol(buff, NULL, 10); //converting to int
}

int processMainChoice(AppState *st, int c, FILE *in) {
    int result = 0;
    switch (c)
    {
    case UO_CREATE_CONTACT: {
        st->currentContact = (Contact){0};
        fullNameInvite(st, in);
        break;
    }
    case UO_ADD_CURR_CONTACT: {
        result = addContact(&st->contactList, &st->currentContact);
        if (result) {
            printf("%s\n", contactStatusString(result));
        }
        else {
            printf("Contact succesfully added!\n");
        }
        break;
    }
    case UO_EDIT_CURR_CONTACT: {
        printEditOptions();
        int choice = readChoice(in);
        result = processEditChoice(st, choice, in);
        break;
    }
    case UO_DELETE_CURR_CONTACT: {
        if (checkContactAtIndex(st, st->currentIndex) != CONTACT_OK) {
            printf("No contact selected.\n");
            return CONTACT_ERR_BAD_INDEX;
        }
        result = deleteContactAt(&st->contactList, st->currentIndex);
        break;
    }
    case UO_PRINT_CURR_CONTACT: {
        if (checkContactAtIndex(st, st->currentIndex) != CONTACT_OK) {
            printf("No contact selected.\n");
            return CONTACT_ERR_BAD_INDEX;
        }
        printContact(&st->currentContact);
        break;
    }
    case UO_CHOOSE_CONTACT_BY_IND: {
        if (st->contactList.count == 0) {
            printf("Contact list is empty.\n");
            return CONTACT_ERR_BAD_INDEX;
        }
        printf("Type index of contact to choose (0-%d):\n", st->contactList.count - 1);
        int index = readChoice(in);
        if (checkContactAtIndex(st, index) != CONTACT_OK) {
            printf("Invalid index.\n");
            return CONTACT_ERR_BAD_INDEX;
        }
        st->currentIndex = index;
        st->currentContact = st->contactList.contacts[st->currentIndex];
        printf("Contact %d selected.\n", st->currentIndex);
        break;
    }
    case UO_EXIT: {
        printf("Exiting...\n");
        st->running = 0;
        return CONTACT_OK;
    }
    default:
        return CONTACT_ERR_BAD_INDEX;
    }
    return result;
}

int processEditChoice(AppState *st, int c, FILE *in) {
    int result = 0;
    c--; // Adjusting for 0-based index
    switch (c)
    {
    case ITEM_PHONE: {
        result = itemInvite(st, ITEM_PHONE, in);
        break;
    }
    case ITEM_EMAIL: {
        result = itemInvite(st, ITEM_EMAIL, in);
        break;
    }
    case ITEM_LINK: {
        result = itemInvite(st, ITEM_LINK, in);
        break;
    }
    case ITEM_NAME: {
        result = -1;
        while (result != CONTACT_OK) {
            result = fullNameInvite(st, in);
        }
        break;
    }
    case ITEM_WORKINFO: {
        result = workInfoInvite(st, in);
        break;
    }
    case 5: {
        result = CONTACT_OK;
        break;
    }
    default:
        return CONTACT_ERR_BAD_INDEX;
    }
    return result;
}

int checkContactAtIndex(AppState *st, int i) {
    if (!st) return CONTACT_ERR_NULL_ARG;
    if (i < 0 || i >= st->contactList.count) return CONTACT_ERR_BAD_INDEX;
    return CONTACT_OK;
}


//invites and user-input parsing
int itemInvite(AppState *st, ItemType type, FILE *in) {
    printf("How much entries do you want to add?\n");
    int n = readChoice(in);
    if (n <= 0 || n > MAX_ITEMS) {
        return CONTACT_ERR_BAD_COUNT;
    } 

    printf("Now type %d items, after each press ENTER.\n", n);


    char buffer[MAX_ITEMS][MAX_LENGTH];
    for (int i = 0; i < n; i++) {
        if (!fgets(buffer[i], MAX_LENGTH, in)) return CONTACT_ERR_EMPTY_ITEM;
        if (!strchr(buffer[i], '\n')) {
            int ch; while ((ch = getc(in)) != '\n' && ch != EOF) { }
        }
        buffer[i][strcspn(buffer[i], "\n")] = '\0'; //removing \n
    }

    return editContact(&st->currentContact, type, n, buffer);
}

int fullNameInvite(AppState *st, FILE *in) {
    char fname[50];
    char mname[50];
    char lname[50];
    printf("Please, type first name and last name, middle name is optional.\n");
    printf("First name: ");
    fgets(fname, MAX_LENGTH, in);
    printf("Middle name: ");
    fgets(mname, MAX_LENGTH, in);
    printf("Last name: ");
    fgets(lname, MAX_LENGTH, in);

    fname[strcspn(fname, "\n")] = '\0';
    mname[strcspn(mname, "\n")] = '\0'; //deleting extra \n in the end
    lname[strcspn(lname, "\n")] = '\0';
    
    int result = editContact(&st->currentContact, ITEM_NAME, fname, mname, lname);
    return result;
}

int workInfoInvite(AppState *st, FILE *in) {
    char position[50];
    char company[50];
    printf("Position: ");
    if (!fgets(position, MAX_LENGTH, in)) return CONTACT_ERR_NULL_ARG;
    printf("Company: ");
    if (!fgets(company, MAX_LENGTH, in)) return CONTACT_ERR_NULL_ARG;

    position[strcspn(position, "\n")] = '\0';
    company[strcspn(company,  "\n")] = '\0';

    int result = editContact(&st->currentContact, ITEM_WORKINFO, position, company);
    return result;
}


int printContact(const Contact *c) {
    if (!c) return CONTACT_ERR_NULL_ARG; 
    
    printf("Name: %s %s %s\n", c->name.firstName, c->name.middleName, c->name.lastName);
    printf("Work: %s at %s\n", c->workInfo.position, c->workInfo.company);
    printf("Phones (%d):\n", c->phoneCount);
    for (int i = 0; i < c->phoneCount; i++) {
        printf("  %s\n", c->phones[i]);
    }
    printf("Emails (%d):\n", c->emailCount);
    for (int i = 0; i < c->emailCount; i++) {
        printf("  %s\n", c->emails[i]);
    }
    printf("Links (%d):\n", c->linkCount);
    for (int i = 0; i < c->linkCount; i++) {
        printf("  %s\n", c->links[i]);
    }
    return CONTACT_OK;
}

#ifndef UNIT_TEST
int main(void) {
    AppState st = {0};
    st.currentIndex = -1;
    st.running = 1;
    while (st.running) {
        printMainOptions();
        processMainChoice(&st, readChoice(stdin), stdin);
    }
    return 0;
}
#endif