function GeneratePanel() {
    let events = [
        "agent.new", "agent.activate", "agent.generate", "agent.checkin", "agent.update", "agent.terminate", "agent.remove",
        "listener.start", "listener.stop",
        "task.create", "task.start", "task.update_job", "task.complete",
        "credentials.add", "credentials.edit", "credentials.remove",
        "download.start", "download.finish", "download.remove",
        "upload.start", "upload.finish", "upload.remove",
        "screenshot.add", "screenshot.remove",
        "tunnel.start", "tunnel.stop",
        "target.add", "target.edit", "target.remove",
        "pivot.create", "pivot.remove",
        "client.connect", "client.disconnect"
    ];

    let eventCombo = form.create_combo();
    eventCombo.addItems(events);
    if (eventCombo.setEditable)
        eventCombo.setEditable(true);

    let applyTemplateBtn = form.create_button("Template");
    applyTemplateBtn.setEnabled(true);

    let eventRow = form.create_hlayout();
    eventRow.setContentsMargins(0, 0, 0, 0);
    eventRow.setSpacing(6);
    eventRow.addWidget(eventCombo);
    eventRow.addWidget(applyTemplateBtn);

    let eventRowPanel = form.create_panel();
    eventRowPanel.setLayout(eventRow);

    let nameEdit = form.create_textline("");
    nameEdit.setPlaceholder("auto_handler");

    let groupEdit = form.create_textline("");
    groupEdit.setPlaceholder("same as name");

    let descEdit = form.create_textline("");
    descEdit.setPlaceholder("What this handler does");

    let idEdit = form.create_textline("");
    idEdit.setVisible(false);

    let metaGrid = form.create_gridlayout();
    let r = 0;
    metaGrid.addWidget(form.create_label("Event"), r, 0, 1, 1);
    metaGrid.addWidget(eventRowPanel, r, 1, 1, 1); r++;
    metaGrid.addWidget(form.create_label("Name"), r, 0, 1, 1);
    metaGrid.addWidget(nameEdit, r, 1, 1, 1); r++;
    metaGrid.addWidget(form.create_label("Group"), r, 0, 1, 1);
    metaGrid.addWidget(groupEdit, r, 1, 1, 1); r++;
    metaGrid.addWidget(form.create_label("Description"), r, 0, 1, 1);
    metaGrid.addWidget(descEdit, r, 1, 1, 1);

    form.connect(applyTemplateBtn, "clicked", function () {
        let et = String(eventCombo.currentText() || "").trim() || "agent.new";
        if (typeof editor === "undefined" || !editor || !editor.apply_handler_template) {
            return;
        }
        if (editor.apply_handler_template(et)) {
            if (!descEdit.text() || String(descEdit.text()).indexOf("Handler for ") === 0)
                descEdit.setText("Handler for " + et);
        }
    });

    let metaInner = form.create_panel();
    metaInner.setLayout(metaGrid);
    let metaBox = form.create_groupbox("Handler");
    metaBox.setPanel(metaInner);

    function makeRow(labelText, placeholder) {
        return {
            lab: form.create_label(labelText),
            field: (function () {
                let e = form.create_textline("");
                e.setPlaceholder(placeholder || "");
                return e;
            })()
        };
    }
    function makeOs() {
        let lab = form.create_label("OS");
        let field = form.create_combo();
        field.addItems(["(any)", "windows", "linux", "macos"]);
        return { lab: lab, field: field };
    }
    function makeAlive() {
        let lab = form.create_label("Alive");
        let field = form.create_combo();
        field.addItems(["(any)", "true", "false"]);
        return { lab: lab, field: field };
    }

    let fAgentId = makeRow("Agent ID", "agent id");
    let fAgentName = makeRow("Name", "agent type / name");
    let fUser = makeRow("User", "username");
    let fOs = makeOs();
    let fComputer = makeRow("Computer", "hostname");
    let fTags = makeRow("Tags", "tag1, tag2");
    let fListener = makeRow("Listener", "listener name");
    let fListenerType = makeRow("Type", "listener type");
    let fListenerTag = makeRow("Tag", "listener tag");
    let fTaskId = makeRow("Task ID", "task id");
    let fClient = makeRow("Client", "operator");
    let fFileId = makeRow("File ID", "file id");
    let fFilename = makeRow("Filename", "name or path fragment");
    let fPort = makeRow("Port", "local/remote port");
    let fTunnelType = makeRow("Type", "tunnel type id");
    let fRealm = makeRow("Realm", "domain / realm");
    let fCredType = makeRow("Type", "password, hash…");
    let fHost = makeRow("Host", "host");
    let fDomain = makeRow("Domain", "domain");
    let fAddress = makeRow("Address", "ip / host");
    let fAlive = makeAlive();
    let filterHint = form.create_label("Empty = match all");

    let allRows = [
        fAgentId, fAgentName, fUser, fOs, fComputer, fTags,
        fListener, fListenerType, fListenerTag,
        fTaskId, fClient,
        fFileId, fFilename,
        fPort, fTunnelType,
        fRealm, fCredType, fHost,
        fDomain, fAddress, fAlive
    ];

    let filterGrid = form.create_gridlayout();
    r = 0;
    for (let i = 0; i < allRows.length; i++) {
        filterGrid.addWidget(allRows[i].lab, r, 0, 1, 1);
        filterGrid.addWidget(allRows[i].field, r, 1, 1, 1);
        r++;
    }
    filterGrid.addWidget(filterHint, r, 0, 1, 2);

    let filterInner = form.create_panel();
    filterInner.setLayout(filterGrid);
    let filterBox = form.create_groupbox("Filters");
    filterBox.setPanel(filterInner);

    function setRow(row, on) {
        if (row && row.lab) row.lab.setVisible(!!on);
        if (row && row.field) row.field.setVisible(!!on);
    }
    function hideAll() {
        for (let i = 0; i < allRows.length; i++) setRow(allRows[i], false);
    }
    function show(rows) {
        hideAll();
        for (let i = 0; i < rows.length; i++) setRow(rows[i], true);
        filterBox.setVisible(rows.length > 0);
        filterHint.setVisible(rows.length > 0);
    }

    function applyFilterVisibility(eventType) {
        eventType = String(eventType || "");
        let fam = eventType.split(".")[0] || "";

        if (fam === "agent") {
            // id, name, os, user, computer, tags
            show([fAgentId, fAgentName, fOs, fUser, fComputer, fTags]);
        } else if (fam === "listener") {
            // name, type, tag
            show([fListener, fListenerType, fListenerTag]);
        } else if (fam === "task") {
            // id, agent, client, computer
            show([fTaskId, fAgentId, fClient, fComputer]);
        } else if (fam === "download" || fam === "upload") {
            // id, agent, computer, filename
            show([fFileId, fAgentId, fComputer, fFilename]);
        } else if (fam === "screenshot") {
            // agent, computer
            show([fAgentId, fComputer]);
        } else if (fam === "tunnel") {
            // agent, port, type, computer
            show([fAgentId, fPort, fTunnelType, fComputer]);
        } else if (fam === "credentials") {
            // realm · type · host · username · tag · agent_id
            show([fRealm, fCredType, fHost, fUser, fTags, fAgentId]);
        } else if (fam === "target") {
            // computer · domain · address · os · tag · alive
            show([fComputer, fDomain, fAddress, fOs, fTags, fAlive]);
        } else if (fam === "client") {
            show([fClient]);
        } else if (fam === "pivot") {
            show([fAgentId]);
        } else {
            show([fAgentId, fAgentName, fUser, fComputer, fTags, fClient, fTaskId]);
        }
    }

    form.connect(eventCombo, "currentTextChanged", function (text) {
        applyFilterVisibility(text);
    });
    applyFilterVisibility(eventCombo.currentText());

    let row = form.create_hlayout();
    row.setContentsMargins(8, 6, 8, 6);
    row.setSpacing(12);
    row.addWidget(metaBox);
    row.addWidget(filterBox);

    let panel = form.create_panel();
    panel.setLayout(row);

    let container = form.create_container();
    container.put("event", eventCombo);
    container.put("name", nameEdit);
    container.put("description", descEdit);
    container.put("group", groupEdit);
    container.put("id", idEdit);

    container.put("agent_id", fAgentId.field);
    container.put("agent_name", fAgentName.field);
    container.put("user", fUser.field);
    container.put("os", fOs.field);
    container.put("computer", fComputer.field);
    container.put("tags", fTags.field);
    container.put("listener", fListener.field);
    container.put("listener_type", fListenerType.field);
    container.put("listener_tag", fListenerTag.field);
    container.put("task_id", fTaskId.field);
    container.put("client", fClient.field);
    container.put("file_id", fFileId.field);
    container.put("filename", fFilename.field);
    container.put("port", fPort.field);
    container.put("tunnel_type", fTunnelType.field);
    container.put("realm", fRealm.field);
    container.put("cred_type", fCredType.field);
    container.put("host", fHost.field);
    container.put("domain", fDomain.field);
    container.put("address", fAddress.field);
    container.put("alive", fAlive.field);

    return {
        ui_panel: panel,
        ui_container: container
    };
}
