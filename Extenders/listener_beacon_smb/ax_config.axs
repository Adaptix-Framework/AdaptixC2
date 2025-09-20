/// Beacon SMB listener

function ListenerUI(mode_create)
{
    let labelPipename = form.create_label("Pipename (C2):");
    let textlinePipename = form.create_textline();
    textlinePipename.setEnabled(mode_create)

    let labelEncryptKey = form.create_label("自定义密钥 (可选):");
    let textlineEncryptKey = form.create_textline();
    textlineEncryptKey.setEnabled(mode_create);
    textlineEncryptKey.setPlaceholderText("留空则自动生成随机密钥");

    let layout = form.create_gridlayout();
    layout.addWidget(labelPipename, 0, 0, 1, 1);
    layout.addWidget(textlinePipename, 0, 1, 1, 1);
    layout.addWidget(labelEncryptKey, 1, 0, 1, 1);
    layout.addWidget(textlineEncryptKey, 1, 1, 1, 1);

    let container = form.create_container();
    container.put("pipename", textlinePipename);
    container.put("encrypt_key_hex", textlineEncryptKey);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}