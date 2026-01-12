#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

// Error reporting functions
void error_report(const char *filename, size_t line, size_t column, const char *message);
int error_count(void);
void error_clear(void);

#endif // ERROR_H
