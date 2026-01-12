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
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_RESULT,
} TypeKind;

// Forward declaration
typedef struct Type Type;

// Type structure
struct Type {
    TypeKind kind;
    union {
        TokenType primitive;  // i32, f64, bool, etc.
        
        struct {
            Type *base;
            bool non_null;  // true for *!T, false for *T
        } pointer;
        
        struct {
            Type *element;
            size_t size;
        } array;
        
        struct {
            Type *return_type;
            Type **param_types;
            size_t param_count;
        } function;
        
        struct {
            Type *ok_type;
            Type *err_type;
        } result;
        
        char *name;  // For struct/enum names
    } data;
};

// Type functions
Type *type_create_primitive(TokenType primitive);
Type *type_create_pointer(Type *base, bool non_null);
Type *type_create_array(Type *element, size_t size);
Type *type_create_function(Type *return_type, Type **param_types, size_t param_count);
Type *type_create_struct(const char *name);
Type *type_create_enum(const char *name);
Type *type_create_result(Type *ok_type, Type *err_type);
Type *type_clone(const Type *type);
void type_free(Type *type);
char *type_to_string(const Type *type);
Type *type_substitute(const Type *type, char **params, Type **args, size_t count);

#endif // TYPE_H
