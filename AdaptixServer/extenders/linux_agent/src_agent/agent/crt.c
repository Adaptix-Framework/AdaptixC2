#include "crt.h"
#include "types.h"

// Include arch-specific syscall wrappers
#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// Linux mmap constants
#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define MAP_PRIVATE  0x02
#define MAP_ANONYMOUS 0x20  // Linux: 0x20 (macOS: 0x1000)
#define MAP_FAILED   ((void*)-1)
#define O_RDONLY     0

// Allocation header for size tracking
typedef struct {
    size_t total_size;
    size_t _pad[1]; // Align to 16 bytes
} alloc_header_t;

#define HEADER_SIZE sizeof(alloc_header_t)

// Page alignment helper
static inline size_t align_page(size_t size) {
    return (size + 4095) & ~(size_t)4095;
}

/// ax_malloc — allocate via mmap syscall (zero libc dependency)
void *ax_malloc(size_t size) {
    if (size == 0) return NULL;

    size_t total = align_page(HEADER_SIZE + size);
    void *ptr = sys_mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return NULL;

    alloc_header_t *hdr = (alloc_header_t *)ptr;
    hdr->total_size = total;
    return (uint8_t *)ptr + HEADER_SIZE;
}

/// ax_free — OPSEC: zero memory before munmap
void ax_free(void *ptr) {
    if (!ptr) return;

    alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    size_t total = hdr->total_size;

    // Sanity check
    if (total < HEADER_SIZE || total > (size_t)256 * 1024 * 1024) return;

    // OPSEC: Zero memory before releasing
    volatile uint8_t *p = (volatile uint8_t *)hdr;
    for (size_t i = 0; i < total; i++) p[i] = 0;

    sys_munmap(hdr, total);
}

/// ax_realloc — malloc new + copy + free old
void *ax_realloc(void *ptr, size_t new_size) {
    if (!ptr) return ax_malloc(new_size);
    if (new_size == 0) { ax_free(ptr); return NULL; }

    alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    size_t old_data_size = hdr->total_size - HEADER_SIZE;

    void *new_ptr = ax_malloc(new_size);
    if (!new_ptr) return NULL;

    size_t copy_size = old_data_size < new_size ? old_data_size : new_size;
    ax_memcpy(new_ptr, ptr, copy_size);
    ax_free(ptr);

    return new_ptr;
}

/// Memory operations

void *ax_memset(void *s, int c, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *ax_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *ax_memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int ax_memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

/// String operations

size_t ax_strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int ax_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int ax_strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)a - *(unsigned char *)b;
}

char *ax_strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *ax_strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = 0;
    return ret;
}

char *ax_strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

char *ax_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && (*h == *n)) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

char *ax_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return NULL;
}

/// Integer conversion

int ax_atoi(const char *s) {
    int result = 0, sign = 1;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result * sign;
}

int ax_hextoi(const char *s) {
    int result = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        result = result * 16 + digit;
        s++;
    }
    return result;
}

char *ax_itoa(int val, char *buf, int base) {
    char *p = buf;
    char *start;
    int neg = 0;

    if (val < 0 && base == 10) {
        neg = 1;
        val = -val;
    }

    start = p;
    do {
        int d = val % base;
        *p++ = (d < 10) ? '0' + d : 'a' + d - 10;
        val /= base;
    } while (val);

    if (neg) *p++ = '-';
    *p = 0;

    // Reverse
    char *end = p - 1;
    char *beg = start;
    while (beg < end) {
        char tmp = *beg;
        *beg++ = *end;
        *end-- = tmp;
    }

    return buf;
}

/// Random bytes — reads /dev/urandom via direct syscall
int ax_random_bytes(void *buf, size_t len) {
    // Try getrandom syscall first (more OPSEC — no file open)
    long ret = sys_getrandom(buf, len, 0);
    if (ret == (long)len) return 0;

    // Fallback: /dev/urandom
    int fd = sys_open("/dev/urandom", O_RDONLY, 0);
    if (fd < 0) return -1;

    size_t total = 0;
    while (total < len) {
        ret = sys_read(fd, (uint8_t *)buf + total, len - total);
        if (ret <= 0) { sys_close(fd); return -1; }
        total += ret;
    }
    sys_close(fd);
    return 0;
}

/// GCC builtins — required for ARM64 (and sometimes x86_64) when the compiler
/// emits implicit memset/memcpy/memmove for struct/array initialization.
void *memset(void *s, int c, size_t n)  { return ax_memset(s, c, n); }
void *memcpy(void *d, const void *s, size_t n)  { return ax_memcpy(d, s, n); }
void *memmove(void *d, const void *s, size_t n) { return ax_memmove(d, s, n); }

/// buffer_t implementation

void buf_init(buffer_t *b, int initial_cap) {
    b->data = (uint8_t *)ax_malloc(initial_cap);
    b->len = 0;
    b->cap = b->data ? initial_cap : 0;
}

void buf_append(buffer_t *b, const void *data, int len) {
    if (b->len + len > b->cap) {
        int new_cap = b->cap * 2;
        if (new_cap < b->len + len) new_cap = b->len + len;
        uint8_t *new_data = (uint8_t *)ax_realloc(b->data, new_cap);
        if (!new_data) return;
        b->data = new_data;
        b->cap = new_cap;
    }
    ax_memcpy(b->data + b->len, data, len);
    b->len += len;
}

void buf_free(buffer_t *b) {
    if (b->data) ax_free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void buf_reset(buffer_t *b) {
    b->len = 0;
}
