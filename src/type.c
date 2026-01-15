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

Type *type_create_slice(Type *element) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_SLICE;
    type->data.slice.element = element;
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

Type *type_create_struct(const char *name, Type **type_args, size_t type_arg_count) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_STRUCT;
    type->data.struct_enum.name = strdup(name);
    type->data.struct_enum.type_args = type_args;
    type->data.struct_enum.type_arg_count = type_arg_count;
    return type;
}

Type *type_create_enum(const char *name, Type **type_args, size_t type_arg_count) {
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_ENUM;
    type->data.struct_enum.name = strdup(name);
    type->data.struct_enum.type_args = type_args;
    type->data.struct_enum.type_arg_count = type_arg_count;
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
        case TYPE_SLICE:
            new_type->data.slice.element = type_clone(type->data.slice.element);
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
            new_type->data.struct_enum.name = strdup(type->data.struct_enum.name);
            new_type->data.struct_enum.type_arg_count = type->data.struct_enum.type_arg_count;
            if (new_type->data.struct_enum.type_arg_count > 0) {
                new_type->data.struct_enum.type_args = malloc(sizeof(Type*) * new_type->data.struct_enum.type_arg_count);
                for (size_t i = 0; i < new_type->data.struct_enum.type_arg_count; i++) {
                    new_type->data.struct_enum.type_args[i] = type_clone(type->data.struct_enum.type_args[i]);
                }
            } else {
                new_type->data.struct_enum.type_args = NULL;
            }
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
        case TYPE_SLICE:
            type_free(type->data.slice.element);
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
            free(type->data.struct_enum.name);
            for (size_t i = 0; i < type->data.struct_enum.type_arg_count; i++) {
                type_free(type->data.struct_enum.type_args[i]);
            }
            free(type->data.struct_enum.type_args);
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
        
        case TYPE_SLICE: {
            char *elem = type_to_string(type->data.slice.element);
            size_t len = strlen(elem) + 3;
            char *buf = malloc(len);
            snprintf(buf, len, "[]%s", elem);
            free(elem);
            return buf;
        }
        
        case TYPE_FUNCTION:
            return strdup("function");
            
        case TYPE_STRUCT:
        case TYPE_ENUM: {
            if (type->data.struct_enum.type_arg_count == 0) {
                return strdup(type->data.struct_enum.name);
            }
            // Estimate length: name + < + (arg + , )*arg_count + >
            size_t len = strlen(type->data.struct_enum.name) + 2;
            char **arg_strs = malloc(sizeof(char*) * type->data.struct_enum.type_arg_count);
            for (size_t i = 0; i < type->data.struct_enum.type_arg_count; i++) {
                arg_strs[i] = type_to_string(type->data.struct_enum.type_args[i]);
                len += strlen(arg_strs[i]) + 2;
            }
            
            char *buf = malloc(len + 1);
            strcpy(buf, type->data.struct_enum.name);
            strcat(buf, "<");
            for (size_t i = 0; i < type->data.struct_enum.type_arg_count; i++) {
                strcat(buf, arg_strs[i]);
                if (i < type->data.struct_enum.type_arg_count - 1) {
                    strcat(buf, ", ");
                }
                free(arg_strs[i]);
            }
            strcat(buf, ">");
            free(arg_strs);
            return buf;
        }
            
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
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_ENUM) {
        // First check if the name itself is a parameter
        for (size_t i = 0; i < count; i++) {
            if (strcmp(type->data.struct_enum.name, params[i]) == 0) {
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
            
        case TYPE_SLICE:
            new_type->data.slice.element = type_substitute(type->data.slice.element, params, args, count);
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
            new_type->data.struct_enum.name = strdup(type->data.struct_enum.name);
            new_type->data.struct_enum.type_arg_count = type->data.struct_enum.type_arg_count;
            if (new_type->data.struct_enum.type_arg_count > 0) {
                new_type->data.struct_enum.type_args = malloc(sizeof(Type*) * new_type->data.struct_enum.type_arg_count);
                for (size_t i = 0; i < new_type->data.struct_enum.type_arg_count; i++) {
                    new_type->data.struct_enum.type_args[i] = type_substitute(type->data.struct_enum.type_args[i], params, args, count);
                }
            } else {
                new_type->data.struct_enum.type_args = NULL;
            }
            break;
        case TYPE_RESULT:
            new_type->data.result.ok_type = type_substitute(type->data.result.ok_type, params, args, count);
            new_type->data.result.err_type = type_substitute(type->data.result.err_type, params, args, count);
            break;
    }
    
    return new_type;
}
