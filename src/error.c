#include <stdio.h>
#include "error.h"

static int error_counter = 0;

void error_report(const char *filename, size_t line, size_t column, const char *message) {
    fprintf(stderr, "error: %s\n", message);
    fprintf(stderr, "  --> %s:%zu:%zu\n", filename, line, column);
    error_counter++;
}

int error_count(void) {
    return error_counter;
}

void error_clear(void) {
    error_counter = 0;
}
