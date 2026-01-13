#include <stdio.h>
#include <string.h>
#include "../include/error.h"

static int error_counter = 0;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void error_report(const char *filename, size_t line, size_t column, const char *message) {
    error_report_ex(LEVEL_ERROR, NULL, filename, line, column, message, NULL);
}

void error_report_ex(ErrorLevel level, const char *code, const char *filename, size_t line, size_t column, 
                     const char *message, const char *suggestion) {
    const char *level_str = "error";
    const char *level_color = ANSI_COLOR_RED;
    
    if (level == LEVEL_WARNING) {
        level_str = "warning";
        level_color = ANSI_COLOR_YELLOW;
    } else if (level == LEVEL_NOTE) {
        level_str = "note";
        level_color = ANSI_COLOR_BLUE;
    }
    
    // Print header: level[code]: message
    fprintf(stderr, "%s%s%s", ANSI_COLOR_BOLD, level_color, level_str);
    if (code) {
        fprintf(stderr, "[%s]", code);
    }
    fprintf(stderr, ": %s%s\n", message, ANSI_COLOR_RESET);
    
    // Print location
    fprintf(stderr, "  %s-->%s %s:%zu:%zu\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, filename, line, column);
    
    // Print code snippet
    if (filename && filename[0] != '\0') {
        FILE *file = fopen(filename, "r");
        if (file) {
            char buffer[1024];
            size_t current_line = 1;
            while (fgets(buffer, sizeof(buffer), file)) {
                if (current_line == line) {
                    // Print line number and the source line
                    fprintf(stderr, "%5zu | %s", line, buffer);
                    
                    // If buffer didn't end with newline, add one
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len-1] != '\n') {
                        fprintf(stderr, "\n");
                    }
                    
                    // Print marker
                    fprintf(stderr, "      | %s", level_color);
                    for (size_t i = 1; i < column; i++) {
                        fprintf(stderr, " ");
                    }
                    fprintf(stderr, "^~~~%s\n", ANSI_COLOR_RESET);
                    break;
                }
                current_line++;
            }
            fclose(file);
        }
    }
    
    // Print suggestion
    if (suggestion) {
        fprintf(stderr, "  %shelp:%s %s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET, suggestion);
    }
    fprintf(stderr, "\n");

    if (level == LEVEL_ERROR) {
        error_counter++;
    }
}

int error_count(void) {
    return error_counter;
}

void error_clear(void) {
    error_counter = 0;
}
