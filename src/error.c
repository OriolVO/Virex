#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/error.h"

static int error_counter = 0;

// ANSI Color Codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Keywords for syntax highlighting
static const char *KEYWORDS[] = {
    "func", "var", "const", "return", "struct", "enum", "if", "else", 
    "while", "for", "break", "continue", "import", "package", "public", 
    "extern", "unsafe", "match", "fail", "null", "true", "false", NULL
};

static const char *TYPES[] = {
    "void", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", 
    "f32", "f64", "bool", "cstring", "result", "thread", "mutex", 
    "rwlock", "atomic_i32", "atomic_u64", NULL
};

static int is_keyword(const char *word, size_t len) {
    for (int i = 0; KEYWORDS[i]; i++) {
        if (strlen(KEYWORDS[i]) == len && strncmp(word, KEYWORDS[i], len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_type(const char *word, size_t len) {
    for (int i = 0; TYPES[i]; i++) {
        if (strlen(TYPES[i]) == len && strncmp(word, TYPES[i], len) == 0) {
            return 1;
        }
    }
    return 0;
}

// Simple syntax highlighter for a single line
static void print_highlighted_line(const char *line) {
    const char *p = line;
    while (*p) {
        if (isspace(*p)) {
            fputc(*p, stderr);
            p++;
        } else if (isalpha(*p) || *p == '_') {
            const char *start = p;
            while (isalnum(*p) || *p == '_') p++;
            size_t len = p - start;
            
            if (is_keyword(start, len)) {
                fprintf(stderr, "%s%.*s%s", ANSI_COLOR_MAGENTA, (int)len, start, ANSI_COLOR_RESET);
            } else if (is_type(start, len)) {
                fprintf(stderr, "%s%.*s%s", ANSI_COLOR_CYAN, (int)len, start, ANSI_COLOR_RESET);
            } else {
                fprintf(stderr, "%.*s", (int)len, start);
            }
        } else if (*p == '"') {
            fprintf(stderr, "%s", ANSI_COLOR_GREEN);
            fputc(*p, stderr);
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) {
                    fputc(*p, stderr);
                    p++;
                }
                fputc(*p, stderr);
                p++;
            }
            if (*p == '"') {
                fputc(*p, stderr);
                p++;
            }
            fprintf(stderr, "%s", ANSI_COLOR_RESET);
        } else if (isdigit(*p)) {
            fprintf(stderr, "%s", ANSI_COLOR_YELLOW);
            while (isdigit(*p) || *p == '.' || *p == 'x' || *p == 'b') {
                fputc(*p, stderr);
                p++;
            }
            fprintf(stderr, "%s", ANSI_COLOR_RESET);
        } else if (strchr("+-*/%=&|<>!^:;,{}[]()", *p)) {
             fprintf(stderr, "%s%c%s", ANSI_COLOR_WHITE, *p, ANSI_COLOR_RESET);
             p++;
        } else {
            fputc(*p, stderr);
            p++;
        }
    }
}

// Levenshtein distance for fuzzy string matching
static int min3(int a, int b, int c) {
    int min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    return min;
}

static int levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    // Create distance matrix
    int *matrix = malloc((len1 + 1) * (len2 + 1) * sizeof(int));
    
    // Initialize first row and column
    for (size_t i = 0; i <= len1; i++) {
        matrix[i * (len2 + 1)] = i;
    }
    for (size_t j = 0; j <= len2; j++) {
        matrix[j] = j;
    }
    
    // Fill matrix
    for (size_t i = 1; i <= len1; i++) {
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            matrix[i * (len2 + 1) + j] = min3(
                matrix[(i-1) * (len2 + 1) + j] + 1,      // deletion
                matrix[i * (len2 + 1) + (j-1)] + 1,      // insertion
                matrix[(i-1) * (len2 + 1) + (j-1)] + cost // substitution
            );
        }
    }
    
    int result = matrix[len1 * (len2 + 1) + len2];
    free(matrix);
    return result;
}

// Find closest match from a list of candidates
// Returns NULL if no good match found (distance > 3)
char *find_closest_match(const char *target, const char **candidates, size_t count) {
    if (!target || !candidates || count == 0) return NULL;
    
    int best_distance = 999;
    const char *best_match = NULL;
    
    for (size_t i = 0; i < count; i++) {
        if (!candidates[i]) continue;
        int dist = levenshtein_distance(target, candidates[i]);
        if (dist < best_distance) {
            best_distance = dist;
            best_match = candidates[i];
        }
    }
    
    // Only suggest if distance is reasonable (max 3 edits)
    if (best_distance <= 3 && best_match) {
        return strdup((char*)best_match);
    }
    
    return NULL;
}

void error_report(const char *filename, size_t line, size_t column, const char *message) {
    error_report_ex(LEVEL_ERROR, NULL, filename, line, column, message, NULL, NULL);
}

void error_report_ex(ErrorLevel level, const char *code, const char *filename, size_t line, size_t column, 
                     const char *message, const char *suggestion, const char *note) {
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
    if (filename) {
        fprintf(stderr, "  %s-->%s %s:%zu:%zu\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, filename, line, column);
    }
    
    // Print code snippet
    if (filename && filename[0] != '\0') {
        FILE *file = fopen(filename, "r");
        if (file) {
            char buffer[1024];
            size_t current_line = 1;
            int context_lines = 0; // Future improvement: lines context
            
            while (fgets(buffer, sizeof(buffer), file)) {
                if (current_line == line) {
                    // Print line number and separator
                    fprintf(stderr, "%s%5zu | %s", ANSI_COLOR_BLUE, line, ANSI_COLOR_RESET);
                    
                    // Highlight syntax
                    print_highlighted_line(buffer);
                    
                    // Ensure newline at end of line
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len-1] != '\n') {
                        fprintf(stderr, "\n");
                    }
                    
                    // Print caret
                    fprintf(stderr, "      %s|%s ", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
                    
                    // Calculate precise caret position handling tabs
                    // Ideally we'd scan buffer for tabs, but for now simple spacing
                    fprintf(stderr, "%s", level_color);
                    for (size_t i = 1; i < column; i++) {
                        // If the char at this pos was a tab, we should also print a tab (or equivalent spaces)
                        // but print_highlighted_line didn't expand tabs. Assuming standard tab stops or 1-width for now.
                        // Simple 1-to-1 mapping for now.
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
        fprintf(stderr, "  %s=%s help: %s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, suggestion);
    }
    
    // Print note
    if (note) {
        fprintf(stderr, "  %s=%s note: %s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET, note);
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
