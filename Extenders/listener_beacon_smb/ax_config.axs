/// Beacon SMB listener

function ListenerUI(mode_create)
{
    let labelPipename = form.create_label("Pipename (C2):");
    let textlinePipename = form.create_textline();
    textlinePipename.setEnabled(mode_create)

    let layout = form.create_gridlayout();
    layout.addWidget(labelPipename, 0, 0, 1, 1);
    layout.addWidget(textlinePipename, 0, 1, 1, 1);

    let container = form.create_container();
    container.put("pipename", textlinePipename);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}