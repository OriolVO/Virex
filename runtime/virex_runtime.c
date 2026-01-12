// Virex Standard Library - Runtime Support
// This file provides essential runtime functions for Virex programs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// std::mem - Memory Management
// ============================================================================

// Allocate memory for count elements of size bytes
void* virex_alloc(long long size, long long count) {
    if (size <= 0 || count <= 0) return NULL;
    return malloc((size_t)size * (size_t)count);
}

// Free allocated memory
void virex_free(void* ptr) {
    free(ptr);
}

// Copy memory from src to dst
void virex_copy(void* dst, const void* src, long long count) {
    if (!dst || !src || count <= 0) return;
    memcpy(dst, src, (size_t)count);
}

// Set memory to a value
void virex_set(void* dst, int value, long long count) {
    if (!dst || count <= 0) return;
    memset(dst, value, (size_t)count);
}

// ============================================================================
// std::io - Input/Output
// ============================================================================

// Print integer
void virex_print_i32(int value) {
    printf("%d", value);
}

// Print integer with newline
void virex_println_i32(int value) {
    printf("%d\n", value);
}

// Print boolean
void virex_print_bool(int value) {
    printf("%s", value ? "true" : "false");
}

// Print boolean with newline
void virex_println_bool(int value) {
    printf("%s\n", value ? "true" : "false");
}

// Print string (cstring)
void virex_print_str(const char* str) {
    if (str) printf("%s", str);
}

// Print string with newline
void virex_println_str(const char* str) {
    if (str) printf("%s\n", str);
    else printf("\n");
}

// ============================================================================
// std::os - Operating System
// ============================================================================

// Exit program with code
void virex_exit(int code) {
    exit(code);
}

// Get command line argument count
int virex_argc = 0;
char** virex_argv = NULL;

void virex_init_args(int argc, char** argv) {
    virex_argc = argc;
    virex_argv = argv;
}

int virex_get_argc(void) {
    return virex_argc;
}

char* virex_get_argv(int index) {
    if (index < 0 || index >= virex_argc) return NULL;
    return virex_argv[index];
}
