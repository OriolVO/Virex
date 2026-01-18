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

char *util_mangle_name(const char *prefix, const char *name) {
    if (!name) return NULL;
    if (!prefix) return strdup(name);
    
    size_t prefix_len = strlen(prefix);
    char *mangled = malloc(prefix_len + strlen(name) + 3); // +2 for "__", +1 for null
    sprintf(mangled, "%s__%s", prefix, name);
    
    // Sanitize prefix (dots and colons to underscores)
    for (char *p = mangled; p < mangled + prefix_len; p++) {
        if (*p == '.' || *p == ':') *p = '_';
    }
    
    return mangled;
}

char *util_mangle_instantiation(const char *base_name, Type **type_args, size_t type_arg_count) {
    if (type_arg_count == 0 || !type_args) {
        return strdup(base_name);
    }
    
    // Calculate required buffer size
    size_t size = strlen(base_name) + 1;
    for (size_t i = 0; i < type_arg_count; i++) {
        char *type_str = type_to_string(type_args[i]);
        size += strlen(type_str) + 1; // +1 for underscore
        free(type_str);
    }
    
    char *mangled = malloc(size);
    strcpy(mangled, base_name);
    
    for (size_t i = 0; i < type_arg_count; i++) {
        strcat(mangled, "_");
        char *type_str = type_to_string(type_args[i]);
        strcat(mangled, type_str);
        free(type_str);
    }
    
    return mangled;
}
