#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include "token.h"

// Type kinds
typedef enum {
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_SLICE,      // Slice type []T
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_FUNCTION,
    TYPE_RESULT,
} TypeKind;

// Forward declaration
typedef struct Type Type;

// Type structure
struct Type {
    TypeKind kind;
    union {
        TokenType primitive;           // For TYPE_PRIMITIVE
        char *primitive_name;          // For named primitives (used during parsing)
        struct {
            struct Type *base;
            bool non_null;             // Non-null pointer type (*!T)
        } pointer;
        struct {
            struct Type *element;
            size_t size;               // Array size
        } array;
        struct {
            struct Type *element;      // Element type for slice []T
        } slice;
        struct {
            char *name;
            struct Type **type_args;   // Generic type arguments
            size_t type_arg_count;
        } struct_enum;                 // Used for both structs and enums
        struct {
            struct Type *return_type;
            struct Type **param_types;
            size_t param_count;
        } function;
        struct {
            struct Type *ok_type;
            struct Type *err_type;
        } result;
    } data;
};

// Type functions
Type *type_create_primitive(TokenType primitive);
Type *type_create_pointer(Type *base, bool non_null);
Type *type_create_array(Type *element, size_t size);
Type *type_create_slice(Type *element);
Type *type_create_function(Type *return_type, Type **param_types, size_t param_count);
Type *type_create_struct(const char *name, Type **type_args, size_t type_arg_count);
Type *type_create_enum(const char *name, Type **type_args, size_t type_arg_count);
Type *type_create_result(Type *ok_type, Type *err_type);
Type *type_clone(const Type *type);
void type_free(Type *type);
char *type_to_string(const Type *type);
Type *type_substitute(const Type *type, char **params, Type **args, size_t count);

#endif // TYPE_H
