#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "mychmod.h"

int printBinary(const mode_t bitmap) {
    char result[10];
    for (int i = 0; i < 9; i++) {
        result[i] = (bitmap & (1 << (8 - i))) ? '1' : '0';
    }
    result[9] = '\0';
    printf("Binary representation: %s\n", result);
    return 0;
}

int printAll(const mode_t bitmap) {
    printf("All representations of permissions:\n");

    char output[10];
    getPermissionsString(bitmap, output);
    printBinary(bitmap);
    printf("String representation: %s\n", output);
    printf("8-based number: %03o\n", bitmap);
}

int bin(char *input) {
    mode_t bitmap;
    getBitmap(input, &bitmap);
    printBinary(bitmap);
}

int show(char *filename) {
    mode_t bitmap;
    if (getPermissionsFromFile(filename, &bitmap) != 0) {
        printf("Unknown file!\n");
        return -1;
    }
    //буква цифра биты
    char output[10];
    getPermissionsString(bitmap, output);
    printAll(bitmap);
    return 0;
}

int chg(char *input, char *cmd) {
    mode_t bitmap;
    if (getBitmap(input, &bitmap) == -1) {
        int result = getPermissionsFromFile(input, &bitmap);
        if (result != 0) {
            return result;
        }
    }

    int result = applyChmod(cmd, &bitmap);
    if (result != 0) {
        printf("Applying chmod error!\n");
        return -1;
    }
    printAll(bitmap);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: [-bin|-show|-chg] <arguments>\n");
        return -1;
    }
    
    const char *flag = argv[1];
    
    if (strcmp(flag, "-bin") == 0) {
        return bin(argv[2]);
    }
    if (strcmp(flag, "-show") == 0) {
        return show(argv[2]);
    }
    if (strcmp(flag, "-chg") == 0) {
        return chg(argv[2], argv[3]);
    }
    return 0;
}