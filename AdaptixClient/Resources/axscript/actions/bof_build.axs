editor.save();
let params = editor.get_panel_data();
let cmdline = editor.expand(String(params.build || ""));
if (!cmdline) {
    editor.log("params.build is empty");
} else {
    editor.job_start(cmdline, { id: "build", cwd: String(params.cwd || "") });
}
