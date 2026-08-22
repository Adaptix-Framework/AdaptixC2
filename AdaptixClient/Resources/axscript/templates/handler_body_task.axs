    // task.* — agentId, taskId, client, cmdline (create), task{…}
    var agentId = event.agentId || (event.task && event.task.t_agent_id);
    var taskId = event.taskId || (event.task && event.task.t_task_id);
    var client = event.client || (event.task && event.task.t_client) || "";
    var cmdline = event.cmdline || "";
    ax.log("{{NAME}}: type=" + event.type
        + " agent=" + agentId
        + " task=" + taskId
        + (client ? " client=" + client : "")
        + (cmdline ? " cmd=" + cmdline : ""));
