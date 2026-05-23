#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

/// Boolean type
#ifndef __cplusplus
#ifndef bool
typedef _Bool bool;
#define true  1
#define false 0
#endif
#endif

/// NULL
#ifndef NULL
#define NULL ((void*)0)
#endif

/// Command codes — must match Go-side defines in pl_utils.go
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
#define COMMAND_RUN             17
#define COMMAND_JOB_LIST        18
#define COMMAND_JOB_KILL        19

// Linux-specific commands (slots 20-30)
#define COMMAND_GETUID          20
#define COMMAND_ENV             21
#define COMMAND_NETSTAT         22
#define COMMAND_MOUNTS          23
#define COMMAND_EDR             24
#define COMMAND_CREDS           25
#define COMMAND_PERSIST         26
#define COMMAND_CONTAINER       27

// OPSEC commands (slots 28-30, 37-38)
#define COMMAND_MASQUERADE      28
#define COMMAND_TIMESTOMP       29
#define COMMAND_CLEANLOG        30
#define COMMAND_INJECT          37
#define COMMAND_MIGRATE         38

// Pivot commands (slots 39-41)
#define COMMAND_PIVOT_EXEC      39
#define COMMAND_LINK            40
#define COMMAND_UNLINK          41

// Tunnel/Terminal commands (control)
#define COMMAND_TUNNEL_START    31
#define COMMAND_TUNNEL_STOP     32
#define COMMAND_TUNNEL_PAUSE    33
#define COMMAND_TUNNEL_RESUME   34

#define COMMAND_TERMINAL_START  35
#define COMMAND_TERMINAL_STOP   36

// Tunnel MUX commands (data flows in main channel, not separate connection)
#define COMMAND_TUNNEL_WRITE    42   // teamserver → agent: write data to target
#define COMMAND_TUNNEL_STATUS   43   // agent → teamserver: connect result
#define COMMAND_TUNNEL_DATA     44   // agent → teamserver: data from target
#define COMMAND_TUNNEL_CLOSE    45   // agent → teamserver: channel closed

// BOF commands (slots 50-52)
#define COMMAND_EXEC_BOF        50   // execute ELF BOF in-memory
#define COMMAND_EXEC_BOF_OUT    51   // BOF output callback
#define COMMAND_EXEC_BOF_ASYNC  52   // execute ELF BOF in background thread

/// Pack types for agent ↔ teamserver protocol
#define INIT_PACK       1
#define EXFIL_PACK      2
#define JOB_PACK        3
#define JOB_TUNNEL      4
#define JOB_TERMINAL    5
#define BOF_PACK        6

/// Pivot type constants
#define PIVOT_TYPE_TCP         2
#define PIVOT_TYPE_DISCONNECT  10

/// Dynamic buffer type
typedef struct {
    uint8_t *data;
    int      len;
    int      cap;
} buffer_t;

// buffer_t functions are implemented in crt.c
void buf_init(buffer_t *b, int initial_cap);
void buf_append(buffer_t *b, const void *data, int len);
void buf_free(buffer_t *b);
void buf_reset(buffer_t *b);

#endif // TYPES_H
