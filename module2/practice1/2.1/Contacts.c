#include <stdio.h>
#include <stdarg.h>
#include "Contacts.h"

void initFullName(Contact* c, char firstName[50], char middleName[50], ...) {
    snprintf(c->name.firstName, sizeof(c->name.firstName), "%s", firstName);
    snprintf(c->name.middleName, sizeof(c->name.middleName), "%s", middleName);
    snprintf(c->name.lastName, sizeof(c->name.lastName), "%s", va_start(va_list param, middleName));
}