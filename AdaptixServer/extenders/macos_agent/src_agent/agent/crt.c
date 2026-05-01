#include "crt.h"
#include "types.h"

/// ARM64 macOS direct syscalls
/// BSD syscall convention: x16 = syscall number (0x2000000 | bsd_number)
/// x0-x5 = arguments, result in x0, carry flag set on error

#define SYS_exit    0x2000001
#define SYS_read    0x2000003
#define SYS_write   0x2000004
#define SYS_open    0x2000005
#define SYS_close   0x2000006
#define SYS_mmap    0x20000C5  // 197
#define SYS_munmap  0x2000049  // 73

static inline long raw_syscall6(long number, long a0, long a1, long a2, long a3, long a4, long a5) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    register long x2  __asm__("x2")  = a2;
    register long x3  __asm__("x3")  = a3;
    register long x4  __asm__("x4")  = a4;
    register long x5  __asm__("x5")  = a5;
    __asm__ volatile(
        "svc #0x80"
        : "+r"(x0)
        : "r"(x16), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall3(long number, long a0, long a1, long a2) {
    return raw_syscall6(number, a0, a1, a2, 0, 0, 0);
}

static inline long raw_syscall1(long number, long a0) {
    return raw_syscall6(number, a0, 0, 0, 0, 0, 0);
}

/// ---- Memory allocation via mmap/munmap ----

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x1000  // macOS: MAP_ANON = 0x1000
#define PROT_READ     0x01
#define PROT_WRITE    0x02

// Allocation header: store size for freeing
typedef struct {
    size_t total_size;
} alloc_header_t;

#define HEADER_SIZE  ((sizeof(alloc_header_t) + 15) & ~15)  // 16-byte aligned

void* ax_malloc(size_t size) {
    if (size == 0) return (void*)0;

    size_t total = HEADER_SIZE + size;
    // Round up to page size for clean munmap on macOS
    size_t page_total = (total + 4095) & ~4095UL;
    // mmap(NULL, page_total, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    long result = raw_syscall6(SYS_mmap, 0, (long)page_total, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result < 0 || result == 0)
        return (void*)0;

    alloc_header_t* header = (alloc_header_t*)result;
    header->total_size = page_total;
    return (void*)((uint8_t*)result + HEADER_SIZE);
}

void ax_free(void* ptr, size_t size) {
    (void)size;
    if (!ptr) return;
    alloc_header_t* header = (alloc_header_t*)((uint8_t*)ptr - HEADER_SIZE);
    size_t total = header->total_size;
    // Sanity check: total_size must be page-aligned and reasonable
    if (total == 0 || (total & 4095) != 0 || total > (1UL << 30)) {
        // Header corrupted — skip memset, just munmap with page-aligned size
        total = ((HEADER_SIZE + size) + 4095) & ~4095UL;
    } else {
        // Zero memory before freeing (OPSEC)
        ax_memset(ptr, 0, total - HEADER_SIZE);
    }
    raw_syscall3(SYS_munmap, (long)header, (long)total, 0);
}

void* ax_realloc(void* ptr, size_t old_size, size_t new_size) {
    if (!ptr) return ax_malloc(new_size);
    if (new_size == 0) {
        ax_free(ptr, old_size);
        return (void*)0;
    }

    void* new_ptr = ax_malloc(new_size);
    if (!new_ptr) return (void*)0;

    size_t copy_size = old_size < new_size ? old_size : new_size;
    ax_memcpy(new_ptr, ptr, copy_size);
    ax_free(ptr, old_size);
    return new_ptr;
}

/// ---- String/memory functions ----

void* ax_memset(void* dst, int val, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) *d++ = (uint8_t)val;
    return dst;
}

void* ax_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* ax_memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int ax_memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++;
        pb++;
    }
    return 0;
}

size_t ax_strlen(const char* s) {
    const char* p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int ax_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int ax_strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)a - *(unsigned char*)b;
}

char* ax_strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* ax_strncpy(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char* ax_strcat(char* dst, const char* src) {
    char* d = dst + ax_strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char* ax_strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    size_t nlen = ax_strlen(needle);
    while (*haystack) {
        if (ax_strncmp(haystack, needle, nlen) == 0)
            return (char*)haystack;
        haystack++;
    }
    return (char*)0;
}

char* ax_strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)0;
}

/// ---- Integer conversion ----

int ax_atoi(const char* s) {
    int result = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}

int ax_hextoi(const char* s) {
    unsigned int result = 0;
    while (*s == ' ') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (1) {
        char c = *s;
        if (c >= '0' && c <= '9') result = (result << 4) | (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') result = (result << 4) | (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') result = (result << 4) | (unsigned)(c - 'A' + 10);
        else break;
        s++;
    }
    return (int)result;
}

void ax_itoa(int val, char* buf, int base) {
    char tmp[32];
    int i = 0, neg = 0;

    if (val < 0 && base == 10) {
        neg = 1;
        val = -val;
    }

    unsigned int uval = (unsigned int)val;
    do {
        int digit = uval % base;
        tmp[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        uval /= base;
    } while (uval > 0);

    if (neg) tmp[i++] = '-';

    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/// ---- Random ----

int ax_random_bytes(void* buf, size_t len) {
    // Open /dev/urandom
    long fd = raw_syscall3(SYS_open, (long)"/dev/urandom", 0 /* O_RDONLY */, 0);
    if (fd < 0) return -1;

    size_t total = 0;
    while (total < len) {
        long n = raw_syscall3(SYS_read, fd, (long)((uint8_t*)buf + total), (long)(len - total));
        if (n <= 0) {
            raw_syscall1(SYS_close, fd);
            return -1;
        }
        total += n;
    }
    raw_syscall1(SYS_close, fd);
    return 0;
}

/// ---- Buffer (growable byte array) ----

int buf_init(buffer_t* b, size_t initial_cap) {
    b->data = (uint8_t*)ax_malloc(initial_cap);
    if (!b->data) return -1;
    b->len = 0;
    b->cap = initial_cap;
    return 0;
}

int buf_append(buffer_t* b, const void* data, size_t len) {
    if (b->len + len > b->cap) {
        size_t new_cap = b->cap * 2;
        while (new_cap < b->len + len) new_cap *= 2;
        uint8_t* new_data = (uint8_t*)ax_realloc(b->data, b->cap, new_cap);
        if (!new_data) return -1;
        b->data = new_data;
        b->cap = new_cap;
    }
    ax_memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}

void buf_free(buffer_t* b) {
    if (b->data) {
        ax_free(b->data, b->cap);
        b->data = (uint8_t*)0;
    }
    b->len = 0;
    b->cap = 0;
}

void buf_reset(buffer_t* b) {
    b->len = 0;
}
