#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "Contacts.h"

const char *contactStatusString(ContactStatus s) {
    switch (s) {
    case CONTACT_OK:                   return "OK";
    case CONTACT_ERR_NULL_ARG:         return "Null argument";
    case CONTACT_ERR_EMPTY_FIRST_NAME: return "First name is required";
    case CONTACT_ERR_EMPTY_LAST_NAME:  return "Last name is required";
    case CONTACT_ERR_LIST_FULL:        return "Contact list is full";
    case CONTACT_ERR_BAD_INDEX:        return "Index out of range";
    case CONTACT_ERR_BAD_COUNT:        return "Wrong number of items";
    case CONTACT_ERR_LIMIT_EXCEEDED:   return "Too many items";
    case CONTACT_ERR_UNKNOWN_TYPE:     return "Unknown item type";
    case CONTACT_ERR_EMPTY_ITEM:       return "Empty item";
    default:                           return "Unknown error";
    }
}

int initFullName(Contact* c , const char* firstName, const char* middleName, const char* lastName) {
    Fullname fn;

    if (!c) { return CONTACT_ERR_NULL_ARG; } //nullptr
    if (checkIfStringEmpty(firstName)) { return CONTACT_ERR_EMPTY_FIRST_NAME; } //firstname is required
    if (checkIfStringEmpty(lastName)) { return CONTACT_ERR_EMPTY_LAST_NAME; }  //lastname is required
    

    snprintf(fn.firstName, sizeof(fn.firstName), "%s", firstName);
    snprintf(fn.middleName, sizeof(fn.middleName), "%s", middleName ? middleName : "");
    snprintf(fn.lastName, sizeof(fn.lastName), "%s", lastName);

    c->name = fn;

    return CONTACT_OK;
}

int checkIfStringEmpty(const char* c) {
    int i = 0;
    int isEmpty = 1;
    if (!c) { return 1; }
    while (c[i] != '\0') { 
        isEmpty = ((c[i] == ' ' || c[i] == '\t' || c[i] == '\n') && isEmpty);
        i++;
    }

    return isEmpty;
}

int initWorkInformation(Contact* c, const char* position, const char* company) {
    if (!c) { return CONTACT_ERR_NULL_ARG; }

    WorkInformation wi;

    snprintf(wi.position, sizeof(wi.position), "%s", position ? position : "");
    snprintf(wi.company, sizeof(wi.company), "%s", company  ? company  : "");

    c->workInfo = wi;

    return CONTACT_OK;
}

int addItems(int n, Contact* c, ItemType type, ...) {
    if (!c) { return CONTACT_ERR_NULL_ARG; }
    int *counter; //pointer to count of corresponding field
    char *base;   //pointer to first byte of 2dim array
    size_t width; //size of one entry

    switch (type)
    {
    case ITEM_PHONE:
        counter = &c->phoneCount;
        base = &c->phones[0][0];
        width = sizeof(c->phones[0]);
        break;
    case ITEM_EMAIL:
        counter = &c->emailCount;
        base = &c->emails[0][0];
        width = sizeof(c->emails[0]);
        break;
    case ITEM_LINK:
        counter = &c->linkCount;
        base = &c->links[0][0];
        width = sizeof(c->links[0]);
        break;
    default:
        return -1;
    }

    //checking border cases

    if ((n < 1) || (n + *counter > MAX_ITEMS)) {
        return CONTACT_ERR_BAD_COUNT;
    }


    //now we need to iterate through all non-required args and add them

    va_list entries, check;
    va_start(entries, type);  //moving pointer to first arg
    va_copy(check, entries);

    //at first we check that all args are not empty
    for (int i = 0; i < n; i++) {
        if (checkIfStringEmpty(va_arg(check, const char*))) {
            va_end(check);
            va_end(entries);
            return CONTACT_ERR_NULL_ARG; //one or few args were empty/NULL
        }
    }
    va_end(check);

    //if all args were proper, write them in Contact
    for (int i = 0; i < n; i++) {
        const char *entry = va_arg(entries, const char*);
        snprintf(base + (size_t)(*counter) * width, width, "%s", entry);
        (*counter)++;
    }

    va_end(entries);

    return CONTACT_OK;
}

int addContact(ContactList* clist, const Contact* c) {
    if (!c || !clist) { return CONTACT_ERR_NULL_ARG; } //nullptr exception
    if (clist->count >= MAX_CONTACTS) { return CONTACT_ERR_LIST_FULL; } //no space in contact list
    if (checkIfStringEmpty(c->name.firstName)) { return CONTACT_ERR_EMPTY_FIRST_NAME; }
    if (checkIfStringEmpty(c->name.lastName)) { return CONTACT_ERR_EMPTY_LAST_NAME; }

    clist->contacts[clist->count++] = *c;
    return CONTACT_OK;
}

int deleteContactAt(ContactList* clist, int index) {
    if (!clist) { return CONTACT_ERR_NULL_ARG; } //nullptr exception
    if (index >= clist->count || index < 0) { return -2; } //out of bounds

    for (int i = index; i < clist->count - 1; i++) {
        clist->contacts[i] = clist->contacts[i + 1]; //moving all array to the left to avoid gaps
    }
    clist->contacts[clist->count - 1] = (Contact){0}; //deleting clone
    clist->count--;
    return CONTACT_OK; 
}

int clearItems(Contact *c, ItemType type) {
    switch (type) {
    case ITEM_PHONE:
        c->phoneCount = 0;
        memset(c->phones, 0, sizeof c->phones);
        return CONTACT_OK;
    case ITEM_EMAIL:
        c->emailCount = 0;
        memset(c->emails, 0, sizeof c->emails);
        return CONTACT_OK;
    case ITEM_LINK:
        c->linkCount = 0;
        memset(c->links, 0, sizeof c->links);
        return CONTACT_OK;
    default:
        return CONTACT_ERR_UNKNOWN_TYPE; //unknown ItemType
    }
}
int editContactWithBuffer() {

}


int editContact(Contact* c, ItemType type, ...) {
    if (!c) { return CONTACT_ERR_NULL_ARG; }

    int result = 0;
    va_list args;
    va_start(args, type); //moving pointer 
    switch (type)
    {
    case ITEM_NAME: {
        char *f = va_arg(args, char*);
        char *m = va_arg(args, char*);
        char *l = va_arg(args, char*);

        result = initFullName(c, f, m, l);
        break;
    }
    case ITEM_WORKINFO: {
        char *p = va_arg(args, char*);
        char *comp = va_arg(args, char*);
        
        result = initWorkInformation(c, p, comp);
        break;
    }
    case ITEM_PHONE:
    case ITEM_LINK:
    case ITEM_EMAIL: {
        int n = va_arg(args, int);
        char (*data)[MAX_LENGTH] = va_arg(args, char (*)[MAX_LENGTH]);

        if (n <= 0 || n > MAX_ITEMS) { result = CONTACT_ERR_BAD_COUNT; break; } //wrong argument
        if (!data) { result = CONTACT_ERR_NULL_ARG; break; } //nullptr
        for (int i = 0; i < n; i++) {
            if (checkIfStringEmpty(data[i])) { result = CONTACT_ERR_EMPTY_ITEM; break; } //empty string
        }
        if (result != CONTACT_OK) { break; } //if any of the checks failed, exit
        Contact tmp = *c; //creating a temporary copy of the contact to avoid partial writing

        clearItems(&tmp, type); //clearing all previous items of this type
        for (int i = 0; i < n; i++) {
            result = addItems(1, &tmp, type, data[i]);
            if (result != CONTACT_OK) { break; }
        }
        if (result == CONTACT_OK) {
            *c = tmp; //updating the original contact with the modified temporary copy
        }
        break;




    }
    default:
        result = -2; //unknown ItemType
    }
    va_end(args);
    return result;
}
