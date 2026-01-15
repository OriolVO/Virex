#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include "../include/util.h"

char *resolve_module_path(const char *current_file, const char *import_path) {
    if (!current_file || !import_path) return NULL;

    // 1. Try relative to current_file
    char *dir_temp = strdup(current_file);
    char *dname = dirname(dir_temp);
    
    char resolved[512];
    snprintf(resolved, sizeof(resolved), "%s/%s", dname, import_path);
    
    struct stat st;
    if (stat(resolved, &st) == 0) {
        free(dir_temp);
        char *rp = realpath(resolved, NULL);
        if (rp) return rp;
        return strdup(resolved);
    }
    
    free(dir_temp);

    // 2. Try relative to CWD (for std/ffi.vx etc)
    if (stat(import_path, &st) == 0) {
        char *rp = realpath(import_path, NULL);
        if (rp) return rp;
        return strdup(import_path);
    }

    // 3. Try stdlib/
    snprintf(resolved, sizeof(resolved), "stdlib/%s", import_path);
    if (stat(resolved, &st) == 0) {
        char *rp = realpath(resolved, NULL);
        if (rp) return rp;
        return strdup(resolved);
    }
    
    return NULL;
}
