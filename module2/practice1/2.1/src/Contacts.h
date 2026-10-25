#include <stdio.h>

#ifndef CONTACTS_H
#define CONTACTS_H

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


//functions

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