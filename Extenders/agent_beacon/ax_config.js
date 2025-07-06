/// Beacom agent

let exit_thread_action  = menu.create_action("Terminate thread",  function(value) { value.forEach(v => ax.execute_command(v, "terminate thread")) });
let exit_process_action = menu.create_action("Terminate process", function(value) { value.forEach(v => ax.execute_command(v, "terminate process")) });
let exit_menu = menu.create_menu("Exit");
exit_menu.addItem(exit_thread_action)
exit_menu.addItem(exit_process_action)
menu.add_session_agent(exit_menu, ["beacon"])

let file_browser_action     = menu.create_action("File Browser",    function(value) { value.forEach(v => ax.open_browser_files(v)) });
let process_browser_action  = menu.create_action("Process Browser", function(value) { value.forEach(v => ax.open_browser_process(v)) });
menu.add_session_browser(file_browser_action, ["beacon"])
menu.add_session_browser(process_browser_action, ["beacon"])

let tunnel_access_action = menu.create_action("Create Tunnel", function(value) { ax.open_access_tunnel(value[0], true, true, true, true) });
menu.add_session_access(tunnel_access_action, ["beacon"])

function RegisterCommands(listenerType)
{
    var cmd_cat = ax.create_command("cat", "Read first 2048 bytes of the specified file", "cat C:\\file.exe", "Task: read file");
    cmd_cat.addArgString("path", true);

    var cmd_cd = ax.create_command("cd", "Change current working directory", "cd C:\\Windows", "Task: change working directory");
    cmd_cd.addArgString("path", true);

    var cmd_cp = ax.create_command("cp", "Copy file", "cp src.txt dst.txt", "Task: copy file");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    var cmd_disks = ax.create_command("disks", "Lists mounted drives on current system", "disks", "Task: show mounted disks");

    var cmd_download = ax.create_command("download", "Download a file", "download C:\\Temp\\file.txt", "Task: download file");
    cmd_download.addArgString("file", true);

    var _cmd_execute_bof = ax.create_command("list", "Execute Beacon Object File", "execute bof /home/user/whoami.o", "Task: execute BOF");
    _cmd_execute_bof.addArgFile("bof", true, "Path to object file");
    _cmd_execute_bof.addArgString("param_data", false);
    var cmd_execute = ax.create_command("execute", "Execute [bof] in the current process's memory");
    cmd_execute.addSubCommands([_cmd_execute_bof])

    var _cmd_exfil_cancel = ax.create_command("cancel", "Cancels a download", "exfil cancel 1a2b3c4d");
    _cmd_exfil_cancel.addArgFile("file_id", true);
    var _cmd_exfil_start = ax.create_command("start", "Resumes a download that's has been stoped", "exfil start 1a2b3c4d");
    _cmd_exfil_start.addArgFile("file_id", true);
    var _cmd_exfil_stop = ax.create_command("stop", "Stops a download that's in-progress", "exfil stop 1a2b3c4d");
    _cmd_exfil_stop.addArgFile("file_id", true);
    var cmd_exfil = ax.create_command("exfil", "Manage current downloads");
    cmd_exfil.addSubCommands([_cmd_exfil_cancel, _cmd_exfil_start, _cmd_exfil_stop])

    var cmd_getuid = ax.create_command("getuid", "Prints the User ID associated with the current token", "getuid", "Task: get username of current token");

    var _cmd_job_list = ax.create_command("list", "List of jobs", "job list", "Task: show jobs");
    var _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "job kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    var cmd_job = ax.create_command("job", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    var _cmd_link_smb = ax.create_command("smb", "Connect to an SMB agent and re-establish control of it", "link smb 192.168.1.2 pipe_a1b2", "Task: Connect to an SMB agent");
    _cmd_link_smb.addArgString("target", true);
    _cmd_link_smb.addArgString("pipename", true);
    var _cmd_link_tcp = ax.create_command("tcp", "Connect to an TCP agent and re-establish control of it", "link tcp 192.168.1.2 8888", "Task: Connect to an TCP agent");
    _cmd_link_tcp.addArgString("target", true);
    _cmd_link_tcp.addArgInt("port", true);
    var cmd_link = ax.create_command("link", "Connect to an pivot agents");
    cmd_link.addSubCommands([_cmd_link_smb, _cmd_link_tcp]);

    var cmd_ls = ax.create_command("ls", "Lists files in a folder", "ls C:\\Windows", "Task: list of files in a folder");
    cmd_ls.addArgString("directory", "", ".");

    var _cmd_lportfwd_start = ax.create_command("start", "Start local port forwarding from server via agent", "lportfwd start 127.0.0.1 8080 192.168.1.1 8080");
    _cmd_lportfwd_start.addArgString("lhost", "Listening interface address on server", "0.0.0.0");
    _cmd_lportfwd_start.addArgInt("lport", true, "Listen port on server");
    _cmd_lportfwd_start.addArgString("fwdhost", true, "Remote forwarding address");
    _cmd_lportfwd_start.addArgInt("fwdport", true, "Remote forwarding port");
    var _cmd_lportfwd_stop = ax.create_command("stop", "Stop local port forwarding", "lportfwd stop 8080");
    _cmd_lportfwd_stop.addArgInt("lport", true);
    var cmd_lportfwd = ax.create_command("lportfwd", "Managing local port forwarding");
    cmd_lportfwd.addSubCommands([_cmd_lportfwd_start, _cmd_lportfwd_stop]);

    var cmd_mv = ax.create_command("mv", "Move file", "mv src.txt dst.txt", "Task: move file");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    var cmd_mkdir = ax.create_command("mkdir", "Make a directory", "mkdir C:\\Temp", "Task: make directory");
    cmd_mkdir.addArgString("path", true);

    var _cmd_profile_chunksize = ax.create_command("download.chunksize", "Change the exfiltrate data size for download request (default 128000)", "profile download.chunksize 512000", "Task: set download chunk size");
    _cmd_profile_chunksize.addArgInt("size", true);
    var _cmd_profile_killdate = ax.create_command("killdate", "Set the date and time for the beacon to stop working", "profile killdate 28.02.2030 12:34:00", "Task: set beacon's killdate");
    _cmd_profile_killdate.addArgString("datetime", true, "Datetime 'DD.MM.YYYY hh:mm:ss' in GMT format. Set 0 to disable the option");
    var _cmd_profile_workingtime = ax.create_command("workingtime", "Set the start and end time of the beacon activity", "profile workingtime 8:00-17:30", "Task: set beacon's workingtime");
    _cmd_profile_workingtime.addArgString("time", true, "Time interval in the format 'HH:mm(start)-HH:mm(end)'. Set 0 to disable the option");
    var cmd_profile = ax.create_command("profile", "Configure the payloads profile for current session");
    cmd_profile.addSubCommands([_cmd_profile_chunksize, _cmd_profile_killdate, _cmd_profile_workingtime]);

    var _cmd_ps_list = ax.create_command("list", "Show process list", "ps list", "Task: show process list");
    var _cmd_ps_kill = ax.create_command("kill", "Kill a process with a given PID", "ps kill 7865", "Task: kill process");
    _cmd_ps_kill.addArgInt("pid", true);
    var _cmd_ps_run = ax.create_command("run", "Run a program", "run -s cmd.exe \"whoami /all\"", "Task: create new process");
    _cmd_ps_run.addArgBool("-s", "Suspend process");
    _cmd_ps_run.addArgBool("-o", "Output to console");
    _cmd_ps_run.addArgString("program", true);
    _cmd_ps_run.addArgString("args", false);
    var cmd_ps = ax.create_command("ps", "Process manager");
    cmd_ps.addSubCommands([_cmd_ps_list, _cmd_ps_kill, _cmd_ps_run]);

    var cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    var cmd_rev2self = ax.create_command("rev2self", "Revert to your original access token", "rev2self", "Task: revert token");

    var cmd_rm = ax.create_command("rm", "Remove a file or folder", "rm C:\\Temp\\file.txt", "Task: remove file or directory");
    cmd_rm.addArgString("path", true);

    var _cmd_rportfwd_start = ax.create_command("start", "Start remote port forwarding from agent via server", "rportfwd start 8080 10.10.10.14 8080");
    _cmd_rportfwd_start.addArgInt("lport", true, "Listen port on agent");
    _cmd_rportfwd_start.addArgString("fwdhost", true, "Remote forwarding address");
    _cmd_rportfwd_start.addArgInt("fwdport", true, "Remote forwarding port");
    var _cmd_rportfwd_stop = ax.create_command("stop", "Stop remote port forwarding", "rportfwd stop 8080");
    _cmd_rportfwd_stop.addArgInt("lport", true);
    var cmd_rportfwd = ax.create_command("rportfwd", "Managing remote port forwarding");
    cmd_rportfwd.addSubCommands([_cmd_rportfwd_start, _cmd_rportfwd_stop]);

    var cmd_sleep = ax.create_command("sleep", "Sets sleep time", "sleep 30m5s 10");
    cmd_sleep.addArgString("sleep", true, "Time in '%h%m%s' format or number of seconds");
    cmd_sleep.addArgInt("jitter", true, "Max random amount of time in % added to sleep");

    var _cmd_socks_start = ax.create_command("start", "Start a SOCKS(4a/5) proxy server and listen on a specified port", "socks start 1080 -auth user pass");
    _cmd_socks_start.addArgFlagString("-h", "address", "Listening interface address", "0.0.0.0");
    _cmd_socks_start.addArgInt("port", true, "Listen port");
    _cmd_socks_start.addArgBool("-socks4", "Use SOCKS4 proxy (Default SOCKS5)");
    _cmd_socks_start.addArgBool("-auth", "Enable User/Password authentication for SOCKS5");
    _cmd_socks_start.addArgString("username", false, "Username for SOCKS5 proxy");
    _cmd_socks_start.addArgString("password", false, "Password for SOCKS5 proxy");
    var _cmd_socks_stop = ax.create_command("stop", "Stop a SOCKS proxy server", "socks stop 1080");
    _cmd_socks_stop.addArgInt("port", true);
    var cmd_socks = ax.create_command("socks", "Managing socks tunnels");
    cmd_socks.addSubCommands([_cmd_socks_start, _cmd_socks_stop]);

    var _cmd_terminate_thread = ax.create_command("thread", "Terminate the main beacon thread (without terminating the process)", "terminate thread", "Task: terminate agent thread");
    var _cmd_terminate_process = ax.create_command("process", "Terminate the beacon process", "terminate process", "Task: terminate agent process");
    var cmd_terminate = ax.create_command("terminate", "Terminate the session");
    cmd_terminate.addSubCommands([_cmd_terminate_thread, _cmd_terminate_process]);

    var cmd_unlink = ax.create_command("unlink", "Disconnect from an pivot agent", "unlink 1a2b3c4d", "Task: disconnect from an pivot agent");
    cmd_unlink.addArgString("id", true);

    var cmd_upload = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt C:\\Temp\\file.txt", "Task: upload file");
    cmd_upload.addArgFile("local_file", true);
    cmd_upload.addArgString("remote_path", false);

    var cmd_shell = ax.create_command("shell", "Execute command via cmd.exe", "shell whoami /all");
    cmd_shell.addArgString("command", true);
    cmd_shell.setPreHook(function (id, cmdline, ...args){
        if(args.length == 0) {
            ax.console_print()
        }
        let params = cmdline.substring(6);
        let new_cmd = "ps run -o C:\\Windows\\System32\\cmd.exe /c " + params;
        ax.execute_alias(id, cmdline, new_cmd);
    });

    if(listenerType == "BeaconHTTP") {
        var commands_external = ax.create_commands_group("beacon", [cmd_cat, cmd_cd, cmd_cp, cmd_disks, cmd_download, cmd_execute, cmd_exfil, cmd_getuid,
            cmd_job, cmd_link, cmd_ls, cmd_lportfwd, cmd_mv, cmd_mkdir, cmd_profile, cmd_ps, cmd_pwd, cmd_rev2self, cmd_rm, cmd_rportfwd, cmd_sleep,
            cmd_socks, cmd_terminate, cmd_unlink, cmd_upload, cmd_shell] );

        return { commands_windows: commands_external }
    }
    else if (listenerType == "BeaconSMB" || listenerType == "BeaconTCP") {
        var commands_internal = ax.create_commands_group("beacon", [cmd_cat, cmd_cd, cmd_cp, cmd_disks, cmd_download, cmd_execute, cmd_exfil, cmd_getuid,
            cmd_job, cmd_link, cmd_ls, cmd_lportfwd, cmd_mv, cmd_mkdir, cmd_profile, cmd_ps, cmd_pwd, cmd_rev2self, cmd_rm, cmd_rportfwd,
            cmd_socks, cmd_terminate, cmd_unlink, cmd_upload] );

        return { commands_windows: commands_internal }
    }

    return ax.create_commands_group("none",[]);
}

function GenerateUI(listenerType)
{
    var labelArch = form.create_label("Arch:");
    var comboArch = form.create_combo()
    comboArch.addItems(["x64", "x86"]);

    var labelFormat = form.create_label("Format:");
    var comboFormat = form.create_combo()
    comboFormat.addItems(["Exe", "Service Exe", "DLL", "Shellcode"]);

    var labelSleep = form.create_label("Sleep (Jitter %):");
    var textSleep = form.create_textline("4s");
    textSleep.setPlaceholder("1h 2m 5s")
    var spinJitter = form.create_spin();
    spinJitter.setRange(0, 100);
    spinJitter.setValue(0);

    if(listenerType != "BeaconHTTP") {
        labelSleep.setVisible(false);
        textSleep.setVisible(false);
        spinJitter.setVisible(false);
    }

    var checkKilldate = form.create_check("Set 'killdate'");
    var dateKill = form.create_dateline("dd.MM.yyyy");
    var timeKill = form.create_timeline("HH:mm:ss");

    var checkWorkingTime = form.create_check("Set 'workingtime'");
    var timeStart = form.create_timeline("HH:mm");
    var timeFinish = form.create_timeline("HH:mm");

    var labelSvcName = form.create_label("Service Name:");
    labelSvcName.setVisible(false)
    var textSvcName = form.create_textline("AgentService");
    textSvcName.setVisible(false);

    var layout = form.create_gridlayout();
    layout.addWidget(labelArch, 0, 0, 1, 1);
    layout.addWidget(comboArch, 0, 1, 1, 2);
    layout.addWidget(labelFormat, 1, 0, 1, 1);
    layout.addWidget(comboFormat, 1, 1, 1, 2);
    layout.addWidget(labelSleep, 2, 0, 1, 1);
    layout.addWidget(textSleep, 2, 1, 1, 1);
    layout.addWidget(spinJitter, 2, 2, 1, 1);
    layout.addWidget(checkKilldate, 3, 0, 1, 1);
    layout.addWidget(dateKill, 3, 1, 1, 1);
    layout.addWidget(timeKill, 3, 2, 1, 1);
    layout.addWidget(checkWorkingTime, 4, 0, 1, 1);
    layout.addWidget(timeStart, 4, 1, 1, 1);
    layout.addWidget(timeFinish, 4, 2, 1, 1);
    layout.addWidget(labelSvcName, 5, 0, 1, 1);
    layout.addWidget(textSvcName, 5, 1, 1, 2);

    form.connect(comboFormat, "currentTextChanged", function(text) {
        if(text == "Service Exe") {
            labelSvcName.setVisible(true)
            textSvcName.setVisible(true);
        }
        else {
            labelSvcName.setVisible(true)
            textSvcName.setVisible(true);
        }
    });

    var container = form.create_container()
    container.put("arch", comboArch)
    container.put("format", comboFormat)
    container.put("sleep", textSleep)
    container.put("jitter", spinJitter)
    container.put("is_killdate", checkKilldate)
    container.put("kill_date", dateKill)
    container.put("kill_time", timeKill)
    container.put("is_workingtime", checkWorkingTime)
    container.put("start_time", timeStart)
    container.put("end_time", timeFinish)
    container.put("svcname", textSvcName)

    var panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container
    }
}
