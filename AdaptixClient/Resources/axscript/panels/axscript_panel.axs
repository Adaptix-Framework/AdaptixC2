function GeneratePanel() {

    let sw = form.create_switch("Main engine (REPL)");

    let layout = form.create_hlayout();
    layout.setContentsMargins(10, 6, 10, 6);
    layout.setSpacing(8);
    layout.addWidget(sw);
    layout.addStretch();

    let panel = form.create_panel();
    panel.setLayout(layout);

    let container = form.create_container();
    container.put("mainEngine", sw);

    return {
        ui_panel: panel,
        ui_container: container
    };
}
