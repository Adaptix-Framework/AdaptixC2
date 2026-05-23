/// Beacon Discord listener

function ListenerUI(mode_create)
{
    // BOT TOKEN
    let labelBotToken = form.create_label("Bot Token:");
    let textlineBotToken = form.create_textline();
    textlineBotToken.setPlaceholder("Discord bot token (server-side only)");
    textlineBotToken.setEnabled(mode_create);

    // CHANNEL IDS
    let labelChannelBeacon = form.create_label("Beacon Channel ID:");
    let textlineChannelBeacon = form.create_textline();
    textlineChannelBeacon.setPlaceholder("Channel for beacon -> server messages");
    textlineChannelBeacon.setEnabled(mode_create);

    let labelChannelTasks = form.create_label("Tasks Channel ID:");
    let textlineChannelTasks = form.create_textline();
    textlineChannelTasks.setPlaceholder("Channel for server -> beacon tasks");
    textlineChannelTasks.setEnabled(mode_create);

    // WEBHOOK URL
    let labelWebhook = form.create_label("Webhook URL:");
    let textlineWebhook = form.create_textline();
    textlineWebhook.setPlaceholder("https://discord.com/api/webhooks/...");

    // POLL INTERVAL
    let labelPollInterval = form.create_label("Poll Interval (seconds):");
    let spinPollInterval = form.create_spin();
    spinPollInterval.setRange(1, 60);
    spinPollInterval.setValue(5);

    // CLEANUP
    let checkCleanup = form.create_check("Delete messages after reading");
    checkCleanup.setChecked(true);

    // ENCRYPTION KEY
    let labelEncryptKey = form.create_label("Encryption key:");
    let textlineEncryptKey = form.create_textline(ax.random_string(64, "hex"));
    textlineEncryptKey.setEnabled(mode_create);
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create);

    form.connect(buttonEncryptKey, "clicked", function() { textlineEncryptKey.setText( ax.random_string(64, "hex") ); });

    // LAYOUT
    let layoutMain = form.create_gridlayout();
    layoutMain.addWidget(labelBotToken,        0, 0, 1, 1);
    layoutMain.addWidget(textlineBotToken,     0, 1, 1, 2);
    layoutMain.addWidget(labelChannelBeacon,   1, 0, 1, 1);
    layoutMain.addWidget(textlineChannelBeacon,1, 1, 1, 2);
    layoutMain.addWidget(labelChannelTasks,    2, 0, 1, 1);
    layoutMain.addWidget(textlineChannelTasks, 2, 1, 1, 2);
    layoutMain.addWidget(labelWebhook,         3, 0, 1, 1);
    layoutMain.addWidget(textlineWebhook,      3, 1, 1, 2);
    layoutMain.addWidget(labelPollInterval,    4, 0, 1, 1);
    layoutMain.addWidget(spinPollInterval,     4, 1, 1, 2);
    layoutMain.addWidget(checkCleanup,         5, 0, 1, 3);
    layoutMain.addWidget(labelEncryptKey,      6, 0, 1, 1);
    layoutMain.addWidget(textlineEncryptKey,   6, 1, 1, 1);
    layoutMain.addWidget(buttonEncryptKey,     6, 2, 1, 1);

    let panelMain = form.create_panel();
    panelMain.setLayout(layoutMain);

    let tabs = form.create_tabs();
    tabs.addTab(panelMain, "Main settings");

    let layout = form.create_hlayout();
    layout.addWidget(tabs);

    let container = form.create_container();
    container.put("bot_token", textlineBotToken);
    container.put("channel_beacon", textlineChannelBeacon);
    container.put("channel_tasks", textlineChannelTasks);
    container.put("webhook_url", textlineWebhook);
    container.put("poll_interval", spinPollInterval);
    container.put("cleanup", checkCleanup);
    container.put("encrypt_key", textlineEncryptKey);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 400,
        ui_width: 650
    }
}
