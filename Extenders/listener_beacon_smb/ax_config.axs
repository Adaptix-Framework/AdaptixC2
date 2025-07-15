/// Beacon SMB listener

function ListenerUI(mode_create)
{
    var labelPipename = form.create_label("Pipename (C2):");
    var textlinePipename = form.create_textline();
    textlinePipename.setEnabled(mode_create)

    var layout = form.create_gridlayout();
    layout.addWidget(labelPipename, 0, 0, 1, 1);
    layout.addWidget(textlinePipename, 0, 1, 1, 1);

    var container = form.create_container();
    container.put("pipename", textlinePipename);

    var panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}