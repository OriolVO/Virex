#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

typedef enum {
    LEVEL_ERROR,
    LEVEL_WARNING,
    LEVEL_NOTE
} ErrorLevel;

// Error reporting functions
void error_report(const char *filename, size_t line, size_t column, const char *message);
void error_report_ex(ErrorLevel level, const char *code, const char *filename, size_t line, size_t column, 
                     const char *message, const char *suggestion, const char *note);
int error_count(void);
void error_clear(void);

#endif // ERROR_H
