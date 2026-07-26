function GeneratePanel() {
    let buildEdit = form.create_textline("");
    buildEdit.setPlaceholder("x86_64-w64-mingw32-gcc -c %f -o %o -DBOF");

    let cwdEdit = form.create_textline("");
    cwdEdit.setPlaceholder("(project / file dir)");

    let hint = form.create_label("%f file · %o output · handlers: get_panel_data + job_start");

    let grid = form.create_gridlayout();
    grid.addWidget(form.create_label("Build:"), 0, 0, 1, 1);
    grid.addWidget(buildEdit, 0, 1, 1, 1);
    grid.addWidget(form.create_label("cwd:"), 1, 0, 1, 1);
    grid.addWidget(cwdEdit, 1, 1, 1, 1);
    grid.addWidget(hint, 2, 0, 1, 2);

    let panel = form.create_panel();
    panel.setLayout(grid);

    let container = form.create_container();
    container.put("build", buildEdit);
    container.put("cwd", cwdEdit);

    return {
        ui_panel: panel,
        ui_container: container
    };
}
