#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "Contacts.h"

const char *contactStatusString(int s) {
    switch (s) {
    case CONTACT_OK:                   return "OK";
    case CONTACT_ERR_NULL_ARG:         return "Null argument";
    case CONTACT_ERR_EMPTY_FIRST_NAME: return "First name is required";
    case CONTACT_ERR_EMPTY_LAST_NAME:  return "Last name is required";
    case CONTACT_ERR_BAD_INDEX:        return "Index out of range";
    case CONTACT_ERR_BAD_COUNT:        return "Wrong number of items";
    case CONTACT_ERR_UNKNOWN_TYPE:     return "Unknown item type";
    case CONTACT_ERR_EMPTY_ITEM:       return "Empty item";
    case CONTACT_ERR_NO_MEMORY:        return "Out of memory";
    default:                           return "Unknown error";
    }
}

int isBlank(const char *s) {
    if (!s) return 1;
    for (; *s; s++) {
        if (!isspace((unsigned char)*s)) return 0;
    }
    return 1;
}

int contactSetName(Contact *c, const char *first, const char *middle, const char *last) {
    if (!c) return CONTACT_ERR_NULL_ARG;
    if (isBlank(first)) return CONTACT_ERR_EMPTY_FIRST_NAME;
    if (isBlank(last))  return CONTACT_ERR_EMPTY_LAST_NAME;

    Fullname fn;
    snprintf(fn.firstName,  sizeof fn.firstName,  "%s", first);
    snprintf(fn.middleName, sizeof fn.middleName, "%s", middle ? middle : "");
    snprintf(fn.lastName,   sizeof fn.lastName,   "%s", last);
    c->name = fn;
    return CONTACT_OK;
}

int contactSetWork(Contact *c, const char *position, const char *company) {
    if (!c) return CONTACT_ERR_NULL_ARG;
    snprintf(c->workInfo.position, sizeof c->workInfo.position, "%s", position ? position : "");
    snprintf(c->workInfo.company,  sizeof c->workInfo.company,  "%s", company  ? company  : "");
    return CONTACT_OK;
}

int contactSetItems(Contact *c, ItemType type, int n, const char items[][MAX_LENGTH]) {
    if (!c || !items) return CONTACT_ERR_NULL_ARG;

    char (*dst)[MAX_LENGTH];
    int *counter;
    switch (type) {
    case ITEM_PHONE: dst = c->phones; counter = &c->phoneCount; break;
    case ITEM_EMAIL: dst = c->emails; counter = &c->emailCount; break;
    case ITEM_LINK:  dst = c->links;  counter = &c->linkCount;  break;
    default: return CONTACT_ERR_UNKNOWN_TYPE;
    }

    if (n < 1 || n > MAX_ITEMS) return CONTACT_ERR_BAD_COUNT;
    for (int i = 0; i < n; i++) {
        if (isBlank(items[i])) return CONTACT_ERR_EMPTY_ITEM;
    }

    memset(dst, 0, (size_t)MAX_ITEMS * MAX_LENGTH);
    for (int i = 0; i < n; i++) {
        snprintf(dst[i], MAX_LENGTH, "%s", items[i]);
    }
    *counter = n;
    return CONTACT_OK;
}

/* Ключ: фамилия; при равенстве имя, затем отчество */
int contactCompare(const Contact *a, const Contact *b) {
    int r = strcasecmp(a->name.lastName, b->name.lastName);
    if (r) return r;
    r = strcasecmp(a->name.firstName, b->name.firstName);
    if (r) return r;
    return strcasecmp(a->name.middleName, b->name.middleName);
}

/* ---------- список ---------- */

void listAttachSorted(ContactList *l, Node *n) {
    Node *p = l->head;
    while (p && contactCompare(&p->data, &n->data) <= 0) p = p->next; /* равные идут после */

    n->next = p;
    if (p) { n->prev = p->prev; p->prev = n; }
    else   { n->prev = l->tail; l->tail = n; }

    if (n->prev) n->prev->next = n;
    else         l->head = n;
    l->count++;
}

void listDetach(ContactList *l, Node *n) {
    if (n->prev) n->prev->next = n->next; else l->head = n->next;
    if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
    n->prev = n->next = NULL;
    l->count--;
}

void listReposition(ContactList *l, Node *n) {
    listDetach(l, n);
    listAttachSorted(l, n);
}

int listInsert(ContactList *l, const Contact *c, Node **out) {
    if (!l || !c) return CONTACT_ERR_NULL_ARG;
    if (isBlank(c->name.firstName)) return CONTACT_ERR_EMPTY_FIRST_NAME;
    if (isBlank(c->name.lastName))  return CONTACT_ERR_EMPTY_LAST_NAME;

    Node *n = malloc(sizeof *n);
    if (!n) return CONTACT_ERR_NO_MEMORY;
    n->data = *c;
    n->prev = n->next = NULL;
    listAttachSorted(l, n);
    if (out) *out = n;
    return CONTACT_OK;
}

int listRemove(ContactList *l, Node *n) {
    if (!l || !n) return CONTACT_ERR_NULL_ARG;
    listDetach(l, n);
    free(n);
    return CONTACT_OK;
}

Node *listGetAt(const ContactList *l, int index) {
    if (!l || index < 0 || index >= l->count) return NULL;
    Node *p;
    if (index < l->count / 2) {
        p = l->head;
        for (int i = 0; i < index; i++) p = p->next;
    } else {
        p = l->tail;
        for (int i = l->count - 1; i > index; i--) p = p->prev;
    }
    return p;
}

void listFree(ContactList *l) {
    Node *p = l->head;
    while (p) {
        Node *next = p->next;
        free(p);
        p = next;
    }
    l->head = l->tail = NULL;
    l->count = 0;
}