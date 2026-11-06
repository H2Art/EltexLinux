#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "loader.h"

//a symbol must not be confused with numbers (digits, '.') or spaces
static int isValidSymbol(char c) {
    return ispunct((unsigned char)c) && c != '.';
}

static int hasSoSuffix(const char *name) {
    size_t n = strlen(name);
    return n > 3 && strcmp(name + n - 3, ".so") == 0;
}

int loadOperations(const char *dir, OperationTable *table, Loader *loader) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    int loaded = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!hasSoSuffix(e->d_name)) continue;

        char path[PATH_MAX];
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) continue;

        void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) { fprintf(stderr, "Skip %s: %s\n", e->d_name, dlerror()); continue; }

        BinaryOp func;
        *(void **)(&func) = dlsym(h, "operation");
        const char *sym = dlsym(h, "symbol");
        const int *prio = dlsym(h, "priority");

        if (!func || !sym || !prio) {
            fprintf(stderr, "Skip %s: no 'operation', 'symbol' or 'priority'\n", e->d_name);
            dlclose(h);
            continue;
        }
        char c = *sym;
        if (!isValidSymbol(c)) {
            fprintf(stderr, "Skip %s: unsupported symbol '%c'\n", e->d_name, c);
            dlclose(h);
            continue;
        }
        if (*prio != PRIORITY_HIGH && *prio != PRIORITY_LOW) {
            fprintf(stderr, "Skip %s: priority must be 1 or 2\n", e->d_name);
            dlclose(h);
            continue;
        }
        if (hasOperation(table, c)) {
            fprintf(stderr, "Skip %s: '%c' is already loaded\n", e->d_name, c);
            dlclose(h);
            continue;
        }

        table->func[(unsigned char)c] = func;
        table->priority[(unsigned char)c] = *prio;
        loader->handles[loader->count++] = h;
        loaded++;
        printf("Loaded '%c' from %s\n", c, e->d_name);
    }
    closedir(d);
    return loaded;
}

void unloadOperations(Loader *loader) {
    for (int i = 0; i < loader->count; i++) dlclose(loader->handles[i]);
    loader->count = 0;
}

BinaryOp getOperation(const OperationTable *table, char symbol) {
    return table->func[(unsigned char)symbol];
}

int hasOperation(const OperationTable *table, char symbol) {
    return getOperation(table, symbol) != NULL;
}

int getPriority(const OperationTable *table, char symbol) {
    return table->priority[(unsigned char)symbol];
}
