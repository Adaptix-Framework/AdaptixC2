/// Beacon TCP listener

function ListenerUI(mode_create)
{
    var spacer1 = form.create_vspacer()

    var labelPortBind = form.create_label("Bind port:");
    var spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(9000);
    spinPortBind.setEnabled(mode_create)

    var labelPrepend = form.create_label("Prepend data:");
    var textlinePrepend = form.create_textline("\\x12\\xabSimple\\x20word\\xa");
    textlinePrepend.setEnabled(mode_create)

    var spacer2 = form.create_vspacer()

    var layout = form.create_gridlayout();
    layout.addWidget(spacer1, 0, 0, 1, 2);
    layout.addWidget(labelPortBind, 1, 0, 1, 1);
    layout.addWidget(spinPortBind, 1, 1, 1, 1);
    layout.addWidget(labelPrepend, 2, 0, 1, 1);
    layout.addWidget(textlinePrepend, 2, 1, 1, 1);
    layout.addWidget(spacer2, 3, 0, 1, 2);

    var container = form.create_container();
    container.put("port_bind", spinPortBind);
    container.put("prepend_data", textlinePrepend);

    var panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}