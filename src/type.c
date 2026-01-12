#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/type.h"

Type *type_create_primitive(TokenType primitive) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_PRIMITIVE;
    type->data.primitive = primitive;
    return type;
}

Type *type_create_pointer(Type *base, bool non_null) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_POINTER;
    type->data.pointer.base = base;
    type->data.pointer.non_null = non_null;
    return type;
}

Type *type_create_array(Type *element, size_t size) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_ARRAY;
    type->data.array.element = element;
    type->data.array.size = size;
    return type;
}

Type *type_create_function(Type *return_type, Type **param_types, size_t param_count) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_FUNCTION;
    type->data.function.return_type = return_type;
    type->data.function.param_types = param_types;
    type->data.function.param_count = param_count;
    return type;
}

Type *type_create_struct(const char *name) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_STRUCT;
    type->data.name = strdup(name);
    return type;
}

Type *type_create_enum(const char *name) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_ENUM;
    type->data.name = strdup(name);
    return type;
}

Type *type_create_result(Type *ok_type, Type *err_type) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_RESULT;
    type->data.result.ok_type = ok_type;
    type->data.result.err_type = err_type;
    return type;
}

Type *type_clone(const Type *type) {
    if (!type) return NULL;
    
    Type *new_type = malloc(sizeof(Type));
    new_type->kind = type->kind;
    
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            new_type->data.primitive = type->data.primitive;
            break;
        case TYPE_POINTER:
            new_type->data.pointer.base = type_clone(type->data.pointer.base);
            new_type->data.pointer.non_null = type->data.pointer.non_null;
            break;
        case TYPE_ARRAY:
            new_type->data.array.element = type_clone(type->data.array.element);
            new_type->data.array.size = type->data.array.size;
            break;
        case TYPE_FUNCTION:
            new_type->data.function.return_type = type_clone(type->data.function.return_type);
            new_type->data.function.param_count = type->data.function.param_count;
            new_type->data.function.param_types = malloc(sizeof(Type*) * type->data.function.param_count);
            for (size_t i = 0; i < type->data.function.param_count; i++) {
                new_type->data.function.param_types[i] = type_clone(type->data.function.param_types[i]);
            }
            break;
        case TYPE_STRUCT:
        case TYPE_ENUM:
            new_type->data.name = strdup(type->data.name);
            break;
        case TYPE_RESULT:
            new_type->data.result.ok_type = type_clone(type->data.result.ok_type);
            new_type->data.result.err_type = type_clone(type->data.result.err_type);
            break;
    }
    
    return new_type;
}

void type_free(Type *type) {
    if (!type) return;
    
    switch (type->kind) {
        case TYPE_POINTER:
            type_free(type->data.pointer.base);
            break;
        case TYPE_ARRAY:
            type_free(type->data.array.element);
            break;
        case TYPE_FUNCTION:
            type_free(type->data.function.return_type);
            for (size_t i = 0; i < type->data.function.param_count; i++) {
                type_free(type->data.function.param_types[i]);
            }
            free(type->data.function.param_types);
            break;
        case TYPE_STRUCT:
        case TYPE_ENUM:
            free(type->data.name);
            break;
        case TYPE_RESULT:
            type_free(type->data.result.ok_type);
            type_free(type->data.result.err_type);
            break;
        case TYPE_PRIMITIVE:
            break;
    }
    
    free(type);
}

char *type_to_string(const Type *type) {
    if (!type) return strdup("unknown");
    
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            return strdup(token_type_name(type->data.primitive));
            
        case TYPE_POINTER: {
            char *base = type_to_string(type->data.pointer.base);
            size_t len = strlen(base) + 10;
            char *buf = malloc(len);
            snprintf(buf, len, "%s%s*", base, type->data.pointer.non_null ? "!" : "");
            free(base);
            return buf;
        }
        
        case TYPE_ARRAY: {
            char *elem = type_to_string(type->data.array.element);
            size_t len = strlen(elem) + 32;
            char *buf = malloc(len);
            snprintf(buf, len, "%s[%zu]", elem, type->data.array.size);
            free(elem);
            return buf;
        }
        
        case TYPE_FUNCTION:
            return strdup("function");
            
        case TYPE_STRUCT:
            return strdup(type->data.name);
            
        case TYPE_ENUM:
            return strdup(type->data.name);
            
        case TYPE_RESULT: {
            char *ok = type_to_string(type->data.result.ok_type);
            char *err = type_to_string(type->data.result.err_type);
            size_t len = strlen(ok) + strlen(err) + 16;
            char *buf = malloc(len);
            snprintf(buf, len, "result<%s, %s>", ok, err);
            free(ok);
            free(err);
            return buf;
        }
    }
    
    return strdup("unknown");
}

Type *type_substitute(const Type *type, char **params, Type **args, size_t count) {
    if (!type) return NULL;
    
    // Check if this type is one of the generic parameters
    if (type->kind == TYPE_STRUCT) {  // Generic types are parsed as struct types with the param name
        for (size_t i = 0; i < count; i++) {
            if (strcmp(type->data.name, params[i]) == 0) {
                return type_clone(args[i]);
            }
        }
    }
    
    // Otherwise, deep clone and recurse
    Type *new_type = malloc(sizeof(Type));
    new_type->kind = type->kind;
    
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            new_type->data.primitive = type->data.primitive;
            break;
            
        case TYPE_POINTER:
            new_type->data.pointer.base = type_substitute(type->data.pointer.base, params, args, count);
            new_type->data.pointer.non_null = type->data.pointer.non_null;
            break;
            
        case TYPE_ARRAY:
            new_type->data.array.element = type_substitute(type->data.array.element, params, args, count);
            new_type->data.array.size = type->data.array.size;
            break;
            
        case TYPE_FUNCTION:
            new_type->data.function.return_type = type_substitute(type->data.function.return_type, params, args, count);
            new_type->data.function.param_count = type->data.function.param_count;
            new_type->data.function.param_types = malloc(sizeof(Type*) * type->data.function.param_count);
            for (size_t i = 0; i < type->data.function.param_count; i++) {
                new_type->data.function.param_types[i] = type_substitute(type->data.function.param_types[i], params, args, count);
            }
            break;
            
        case TYPE_STRUCT:
        case TYPE_ENUM:
            new_type->data.name = strdup(type->data.name);
            break;
        case TYPE_RESULT:
            new_type->data.result.ok_type = type_substitute(type->data.result.ok_type, params, args, count);
            new_type->data.result.err_type = type_substitute(type->data.result.err_type, params, args, count);
            break;
    }
    
    return new_type;
}
