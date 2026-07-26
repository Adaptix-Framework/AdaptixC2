    var id = event.agentId || (event.agent && event.agent.a_id);
    var computer = (event.agent && event.agent.a_computer) || "";
    ax.log("{{NAME}}: agent=" + id + " computer=" + computer);
    // ax.execute_command(id, "pwd");
