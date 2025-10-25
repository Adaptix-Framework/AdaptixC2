/// Beacon TCP listener

function ListenerUI(mode_create)
{
    let spacer1 = form.create_vspacer()

    let labelPortBind = form.create_label("Bind port:");
    let spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(9000);
    spinPortBind.setEnabled(mode_create)

    let labelPrepend = form.create_label("Prepend data:");
    let textlinePrepend = form.create_textline("\\x12\\xabSimple\\x20word\\xa");
    textlinePrepend.setEnabled(mode_create)

    let labelEncryptKey = form.create_label("Encryption key:");
    let textlineEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textlineEncryptKey.setEnabled(mode_create)
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create)

    let spacer2 = form.create_vspacer()

    form.connect(buttonEncryptKey, "clicked", function() { textlineEncryptKey.setText( ax.random_string(32, "hex") ); });

    let layout = form.create_gridlayout();
    layout.addWidget(spacer1,            0, 0, 1, 3);
    layout.addWidget(labelPortBind,      1, 0, 1, 1);
    layout.addWidget(spinPortBind,       1, 1, 1, 2);
    layout.addWidget(labelPrepend,       2, 0, 1, 1);
    layout.addWidget(textlinePrepend,    2, 1, 1, 2);
    layout.addWidget(labelEncryptKey,    3, 0, 1, 1);
    layout.addWidget(textlineEncryptKey, 3, 1, 1, 1);
    layout.addWidget(buttonEncryptKey,   3, 2, 1, 1);
    layout.addWidget(spacer2,            4, 0, 1, 3);

    let container = form.create_container();
    container.put("port_bind", spinPortBind);
    container.put("prepend_data", textlinePrepend);
    container.put("encrypt_key", textlineEncryptKey);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}