    var agentId = event.agentId || (event.task && event.task.t_agent_id);
    var taskId = event.task && event.task.t_task_id;
    ax.log("{{NAME}}: agent=" + agentId + " task=" + taskId);
