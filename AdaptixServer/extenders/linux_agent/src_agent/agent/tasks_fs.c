/// tasks_fs.c -- Filesystem commands for Linux agent
/// All ops via direct syscalls — zero libc dependency.

#include "tasks_fs.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// ── Linux constants ──

#define O_RDONLY     0
#define O_WRONLY     1
#define O_RDWR       2
#define O_CREAT      0100
#define O_TRUNC      01000
#define O_APPEND     02000

// stat mode bits
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_IRUSR  0400
#define S_IWUSR  0200
#define S_IXUSR  0100
#define S_IRGRP  0040
#define S_IWGRP  0020
#define S_IXGRP  0010
#define S_IROTH  0004
#define S_IWOTH  0002
#define S_IXOTH  0001

// getdents64 structure
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

#define DT_DIR  4
#define DT_REG  8

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

/// Parse a single string field from msgpack params
static int parse_string_param(const uint8_t *data, uint32_t data_len,
                              const char *key_name, char *out, size_t out_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    size_t kname_len = ax_strlen(key_name);
    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;
        if (klen == kname_len && ax_memcmp(key, key_name, klen) == 0) {
            const char *val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) return -1;
            if (vlen >= out_size) vlen = (uint32_t)(out_size - 1);
            ax_memcpy(out, val, vlen);
            out[vlen] = '\0';
            return 0;
        }
        mp_skip(&r);
    }
    return -1;
}

/// Parse two string fields (src, dst)
static int parse_two_strings(const uint8_t *data, uint32_t data_len,
                             const char *key1, char *out1, size_t out1_size,
                             const char *key2, char *out2, size_t out2_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    size_t k1len = ax_strlen(key1);
    size_t k2len = ax_strlen(key2);
    int found = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;
        if (klen == k1len && ax_memcmp(key, key1, klen) == 0) {
            const char *val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) return -1;
            if (vlen >= out1_size) vlen = (uint32_t)(out1_size - 1);
            ax_memcpy(out1, val, vlen);
            out1[vlen] = '\0';
            found++;
        } else if (klen == k2len && ax_memcmp(key, key2, klen) == 0) {
            const char *val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) return -1;
            if (vlen >= out2_size) vlen = (uint32_t)(out2_size - 1);
            ax_memcpy(out2, val, vlen);
            out2[vlen] = '\0';
            found++;
        } else {
            mp_skip(&r);
        }
    }
    return (found >= 2) ? 0 : -1;
}

/// Expand ~ to /home/<user> via /proc/self/environ HOME=
static void normalize_path(const char *input, char *out, size_t out_size) {
    if (input[0] == '~' && (input[1] == '/' || input[1] == '\0')) {
        // Read HOME from /proc/self/environ
        char env_buf[4096];
        int fd = sys_open("/proc/self/environ", O_RDONLY, 0);
        if (fd >= 0) {
            long n = sys_read(fd, env_buf, sizeof(env_buf) - 1);
            sys_close(fd);
            if (n > 0) {
                env_buf[n] = '\0';
                // Environ is null-separated. Find HOME=
                char *p = env_buf;
                char *end = env_buf + n;
                while (p < end) {
                    if (p[0] == 'H' && p[1] == 'O' && p[2] == 'M' && p[3] == 'E' && p[4] == '=') {
                        ax_strncpy(out, p + 5, out_size - 1);
                        ax_strcat(out, input + 1);
                        out[out_size - 1] = '\0';
                        return;
                    }
                    while (p < end && *p) p++;
                    p++; // skip null separator
                }
            }
        }
        // Fallback: /tmp
        ax_strncpy(out, "/tmp", out_size - 1);
        ax_strcat(out, input + 1);
    } else {
        ax_strncpy(out, input, out_size - 1);
    }
    out[out_size - 1] = '\0';
}

/// Build permission mode string from stat mode
static void mode_string(unsigned int mode, char *buf) {
    buf[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

/// Format mtime from epoch seconds into "Mon DD HH:MM" style
/// Simplified — no timezone support, uses UTC
static void format_date(unsigned long epoch, char *buf, size_t buf_size) {
    // Simplified: just show epoch as a number if we can't format properly
    // For real formatting, would need a mini gmtime implementation
    // We'll format as "YYYY-MM-DD HH:MM" using a minimal epoch decoder

    // Days since epoch
    unsigned long secs = epoch;
    unsigned long mins = secs / 60; secs %= 60;
    unsigned long hours = mins / 60; mins %= 60;
    unsigned long days = hours / 24; hours %= 24;

    // Year calculation (simplified — ignores leap seconds)
    int year = 1970;
    for (;;) {
        int yday = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 366 : 365;
        if (days < (unsigned long)yday) break;
        days -= yday;
        year++;
    }

    // Month calculation
    int leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
    int month_days[] = {31, 28 + leap, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const char *month_names[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                        "Jul","Aug","Sep","Oct","Nov","Dec"};
    int month = 0;
    while (month < 12 && days >= (unsigned long)month_days[month]) {
        days -= month_days[month];
        month++;
    }
    int day = (int)days + 1;

    // Format: "Mon DD HH:MM"
    char tmp[32];
    int pos = 0;
    const char *mn = month_names[month < 12 ? month : 0];
    tmp[pos++] = mn[0]; tmp[pos++] = mn[1]; tmp[pos++] = mn[2];
    tmp[pos++] = ' ';
    if (day >= 10) tmp[pos++] = '0' + day / 10;
    else tmp[pos++] = ' ';
    tmp[pos++] = '0' + day % 10;
    tmp[pos++] = ' ';
    tmp[pos++] = '0' + (int)(hours / 10);
    tmp[pos++] = '0' + (int)(hours % 10);
    tmp[pos++] = ':';
    tmp[pos++] = '0' + (int)(mins / 10);
    tmp[pos++] = '0' + (int)(mins % 10);
    tmp[pos] = '\0';

    ax_strncpy(buf, tmp, buf_size - 1);
    buf[buf_size - 1] = '\0';
}

/// Convert UID to username by parsing /etc/passwd
static void uid_to_name(unsigned int uid, char *buf, size_t buf_size) {
    char passwd[8192];
    int fd = sys_open("/etc/passwd", O_RDONLY, 0);
    if (fd < 0) { goto fallback; }

    long n = sys_read(fd, passwd, sizeof(passwd) - 1);
    sys_close(fd);
    if (n <= 0) { goto fallback; }
    passwd[n] = '\0';

    // Format: name:x:uid:gid:...
    char uid_str[16];
    ax_itoa((int)uid, uid_str, 10);
    size_t uid_len = ax_strlen(uid_str);

    char *line = passwd;
    while (*line) {
        char *eol = ax_strchr(line, '\n');
        if (eol) *eol = '\0';

        // Find first ':'  (after name)
        char *p1 = ax_strchr(line, ':');
        if (!p1) goto next;
        // Find second ':' (after 'x')
        char *p2 = ax_strchr(p1 + 1, ':');
        if (!p2) goto next;
        // UID starts at p2+1
        char *uid_start = p2 + 1;
        char *p3 = ax_strchr(uid_start, ':');
        if (!p3) goto next;

        size_t field_len = (size_t)(p3 - uid_start);
        if (field_len == uid_len && ax_memcmp(uid_start, uid_str, uid_len) == 0) {
            // Found! Copy name (line to p1)
            size_t name_len = (size_t)(p1 - line);
            if (name_len >= buf_size) name_len = buf_size - 1;
            ax_memcpy(buf, line, name_len);
            buf[name_len] = '\0';
            return;
        }

    next:
        if (eol) line = eol + 1;
        else break;
    }

fallback:
    ax_itoa((int)uid, buf, 10);
}

/// Convert GID to group name by parsing /etc/group
static void gid_to_name(unsigned int gid, char *buf, size_t buf_size) {
    char group[8192];
    int fd = sys_open("/etc/group", O_RDONLY, 0);
    if (fd < 0) { goto fallback; }

    long n = sys_read(fd, group, sizeof(group) - 1);
    sys_close(fd);
    if (n <= 0) { goto fallback; }
    group[n] = '\0';

    char gid_str[16];
    ax_itoa((int)gid, gid_str, 10);
    size_t gid_len = ax_strlen(gid_str);

    char *line = group;
    while (*line) {
        char *eol = ax_strchr(line, '\n');
        if (eol) *eol = '\0';

        // Format: name:x:gid:members
        char *p1 = ax_strchr(line, ':');
        if (!p1) goto next;
        char *p2 = ax_strchr(p1 + 1, ':');
        if (!p2) goto next;
        char *gid_start = p2 + 1;
        char *p3 = ax_strchr(gid_start, ':');
        if (!p3) goto next;

        size_t field_len = (size_t)(p3 - gid_start);
        if (field_len == gid_len && ax_memcmp(gid_start, gid_str, gid_len) == 0) {
            size_t name_len = (size_t)(p1 - line);
            if (name_len >= buf_size) name_len = buf_size - 1;
            ax_memcpy(buf, line, name_len);
            buf[name_len] = '\0';
            return;
        }

    next:
        if (eol) line = eol + 1;
        else break;
    }

fallback:
    ax_itoa((int)gid, buf, 10);
}

// ──── Command handlers ────

int task_pwd(mp_writer_t *w)
{
    char cwd[4096];
    if (sys_getcwd(cwd, sizeof(cwd)) <= 0) {
        write_error(w, "getcwd failed");
        return 0;
    }
    mp_write_map(w, 1);
    mp_write_kv_str(w, "path", cwd);
    return 0;
}

int task_cd(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    if (sys_chdir(path) != 0) {
        write_error(w, "chdir failed");
        return 0;
    }

    char cwd[4096];
    if (sys_getcwd(cwd, sizeof(cwd)) <= 0) {
        write_error(w, "getcwd failed after chdir");
        return 0;
    }

    mp_write_map(w, 1);
    mp_write_kv_str(w, "path", cwd);
    return 0;
}

int task_cat(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    // Check file size
    struct linux_stat st;
    if (sys_stat(path, &st) != 0) {
        write_error(w, "file not found");
        return 0;
    }
    if (st.st_size > 1024 * 1024) {
        write_error(w, "file size exceeds 1 MB (use download)");
        return 0;
    }

    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_error(w, "cannot open file");
        return 0;
    }

    uint8_t *content = (uint8_t *)ax_malloc((size_t)st.st_size);
    if (!content) {
        sys_close(fd);
        write_error(w, "malloc failed");
        return 0;
    }

    size_t total = 0;
    while (total < (size_t)st.st_size) {
        long n = sys_read(fd, content + total, (size_t)st.st_size - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    sys_close(fd);

    mp_write_map(w, 2);
    mp_write_kv_str(w, "path", path);
    mp_write_kv_bin(w, "content", content, (uint32_t)total);

    ax_free(content);
    return 0;
}

int task_ls(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_path[4096] = {0};
    ax_strcpy(raw_path, ".");
    parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path));

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    struct linux_stat dir_st;
    if (sys_stat(path, &dir_st) != 0) {
        mp_write_map(w, 3);
        mp_write_kv_bool(w, "result", 0);
        mp_write_kv_str(w, "status", "path not found");
        mp_write_kv_str(w, "path", path);
        return 0;
    }

    mp_writer_t files_writer;
    mp_writer_init(&files_writer, 4096);

    if (S_ISDIR(dir_st.st_mode)) {
        int dirfd = sys_open(path, O_RDONLY, 0);
        if (dirfd < 0) {
            mp_write_map(w, 3);
            mp_write_kv_bool(w, "result", 0);
            mp_write_kv_str(w, "status", "cannot open directory");
            mp_write_kv_str(w, "path", path);
            return 0;
        }

        // First pass: count entries
        char dirbuf[4096];
        uint32_t count = 0;
        for (;;) {
            int nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;
            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *d = (struct linux_dirent64 *)(dirbuf + pos);
                count++;
                pos += d->d_reclen;
            }
        }

        // Reopen for second pass (no lseek on getdents)
        sys_close(dirfd);
        dirfd = sys_open(path, O_RDONLY, 0);
        if (dirfd < 0) {
            mp_write_map(w, 3);
            mp_write_kv_bool(w, "result", 0);
            mp_write_kv_str(w, "status", "cannot reopen directory");
            mp_write_kv_str(w, "path", path);
            return 0;
        }

        mp_write_array(&files_writer, count);

        for (;;) {
            int nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;
            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *d = (struct linux_dirent64 *)(dirbuf + pos);

                // Build full path for stat
                char fullpath[4096];
                ax_strncpy(fullpath, path, sizeof(fullpath) - 1);
                size_t plen = ax_strlen(fullpath);
                if (plen > 0 && fullpath[plen - 1] != '/') {
                    fullpath[plen] = '/';
                    fullpath[plen + 1] = '\0';
                }
                ax_strcat(fullpath, d->d_name);

                struct linux_stat fst;
                ax_memset(&fst, 0, sizeof(fst));
                sys_stat(fullpath, &fst);

                char mode[11];
                mode_string(fst.st_mode, mode);

                char user[64], group[64];
                uid_to_name(fst.st_uid, user, sizeof(user));
                gid_to_name(fst.st_gid, group, sizeof(group));

                char date[32];
                format_date(fst.st_mtime_sec, date, sizeof(date));

                mp_write_map(&files_writer, 8);
                mp_write_kv_str(&files_writer, "mode", mode);
                mp_write_kv_int(&files_writer, "nlink", (int64_t)fst.st_nlink);
                mp_write_kv_str(&files_writer, "user", user);
                mp_write_kv_str(&files_writer, "group", group);
                mp_write_kv_int(&files_writer, "size", (int64_t)fst.st_size);
                mp_write_kv_str(&files_writer, "date", date);
                mp_write_kv_str(&files_writer, "filename", d->d_name);
                mp_write_kv_bool(&files_writer, "is_dir", S_ISDIR(fst.st_mode) ? 1 : 0);

                pos += d->d_reclen;
            }
        }
        sys_close(dirfd);
    } else {
        // Single file
        mp_write_array(&files_writer, 1);

        char mode[11];
        mode_string(dir_st.st_mode, mode);

        char user[64], group[64];
        uid_to_name(dir_st.st_uid, user, sizeof(user));
        gid_to_name(dir_st.st_gid, group, sizeof(group));

        char date[32];
        format_date(dir_st.st_mtime_sec, date, sizeof(date));

        const char *basename = raw_path;
        for (const char *p = raw_path; *p; p++) {
            if (*p == '/') basename = p + 1;
        }

        mp_write_map(&files_writer, 8);
        mp_write_kv_str(&files_writer, "mode", mode);
        mp_write_kv_int(&files_writer, "nlink", (int64_t)dir_st.st_nlink);
        mp_write_kv_str(&files_writer, "user", user);
        mp_write_kv_str(&files_writer, "group", group);
        mp_write_kv_int(&files_writer, "size", (int64_t)dir_st.st_size);
        mp_write_kv_str(&files_writer, "date", date);
        mp_write_kv_str(&files_writer, "filename", basename);
        mp_write_kv_bool(&files_writer, "is_dir", 0);
    }

    // Ensure display path ends with / for directories
    char display_path[4096];
    ax_strncpy(display_path, path, sizeof(display_path) - 2);
    size_t dlen = ax_strlen(display_path);
    if (dlen > 0 && display_path[dlen - 1] != '/' && S_ISDIR(dir_st.st_mode)) {
        display_path[dlen] = '/';
        display_path[dlen + 1] = '\0';
    }

    mp_write_map(w, 4);
    mp_write_kv_bool(w, "result", 1);
    mp_write_kv_str(w, "status", "");
    mp_write_kv_str(w, "path", display_path);
    mp_write_kv_bin(w, "files", files_writer.buf.data, (uint32_t)files_writer.buf.len);

    mp_writer_free(&files_writer);
    return 0;
}

int task_cp(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_src[4096] = {0}, raw_dst[4096] = {0};
    if (parse_two_strings(data, data_len, "src", raw_src, sizeof(raw_src),
                          "dst", raw_dst, sizeof(raw_dst)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char src[4096], dst[4096];
    normalize_path(raw_src, src, sizeof(src));
    normalize_path(raw_dst, dst, sizeof(dst));

    // Copy file via syscalls
    int sfd = sys_open(src, O_RDONLY, 0);
    if (sfd < 0) {
        write_error(w, "cannot open source");
        return 0;
    }

    struct linux_stat st;
    sys_fstat(sfd, &st);

    int dfd = sys_open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dfd < 0) {
        sys_close(sfd);
        write_error(w, "cannot create destination");
        return 0;
    }

    char buf[8192];
    for (;;) {
        long n = sys_read(sfd, buf, sizeof(buf));
        if (n <= 0) break;
        long written = 0;
        while (written < n) {
            long w2 = sys_write(dfd, buf + written, (size_t)(n - written));
            if (w2 <= 0) break;
            written += w2;
        }
    }
    sys_close(sfd);
    sys_close(dfd);

    mp_write_nil(w);
    return 0;
}

int task_mv(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_src[4096] = {0}, raw_dst[4096] = {0};
    if (parse_two_strings(data, data_len, "src", raw_src, sizeof(raw_src),
                          "dst", raw_dst, sizeof(raw_dst)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char src[4096], dst[4096];
    normalize_path(raw_src, src, sizeof(src));
    normalize_path(raw_dst, dst, sizeof(dst));

    if (sys_rename(src, dst) != 0) {
        write_error(w, "rename failed");
        return 0;
    }

    mp_write_nil(w);
    return 0;
}

int task_mkdir(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    // Create with parents (simplified mkdirall)
    char tmp[4096];
    ax_strncpy(tmp, path, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            sys_mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (sys_mkdir(tmp, 0755) != 0) {
        // Check if it already exists as dir
        struct linux_stat st;
        if (sys_stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            write_error(w, "mkdir failed");
            return 0;
        }
    }

    mp_write_nil(w);
    return 0;
}

int task_rm(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    struct linux_stat st;
    if (sys_stat(path, &st) != 0) {
        write_error(w, "path not found");
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        // Recursive rm via fork+execve("/bin/rm", ["-rf", path])
        int pid = sys_fork();
        if (pid == 0) {
            // Child — execve /bin/rm -rf <path>
            char *argv[] = { "/bin/rm", "-rf", path, (char *)0 };
            char *envp[] = { (char *)0 };
            sys_execve("/bin/rm", argv, envp);
            sys_exit_group(1);
        } else if (pid > 0) {
            int status = 0;
            sys_wait4(pid, &status, 0, (void *)0);
            if ((status & 0xff00) >> 8 != 0) {
                write_error(w, "rm -rf failed");
                return 0;
            }
        } else {
            write_error(w, "fork failed");
            return 0;
        }
    } else {
        if (sys_unlink(path) != 0) {
            write_error(w, "unlink failed");
            return 0;
        }
    }

    mp_write_nil(w);
    return 0;
}

int task_zip(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    // zip not available on Linux via simple binary — stub for now
    (void)data;
    (void)data_len;
    write_error(w, "zip not implemented");
    return 0;
}
