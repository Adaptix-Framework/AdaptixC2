function GeneratePanel() {
    let buildEdit = form.create_textline("");
    buildEdit.setPlaceholder("shell command · %f file · %o out · %d defines");

    let cwdEdit = form.create_textline("");
    cwdEdit.setPlaceholder("(project / file dir)");

    let row = 0;
    let grid = form.create_gridlayout();
    grid.addWidget(form.create_label("Build:"), row, 0, 1, 1);
    grid.addWidget(buildEdit, row, 1, 1, 1); row++;
    grid.addWidget(form.create_label("cwd:"), row, 0, 1, 1);
    grid.addWidget(cwdEdit, row, 1, 1, 1); row++;

    let panel = form.create_panel();
    panel.setLayout(grid);

    let container = form.create_container();
    container.put("cmd", buildEdit);
    container.put("cwd", cwdEdit);

    return {
        ui_panel: panel,
        ui_container: container
    };
}
