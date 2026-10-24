#include <stdio.h>

#ifndef CONTACTS_H
#define CONTACTS_H

#define MAX_CONTACTS 100
#define MAX_ITEMS    10

typedef struct Fullname {
    char firstName[50];
    char middleName[50];
    char lastName[50];
} Fullname;

typedef struct WorkInformation {
    char position[50];
    char company[50];
} WorkInformation;

typedef struct ContactList {
    Contact contacts[MAX_CONTACTS];
    int count;  
} ContactList;

typedef struct Contact {
    
    //basic information via inner structs
    struct Fullname name;            //Full name of the contact
    struct WorkInformation workInfo; //Work information of the contact
    
    //arrays for phones, emails, and links
    char phones[MAX_ITEMS][20];      //10 phone numbers, each up to 20 characters
    char emails[MAX_ITEMS][50];      //10 email addresses, each up to 50 characters
    char  links[MAX_ITEMS][50];      //10 links, each up to 50 characters
    
    //counters
    int phoneCount;
    int emailCount;
    int linkCount;
} Contact;

//functions

//adds
void addContact(Contact*);
void addNumber(Contact*, char[], ...);
void addLink(Contact*, char[], ...);
//inits
void initFullName(Contact*, char[50], char[50], ...);
void initWorkInformation(Contact*, char[50], char[50]);
//endpoints
void deleteContact(Contact*);
void editContact(struct Contact*);

#endif