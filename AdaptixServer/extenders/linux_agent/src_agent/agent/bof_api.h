/// bof_api.h — Linux Beacon API header for BOF authors
/// This is the public API that BOF .o files link against.
/// Compile BOFs with: gcc -c -o bof.o bof.c -include bof_api.h -Os -fPIC

#ifndef LINUX_BEACON_API_H
#define LINUX_BEACON_API_H

/// ── Data parser types ──

typedef struct {
    char *original;
    char *buffer;
    int   length;
    int   size;
} datap;

typedef struct {
    char *original;
    char *buffer;
    int   length;
    int   size;
} formatp;

/// ── Output types (CS-compatible) ──

#define CALLBACK_OUTPUT      0x0
#define CALLBACK_OUTPUT_OEM  0x1e
#define CALLBACK_OUTPUT_UTF8 0x20
#define CALLBACK_ERROR       0x0d

/// ── Data Parser API ──

void   BeaconDataParse(datap *parser, char *buffer, int size);
int    BeaconDataInt(datap *parser);
short  BeaconDataShort(datap *parser);
int    BeaconDataLength(datap *parser);
char  *BeaconDataExtract(datap *parser, int *size);

/// ── Output API ──

void   BeaconOutput(int type, const char *data, int len);
void   BeaconPrintf(int type, const char *fmt, ...);

/// ── Format API ──

void   BeaconFormatAlloc(formatp *format, int maxsz);
void   BeaconFormatReset(formatp *format);
void   BeaconFormatAppend(formatp *format, const char *text, int len);
void   BeaconFormatPrintf(formatp *format, const char *fmt, ...);
char  *BeaconFormatToString(formatp *format, int *size);
void   BeaconFormatFree(formatp *format);
void   BeaconFormatInt(formatp *format, int value);

/// ── Utility ──

int    BeaconIsAdmin(void);

/// ── Async BOF APIs ──

void   BeaconWakeup(void);
int    BeaconGetStopJobEvent(void);  // returns readable fd (-1 if not async)

/// ── Adaptix extensions ──

void   AxDownloadMemory(char *filename, char *data, int len);

/// ── Linux system primitives (Adaptix extensions) ──
/// These expose the agent's nostdlib syscall wrappers to BOFs

// File I/O
int    AxOpenFile(const char *path, int flags, int mode);
int    AxCloseFile(int fd);
int    AxReadFile(int fd, void *buf, int count);

// File read helper — reads entire file into malloc'd buffer, returns bytes read (-1 on error)
int    AxReadFileToBuffer(const char *path, char **out_buf, int max_size);

// File stat
int    AxFileStat(const char *path, unsigned int *out_mode, long *out_size, unsigned int *out_uid, unsigned int *out_gid);

// Directory listing
int    AxOpenDir(const char *path);
int    AxReadDir(int fd, void *buf, int bufsize);

// Memory
void  *AxMalloc(int size);
void   AxFree(void *ptr);
void  *AxMemset(void *s, int c, int n);
void  *AxMemcpy(void *dst, const void *src, int n);

// String operations
int    AxStrlen(const char *s);
int    AxStrcmp(const char *a, const char *b);
int    AxStrncmp(const char *a, const char *b, int n);
char  *AxStrcpy(char *dst, const char *src);
char  *AxStrncpy(char *dst, const char *src, int n);
char  *AxStrcat(char *dst, const char *src);
char  *AxStrstr(const char *haystack, const char *needle);
char  *AxStrchr(const char *s, int c);

// Formatted output
int    AxSnprintf(char *buf, int size, const char *fmt, ...);

// Process info
int    AxGetPid(void);
int    AxGetUid(void);
int    AxGetEuid(void);

// getcwd
int    AxGetCwd(char *buf, int size);

// getenv equivalent — reads /proc/self/environ
int    AxGetEnv(const char *name, char *out_buf, int out_size);

#endif // LINUX_BEACON_API_H
