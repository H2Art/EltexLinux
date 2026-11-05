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
    char links[MAX_ITEMS][MAX_LENGTH];   // Links, Phones, Emails
    int  phoneCount;
    int  emailCount;
    int  linkCount;
} Contact;

// Red-black tree. Key: lastName -> firstName -> middleName (case-insensitive), keys are unique
typedef enum {
    RB_RED,
    RB_BLACK
} NodeColor;

typedef struct Node {
    Contact       data;
    NodeColor     color;
    struct Node  *left;
    struct Node  *right;
    struct Node  *parent;
} Node;

typedef struct ContactTree {
    Node *root;
    int   count;
} ContactTree;

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
    CONTACT_ERR_NO_MEMORY = -8,
    CONTACT_ERR_DUPLICATE = -9
} ContactStatus;

const char *contactStatusString(int status);

//Contact functions
int isBlank(const char *s);
int contactSetName(Contact *c, const char *first, const char *middle, const char *last);
int contactSetWork(Contact *c, const char *position, const char *company);

//Changing all items of a specific type; if an error occurs, the contact remains unchanged
int contactSetItems(Contact *c, ItemType type, int n, const char items[][MAX_LENGTH]);
int contactCompare(const Contact *a, const Contact *b);

// Tree functions
// Insert a copy of c; CONTACT_ERR_DUPLICATE if a contact with the same full name exists
int   treeInsert(ContactTree *t, const Contact *c, Node **out);
// Replace data of node n with newData (the node pointer stays valid).
// CONTACT_ERR_DUPLICATE if newData collides with ANOTHER node; on error nothing changes
int   treeUpdate(ContactTree *t, Node *n, const Contact *newData);
int   treeRemove(ContactTree *t, Node *n);
Node *treeFind(const ContactTree *t, const Contact *key);
Node *treeMin(const Node *n);                          // leftmost node of the subtree
Node *treeNext(const Node *n);                         // in-order successor or NULL
Node *treeGetAt(const ContactTree *t, int index);      // index in sorted order
void  treeFree(ContactTree *t);
// Checks red-black properties and ordering. Returns black height (>= 0) or -1 if broken
int   treeValidate(const ContactTree *t);

#endif