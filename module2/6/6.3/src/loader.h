#ifndef LOADER_H
#define LOADER_H

#include "calculator.h"

#define MAX_LIBS 256

typedef struct Loader {
    void *handles[MAX_LIBS];
    int   count;
} Loader;

//Loads every *.so from dir. Each library must export:
//    double operation(double, double);
//    const char symbol;
//Returns the number of loaded operations, -1 if the directory cannot be opened.
int  loadOperations(const char *dir, OperationTable *table, Loader *loader);
void unloadOperations(Loader *loader);

#endif
