/// Beacon SMB listener

function ListenerUI(mode_create)
{
    let spacer1 = form.create_vspacer()

    let labelPipename = form.create_label("Pipename (C2):");
    let textlinePipename = form.create_textline();
    textlinePipename.setEnabled(mode_create)

    let labelEncryptKey = form.create_label("Encryption key:");
    let textlineEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textlineEncryptKey.setEnabled(mode_create)
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create)

    let spacer2 = form.create_vspacer()

    form.connect(buttonEncryptKey, "clicked", function() { textlineEncryptKey.setText( ax.random_string(32, "hex") ); });

    let layout = form.create_gridlayout();
    layout.addWidget(spacer1,            0, 0, 1, 3);
    layout.addWidget(labelPipename,      1, 0, 1, 1);
    layout.addWidget(textlinePipename,   1, 1, 1, 2);
    layout.addWidget(labelEncryptKey,    2, 0, 1, 1);
    layout.addWidget(textlineEncryptKey, 2, 1, 1, 1);
    layout.addWidget(buttonEncryptKey,   2, 2, 1, 1);
    layout.addWidget(spacer2,            3, 0, 1, 3);

    let container = form.create_container();
    container.put("pipename", textlinePipename);
    container.put("encrypt_key", textlineEncryptKey);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}