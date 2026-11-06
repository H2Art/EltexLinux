#ifndef LOADER_H
#define LOADER_H

#define MAX_LIBS 256

//operation priorities: 1 is evaluated first (like * and /), 2 second (like + and -)
#define PRIORITY_HIGH 1
#define PRIORITY_LOW  2

//pointer to a math function loaded from a library
typedef double (*BinaryOp)(double, double);

//loaded functions, indexed by operator symbol: func['+'] etc. NULL = not loaded
typedef struct OperationTable {
    BinaryOp func[256];
    int      priority[256];
} OperationTable;

typedef struct Loader {
    void *handles[MAX_LIBS];
    int   count;
} Loader;

//Loads every *.so from dir. Each library must export:
//    double operation(double, double);
//    const char symbol;       (any punctuation character except '.')
//    const int  priority;     (1 or 2)
//Returns the number of loaded operations, -1 if the directory cannot be opened.
int  loadOperations(const char *dir, OperationTable *table, Loader *loader);
void unloadOperations(Loader *loader);

//Is there a loaded library for this symbol?
int      hasOperation(const OperationTable *table, char symbol);
//Function for this symbol, NULL if not loaded
BinaryOp getOperation(const OperationTable *table, char symbol);
//Priority (1 or 2) for this symbol, 0 if not loaded
int      getPriority(const OperationTable *table, char symbol);

#endif
