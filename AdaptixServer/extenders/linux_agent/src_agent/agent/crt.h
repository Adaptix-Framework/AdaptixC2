#ifndef CRT_H
#define CRT_H

#include <stddef.h>
#include <stdint.h>

/// Memory allocation (via direct mmap/munmap syscalls — zero libc dependency)
void *ax_malloc(size_t size);
void  ax_free(void *ptr);
void *ax_realloc(void *ptr, size_t new_size);

/// Memory operations
void *ax_memset(void *s, int c, size_t n);
void *ax_memcpy(void *dst, const void *src, size_t n);
void *ax_memmove(void *dst, const void *src, size_t n);
int   ax_memcmp(const void *a, const void *b, size_t n);

/// String operations
size_t ax_strlen(const char *s);
int    ax_strcmp(const char *a, const char *b);
int    ax_strncmp(const char *a, const char *b, size_t n);
char  *ax_strcpy(char *dst, const char *src);
char  *ax_strncpy(char *dst, const char *src, size_t n);
char  *ax_strcat(char *dst, const char *src);
char  *ax_strstr(const char *haystack, const char *needle);
char  *ax_strchr(const char *s, int c);

/// Integer conversion
int    ax_atoi(const char *s);
int    ax_hextoi(const char *s);
char  *ax_itoa(int val, char *buf, int base);

/// Random bytes (reads /dev/urandom via syscall)
int    ax_random_bytes(void *buf, size_t len);

/// Formatted output (nostdlib vsnprintf)
#include <stdarg.h>
int    ax_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int    ax_snprintf(char *buf, size_t size, const char *fmt, ...);

#endif // CRT_H
