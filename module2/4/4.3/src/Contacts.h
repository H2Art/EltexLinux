#ifndef CONTACTS_H
#define CONTACTS_H

#define MAX_ITEMS  10
#define MAX_LENGTH 50

typedef struct Fullname {
    char firstName[MAX_LENGTH];
    char middleName[MAX_LENGTH];
    char lastName[MAX_LENGTH];
} Fullname;

typedef struct WorkInformation {
    char position[MAX_LENGTH];
    char company[MAX_LENGTH];
} WorkInformation;

typedef struct Contact {
    Fullname        name;
    WorkInformation workInfo;
    char phones[MAX_ITEMS][MAX_LENGTH];
    char emails[MAX_ITEMS][MAX_LENGTH];
    char links[MAX_ITEMS][MAX_LENGTH];   /* соцсети и мессенджеры */
    int  phoneCount;
    int  emailCount;
    int  linkCount;
} Contact;

/* Двухсвязный список, упорядочен по фамилии */
typedef struct Node {
    Contact      data;
    struct Node *prev;
    struct Node *next;
} Node;

typedef struct ContactList {
    Node *head;
    Node *tail;
    int   count;
} ContactList;

typedef enum {
    ITEM_PHONE,
    ITEM_EMAIL,
    ITEM_LINK
} ItemType;

typedef enum {
    CONTACT_OK = 0,
    CONTACT_ERR_NULL_ARG = -1,
    CONTACT_ERR_EMPTY_FIRST_NAME = -2,
    CONTACT_ERR_EMPTY_LAST_NAME = -3,
    CONTACT_ERR_BAD_INDEX = -4,
    CONTACT_ERR_BAD_COUNT = -5,
    CONTACT_ERR_UNKNOWN_TYPE = -6,
    CONTACT_ERR_EMPTY_ITEM = -7,
    CONTACT_ERR_NO_MEMORY = -8
} ContactStatus;

const char *contactStatusString(int status);

/* Contact */
int isBlank(const char *s);
int contactSetName(Contact *c, const char *first, const char *middle, const char *last);
int contactSetWork(Contact *c, const char *position, const char *company);
/* Заменяет ВСЕ элементы типа type; при ошибке контакт не меняется */
int contactSetItems(Contact *c, ItemType type, int n, const char items[][MAX_LENGTH]);
int contactCompare(const Contact *a, const Contact *b);

/* List */
int   listInsert(ContactList *l, const Contact *c, Node **out);
void  listDetach(ContactList *l, Node *n);
void  listAttachSorted(ContactList *l, Node *n);
void  listReposition(ContactList *l, Node *n);  /* после смены фамилии */
int   listRemove(ContactList *l, Node *n);
Node *listGetAt(const ContactList *l, int index);
void  listFree(ContactList *l);

#endif