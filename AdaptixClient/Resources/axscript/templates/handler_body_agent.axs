    // agent.* — fields from EventToMap (agentId, agent{…}, restore, …)
    var id = event.agentId || (event.agent && event.agent.a_id);
    var name = event.agentName || (event.agent && event.agent.a_name) || "";
    var computer = event.computer || (event.agent && event.agent.a_computer) || "";
    var user = event.user || (event.agent && event.agent.a_username) || "";
    ax.log("{{NAME}}: type=" + event.type
        + " agent=" + id
        + " name=" + name
        + " computer=" + computer
        + (user ? " user=" + user : "")
        + (event.restore ? " restore=true" : ""));
    // if (event.restore) return; // skip DB restore
    // ax.execute_command(id, "pwd");
