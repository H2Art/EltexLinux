#include "Contacts.h"
int main() {
    Contact c1;
    Contact* c1ptr = &(c1);
    ContactList contactList;
    ContactList* clptr = &(contactList);
    addContact(clptr, c1ptr);
    initFullName(c1ptr, "Mike", NULL, "Wazzowski");
    addItems(1, c1ptr, ITEM_PHONE, "+79538668660");
    printf("%s\n", c1ptr->phones[0]);
    printf("firstname: %s lastname: %s\n", c1ptr->name.firstName, c1ptr->name.lastName);
    printf("I'm up to date!\n");
    editContact(c1ptr, ITEM_NAME, "Artur", "Andreevich", "Vulf");
    printf("firstname: %s   middlename: %s    lastname: %s\n", c1ptr->name.firstName, c1ptr->name.middleName, c1ptr->name.lastName);




    return 0;
}