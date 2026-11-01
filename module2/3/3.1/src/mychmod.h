#include <stdio.h>

#ifndef MYCHMOD_H
#define MYCHMOD_H

int getPermissionsString(const mode_t bitmap, char* output);

int getPermissionsFromFile(const char *filename, mode_t *bitmap);
int getBitmap(const char* permissions, mode_t *bitmap);
int applyChmod(const char *cmd, mode_t *bitmap);
#endif