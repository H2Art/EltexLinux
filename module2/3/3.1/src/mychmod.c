#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "mychmod.h"

int getPermissionsString(const mode_t bitmap, char *output) {
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) {
        output[i] = (bitmap & (1 << (8 - i))) ? rwx[i] : '-';
    }
    output[9] = '\0';
    return 0;
}

int getPermissionsFromFile(const char *filename, mode_t *bitmap) {
    struct stat st;
    if (stat(filename, &st) != 0) { return -1; }
    *bitmap = st.st_mode & 0777;
    return 0;
}

int getBitmap(const char *permissions, mode_t *bitmap) {
    size_t len = strlen(permissions);
    mode_t result = 0;

    if (len == 3) {
        for (int i = 0; i < 3; i++) {
            if (permissions[i] < '0' || permissions[i] > '7')
                return -1;
        }
        result = (mode_t)strtol(permissions, NULL, 8);
    } 
    else if (len == 9) {
        const char *rwx = "rwxrwxrwx";
        for (int i = 0; i < 9; i++) {
            if (permissions[i] == rwx[i])
                result |= 1 << (8 - i);
            else if (permissions[i] != '-')
                return -1;
        }
    } 
    else {
        return -1;
    }

    *bitmap = result;
    return 0;
}

int applyChmod(const char *cmd, mode_t *bitmap) {
    const char *p = cmd;
    mode_t who = 0, perm = 0;
    mode_t perm_mask;

    for (; *p == 'u' || *p == 'g' || *p == 'o' || *p == 'a'; p++) {
        switch (*p)
        {
        case 'u':
            who |= 0700;
            break;
        case 'g':
            who |= 0070;
            break;
        case 'o':
            who |= 0007;
            break;
        case 'a':
            who |= 0777;
            break;
        default:
            break;
        }
    }
    if (who == 0) who = 0777;

    char op = *p;
    if (op != '+' && op != '-' && op != '=') return -1;
    p++;

    for (; *p == 'r' || *p == 'w' || *p == 'x'; p++) {
        switch (*p)
        {
        case 'r':
            perm |= 4;
            break;
        case 'w':
            perm |= 2;
            break;
        case 'x':
            perm |= 1;
            break;
        default:
            break;
        }
    }

    if (*p != '\0') return -1;

    perm_mask = ((perm << 6) | (perm << 3) | perm) & who;

    switch (op)
    {
    case '+':
        *bitmap |= perm_mask;
        break;
    case '-':
        *bitmap &= ~perm_mask;
        break;
    case '=':
        *bitmap = (*bitmap & ~who) | perm_mask;
        break;
    }
    *bitmap &= 0777;
    return 0;
}
