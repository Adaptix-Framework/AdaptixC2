#ifndef CRT_H
#define CRT_H

#include <stddef.h>
#include <stdint.h>

/// Minimal custom runtime — no libc dependency for core operations
/// Memory allocation uses mmap/munmap directly

void* ax_malloc(size_t size);
void  ax_free(void* ptr, size_t size);
void* ax_realloc(void* ptr, size_t old_size, size_t new_size);

void* ax_memset(void* dst, int val, size_t n);
void* ax_memcpy(void* dst, const void* src, size_t n);
void* ax_memmove(void* dst, const void* src, size_t n);
int   ax_memcmp(const void* a, const void* b, size_t n);

size_t ax_strlen(const char* s);
int    ax_strcmp(const char* a, const char* b);
int    ax_strncmp(const char* a, const char* b, size_t n);
char*  ax_strcpy(char* dst, const char* src);
char*  ax_strncpy(char* dst, const char* src, size_t n);
char*  ax_strcat(char* dst, const char* src);
char*  ax_strstr(const char* haystack, const char* needle);
char*  ax_strchr(const char* s, int c);

/// Integer conversion
int  ax_atoi(const char* s);
int  ax_hextoi(const char* s);
void ax_itoa(int val, char* buf, int base);

/// Random bytes (reads /dev/urandom)
int ax_random_bytes(void* buf, size_t len);

#endif // CRT_H
