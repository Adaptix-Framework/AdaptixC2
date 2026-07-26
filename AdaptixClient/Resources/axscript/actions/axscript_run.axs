let params = editor.get_panel_data();
let code = editor.content();
editor.eval(code, { main: !!params.mainEngine });
