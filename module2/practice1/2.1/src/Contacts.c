#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "Contacts.h"

int initFullName(Contact* c , const char* firstName, const char* middleName, const char* lastName) {
    Fullname fn;

    if (c == NULL) { return -1; } //nullptr
    if (firstName == NULL || firstName[0] == '\0') { return -2; }    //firstname is required
    if (lastName == NULL || lastName[0] == '\0') { return -3; } //lastname is required
    

    snprintf(fn.firstName, sizeof(fn.firstName), "%s", firstName);
    snprintf(fn.middleName, sizeof(fn.middleName), "%s", middleName ? middleName : "");
    snprintf(fn.lastName, sizeof(fn.lastName), "%s", lastName);

    c->name = fn;

    return 0;
}

int initWorkInformation(Contact* c, const char* position, const char* company) {
    WorkInformation wi;

    snprintf(wi.position, sizeof(wi.position), "%s", position ? position : "");
    snprintf(wi.company, sizeof(wi.company), "%s", company  ? company  : "");

    c->workInfo = wi;

    return 0;
}

int addItems(int n, Contact* c, ItemType type, ...) {
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

    if (n < 1) {
        return -2;
    }
    if (n + *counter > MAX_ITEMS) {
        return -3;
    }


    //now we need to iterate through all non-required args and add them

    va_list entries;
    va_start(entries, type);  //moving pointer to first arg
    for (int i = 0; i < n; i++) {
        const char *entry = va_arg(entries, const char*);
        snprintf(base + (size_t)(*counter) * width, width, "%s", entry);
        (*counter)++;
    }

    va_end(entries);

    return 0;
}

int addContact(ContactList* clist, const Contact* c) {
    if (!c || !clist) { return -1; } //nullptr exception
    if (clist->count >= MAX_CONTACTS) { return -2; } //no space in contact list
    clist->contacts[clist->count++] = *c;

    return 0;
}

int deleteContactAt(ContactList* clist, int index) {
    if (!clist) { return -1; } //nullptr exception
    if (index >= clist->count || index < 0) { return -2; } //out of bounds

    for (int i = index; i < clist->count - 1; i++) {
        clist->contacts[i] = clist->contacts[i + 1]; //moving all array to the left to avoid gaps
    }
    clist->contacts[clist->count - 1] = (Contact){0}; //deleting clone
    clist->count--;
    return 0; 
}

int editContact(Contact* c, ItemType type, ...) {
    if (!c) { return -1; }

    int result = 0;
    va_list args;
    va_start(args, type); //moving pointer 
    switch (type)
    {
    case ITEM_NAME:
        char *f = va_arg(args, char*);
        char *m = va_arg(args, char*);
        char *l = va_arg(args, char*);

        result = initFullName(c, f, m, l);

        return result;
    case ITEM_WORKINFO:
        char *p = va_arg(args, char*);
        char *comp = va_arg(args, char*);
        
        result = initWorkInformation(c, p, comp);
        
        return result;
    case ITEM_PHONE:
    case ITEM_LINK:
    case ITEM_EMAIL:
        int n = va_arg(args, int);
        result = 0;
        if (n <= 0) { return -1; } //wrong argument
        while (n > 0) {
            char *field = va_arg(args, char*);
            result = addItems(1, c, type, field);
            if (result != 0) { return result; } //runtime error, partially written
            n--;
        }
        return result;
    default:
        va_end(args);
        return -2; //unknown ItemType
    }
    va_end(args);
    return 0;
}
