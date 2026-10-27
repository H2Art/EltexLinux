#include "Contacts.h"
#include <string.h>
#include <stdlib.h>
#include "main.h"

static Contact     CURRENT_CONTACT;
static ContactList CURRENT_CONTACT_LIST;
static int         CURRENT_INDEX;

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

int readChoice() {
    char buff[16]; // 1 char + '\0'
    fgets(buff, sizeof(buff), stdin); //all other input gives 0! Not recommended to associate 0 to any option.
    int choice = (int)strtol(buff, NULL, 10); //converting to int
    return choice;
}

int processMainChoice(int c) {
    int result = 0;
    switch (c)
    {
    case UO_CREATE_CONTACT: {
        CURRENT_CONTACT = (Contact){0};
        fullNameInvite();
        break;
    }
    case UO_ADD_CURR_CONTACT: {
        result = addContact(&CURRENT_CONTACT_LIST, &CURRENT_CONTACT);
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
        int choice = readChoice();
        result = processEditChoice(choice);
        break;
    }
    case UO_DELETE_CURR_CONTACT: {

        break;
    }
    default:
        return CONTACT_ERR_BAD_INDEX;
    }
    return result;
}

processEditChoice(int c) {
    int result = 0;
    switch (c)
    {
    case ITEM_PHONE: {
        
    }
    case ITEM_EMAIL: {

    }
    case ITEM_LINK: {

    }
    case ITEM_NAME: {
        result = -1;
        while (result != 0) {
            result = fullNameInvite();
        }
        break;
    }
    case ITEM_WORKINFO: {
        result = workInfoInvite();
        break;
    }
    default:
        return CONTACT_ERR_BAD_INDEX;
    }
    return result;
}

int checkContactAtIndex(int i) {
    if (i < 0 || i >= MAX_CONTACTS) { return CONTACT_ERR_BAD_INDEX; }
    if (&CURRENT_CONTACT_LIST.contacts[i] == NULL) {
        return CONTACT_ERR_NULL_ARG;
    }
    return CONTACT_OK;
}


//invites
int itemInvite(ItemType type) {
    printf("How much entries do you want to add?\n");
    int n = readChoice();
    if (n <= 0 || n > MAX_ITEMS) {
        return CONTACT_ERR_BAD_COUNT;
    } 
    printf("Now type %d items, after each press ENTER.\n", n);
    char buffer[MAX_ITEMS][MAX_LENGTH];
    for (int i = 0; i < n; i++) {
        fgets(buffer[i], MAX_LENGTH, stdin);
    }

    editContact(&CURRENT_CONTACT, type, n, n);

}

int fullNameInvite() {
    char fname[50];
    char mname[50];
    char lname[50];
    printf("Please, type first name and last name, middle name is optional.\n");
    printf("First name: ");
    fgets(fname, MAX_LENGTH, stdin);
    printf("Middle name: ");
    fgets(mname, MAX_LENGTH, stdin);
    printf("Last name: ");
    fgets(lname, MAX_LENGTH, stdin);

    fname[strcspn(fname, "\n")] = '\0';
    mname[strcspn(mname, "\n")] = '\0'; //deleting extra \n in the end
    lname[strcspn(lname, "\n")] = '\0';
    
    int result = editContact(&CURRENT_CONTACT, ITEM_NAME, fname, mname, lname);
    return result;
}

int workInfoInvite() {
    char position[50];
    char company[50];
    printf("Position: ");
    fgets(position, MAX_LENGTH, stdin);
    printf("Company: ");
    fgets(company, MAX_LENGTH, stdin);

    int result = editContact(&CURRENT_CONTACT, ITEM_WORKINFO, position, company);
    return result;
}





int printName(Contact *c) {
    printf("%s\t%s\t%s\n", c->name.firstName, c->name.middleName, c->name.lastName);
}

int main() {
    CURRENT_CONTACT = (Contact){0};
    CURRENT_CONTACT_LIST = (ContactList){0};
    CURRENT_INDEX = -1;
    int running = 0;
    while (running) {
        
    }

    readChoice();


    return 0;
}