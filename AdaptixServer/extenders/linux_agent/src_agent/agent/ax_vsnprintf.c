/// ax_vsnprintf.c — Minimal vsnprintf for nostdlib environment
/// Supports: %s %d %i %u %x %X %p %c %% %ld %lu %lx %lX
/// Supports: width, zero-padding, left-align (-)
/// No float support (not needed for BOFs)

#include "crt.h"
#include <stdarg.h>

/// Write a single character to buffer (with bounds check)
static inline int out_char(char *buf, size_t pos, size_t size, char c) {
    if (pos < size - 1)
        buf[pos] = c;
    return 1;
}

/// Write a string to buffer
static int out_str(char *buf, size_t pos, size_t size, const char *s, int width, int left_align) {
    int written = 0;
    int slen = 0;
    const char *p = s;

    if (!s) s = "(null)";
    p = s;
    while (*p) { slen++; p++; }

    int pad = (width > slen) ? width - slen : 0;

    if (!left_align) {
        for (int i = 0; i < pad; i++)
            written += out_char(buf, pos + written, size, ' ');
    }
    for (int i = 0; i < slen; i++)
        written += out_char(buf, pos + written, size, s[i]);
    if (left_align) {
        for (int i = 0; i < pad; i++)
            written += out_char(buf, pos + written, size, ' ');
    }
    return written;
}

/// Write an unsigned integer (base 10 or 16)
static int out_uint(char *buf, size_t pos, size_t size,
                    unsigned long long val, int base, int upper,
                    int width, char pad_char, int left_align) {
    char tmp[24];  // enough for 64-bit
    int  idx = 0;
    int  written = 0;

    if (val == 0) {
        tmp[idx++] = '0';
    } else {
        while (val > 0) {
            int d = val % base;
            if (d < 10)
                tmp[idx++] = '0' + d;
            else
                tmp[idx++] = (upper ? 'A' : 'a') + d - 10;
            val /= base;
        }
    }

    int numlen = idx;
    int pad = (width > numlen) ? width - numlen : 0;

    if (!left_align) {
        for (int i = 0; i < pad; i++)
            written += out_char(buf, pos + written, size, pad_char);
    }
    // digits in reverse
    for (int i = idx - 1; i >= 0; i--)
        written += out_char(buf, pos + written, size, tmp[i]);
    if (left_align) {
        for (int i = 0; i < pad; i++)
            written += out_char(buf, pos + written, size, ' ');
    }
    return written;
}

/// Write a signed integer
static int out_int(char *buf, size_t pos, size_t size,
                   long long val, int width, char pad_char, int left_align) {
    int written = 0;
    int negative = 0;

    if (val < 0) {
        negative = 1;
        val = -val;
        if (pad_char == '0' && !left_align) {
            written += out_char(buf, pos + written, size, '-');
            width--;  // sign takes one slot
        }
    }

    if (negative && pad_char != '0') {
        // Count digits to determine padding
        char tmp[24];
        int idx = 0;
        long long v = val;
        if (v == 0) { tmp[idx++] = '0'; }
        else { while (v > 0) { tmp[idx++] = '0' + (v % 10); v /= 10; } }
        int numlen = idx + 1;  // +1 for sign
        int pad = (width > numlen) ? width - numlen : 0;

        if (!left_align) {
            for (int i = 0; i < pad; i++)
                written += out_char(buf, pos + written, size, ' ');
        }
        written += out_char(buf, pos + written, size, '-');
        for (int i = idx - 1; i >= 0; i--)
            written += out_char(buf, pos + written, size, tmp[i]);
        if (left_align) {
            for (int i = 0; i < pad; i++)
                written += out_char(buf, pos + written, size, ' ');
        }
        return written;
    }

    if (negative && pad_char == '0') {
        // sign already written above
        written += out_uint(buf, pos + written, size, (unsigned long long)val, 10, 0, width, pad_char, left_align);
    } else {
        written += out_uint(buf, pos + written, size, (unsigned long long)val, 10, 0, width, pad_char, left_align);
    }
    return written;
}

int ax_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;

    if (!buf || size == 0)
        return 0;

    while (*fmt) {
        if (*fmt != '%') {
            pos += out_char(buf, pos, size, *fmt);
            fmt++;
            continue;
        }

        fmt++;  // skip '%'

        // Parse flags
        int left_align = 0;
        char pad_char = ' ';

        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left_align = 1;
            if (*fmt == '0' && !left_align) pad_char = '0';
            fmt++;
        }

        // Parse width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        // Parse length modifier
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                is_long = 2;
                fmt++;
            }
        }

        // Parse conversion
        switch (*fmt) {
        case 'd':
        case 'i': {
            long long val;
            if (is_long >= 2)
                val = va_arg(ap, long long);
            else if (is_long == 1)
                val = va_arg(ap, long);
            else
                val = va_arg(ap, int);
            pos += out_int(buf, pos, size, val, width, pad_char, left_align);
            break;
        }
        case 'u': {
            unsigned long long val;
            if (is_long >= 2)
                val = va_arg(ap, unsigned long long);
            else if (is_long == 1)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            pos += out_uint(buf, pos, size, val, 10, 0, width, pad_char, left_align);
            break;
        }
        case 'x': {
            unsigned long long val;
            if (is_long >= 2)
                val = va_arg(ap, unsigned long long);
            else if (is_long == 1)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            pos += out_uint(buf, pos, size, val, 16, 0, width, pad_char, left_align);
            break;
        }
        case 'X': {
            unsigned long long val;
            if (is_long >= 2)
                val = va_arg(ap, unsigned long long);
            else if (is_long == 1)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            pos += out_uint(buf, pos, size, val, 16, 1, width, pad_char, left_align);
            break;
        }
        case 'p': {
            unsigned long long val = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            pos += out_char(buf, pos, size, '0');
            pos += out_char(buf, pos, size, 'x');
            pos += out_uint(buf, pos, size, val, 16, 0, 0, '0', 0);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            pos += out_str(buf, pos, size, s, width, left_align);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            pos += out_char(buf, pos, size, c);
            break;
        }
        case '%':
            pos += out_char(buf, pos, size, '%');
            break;
        default:
            // Unknown format — output as-is
            pos += out_char(buf, pos, size, '%');
            pos += out_char(buf, pos, size, *fmt);
            break;
        }

        fmt++;
    }

    // Null-terminate
    if (pos < size)
        buf[pos] = '\0';
    else if (size > 0)
        buf[size - 1] = '\0';

    return (int)pos;
}

int ax_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = ax_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}
