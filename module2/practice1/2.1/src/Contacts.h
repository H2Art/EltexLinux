#include <stdio.h>

#ifndef CONTACTS_H
#define CONTACTS_H

//my constants
#define MAX_CONTACTS 100
#define MAX_ITEMS    10
#define MAX_LENGTH   50

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
    
    //basic information via inner structs
    Fullname name;            //Full name of the contact
    WorkInformation workInfo; //Work information of the contact
    
    //arrays for phones, emails, and links
    char phones[MAX_ITEMS][MAX_LENGTH];      //10 phone numbers, each up to 50 characters
    char emails[MAX_ITEMS][MAX_LENGTH];      //10 email addresses, each up to 50 characters
    char  links[MAX_ITEMS][MAX_LENGTH];      //10 links, each up to 50 characters
    
    //counters
    int phoneCount;
    int emailCount;
    int linkCount;
    
} Contact;

typedef struct ContactList {
    Contact contacts[MAX_CONTACTS];
    int count;  
} ContactList;

typedef enum {
    ITEM_PHONE,
    ITEM_EMAIL,
    ITEM_LINK,
    ITEM_NAME,
    ITEM_WORKINFO,
} ItemType;

typedef enum {
    CONTACT_OK = 0,
    CONTACT_ERR_NULL_ARG = -1,        // NULL instead of Contact/ContactList
    CONTACT_ERR_EMPTY_FIRST_NAME = -2,
    CONTACT_ERR_EMPTY_LAST_NAME = -3,
    CONTACT_ERR_LIST_FULL = -4,       // No space in ContactList
    CONTACT_ERR_BAD_INDEX = -5,
    CONTACT_ERR_BAD_COUNT = -6,       // n < 1 or n > MAX_ITEMS
    CONTACT_ERR_LIMIT_EXCEEDED = -7,  // Won't fit in array of phones/emails/links
    CONTACT_ERR_UNKNOWN_TYPE = -8,
    CONTACT_ERR_EMPTY_ITEM = -9       // Empty or NULL entry in list 
} ContactStatus;



//bridge between number (enum entry) and text
const char *contactStatusString(ContactStatus s);

//functions
int checkIfStringEmpty(const char*);
int clearItems        (Contact*, ItemType);
//adds
int addItems(int, Contact*, ItemType, ...);

//inits
int initFullName       (Contact*, const char[MAX_LENGTH], const char[MAX_LENGTH], const char[MAX_LENGTH]);
int initWorkInformation(Contact*, const char[MAX_LENGTH], const char[MAX_LENGTH]);

//endpoints (ContactList actions)
int deleteContactAt(ContactList*, int);
int editContact    (Contact*, ItemType, ...);
int addContact     (ContactList*, const Contact*);

#endif