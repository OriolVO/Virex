#ifndef UTIL_H
#define UTIL_H

#include "type.h"

char *resolve_module_path(const char *current_file, const char *import_path);
char *util_mangle_name(const char *prefix, const char *name);
char *util_mangle_instantiation(const char *base_name, Type **type_args, size_t type_arg_count);

#endif
