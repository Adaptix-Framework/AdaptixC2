package axscript

import (
	"path/filepath"

	"github.com/dop251/goja"
)

func newStubWidget(rt *goja.Runtime) *goja.Object {
	obj := rt.NewObject()
	obj.Set("setEnabled", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setVisible", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setPlaceholder", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setRange", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setValue", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setChecked", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setLayout", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setPanel", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("setSize", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("addWidget", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("addItem", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("addItems", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("addSeparator", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("text", func(goja.FunctionCall) goja.Value { return rt.ToValue("") })
	obj.Set("setText", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("value", func(goja.FunctionCall) goja.Value { return rt.ToValue(0) })
	obj.Set("isChecked", func(goja.FunctionCall) goja.Value { return rt.ToValue(false) })
	obj.Set("currentText", func(goja.FunctionCall) goja.Value { return rt.ToValue("") })
	obj.Set("currentIndex", func(goja.FunctionCall) goja.Value { return rt.ToValue(0) })
	obj.Set("setCurrentIndex", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	obj.Set("filePath", func(goja.FunctionCall) goja.Value { return rt.ToValue("") })
	obj.Set("exec", func(goja.FunctionCall) goja.Value { return rt.ToValue(false) })
	return obj
}

func registerFormStubs(engine *ScriptEngine) {
	rt := engine.runtime

	formObj := rt.NewObject()

	// Layout
	formObj.Set("create_vlayout", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_hlayout", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_grid_layout", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_gridlayout", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Separators / Spacers
	formObj.Set("create_vline", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_hline", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_vspacer", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_hspacer", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_separator", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Basic widgets
	formObj.Set("create_label", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_textline", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_combo", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_checkbox", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_check", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_spin", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_dateline", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_timeline", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_button", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_textmulti", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_textarea", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_input", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_input_number", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_list", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_table", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Containers
	formObj.Set("create_tabs", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_groupbox", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_hsplitter", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_vsplitter", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_scrollarea", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_panel", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_stack", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_dialog", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Selectors
	formObj.Set("create_file_chooser", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_file", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_credentials", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_agents", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_listeners", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_targets", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_selector_downloads", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Extensions
	formObj.Set("create_ext_dock", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	formObj.Set("create_ext_dialog", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	formObj.Set("create_container", func(call goja.FunctionCall) goja.Value {
		obj := rt.NewObject()
		data := make(map[string]interface{})
		obj.Set("put", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) >= 2 {
				data[call.Argument(0).String()] = call.Argument(1).Export()
			}
			return goja.Undefined()
		})
		obj.Set("get", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) >= 1 {
				if v, ok := data[call.Argument(0).String()]; ok {
					return rt.ToValue(v)
				}
			}
			return goja.Undefined()
		})
		return obj
	})

	formObj.Set("connect", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })

	rt.Set("form", formObj)
}

func registerMenuStubs(engine *ScriptEngine) {
	rt := engine.runtime

	menuObj := rt.NewObject()

	// Create
	menuObj.Set("create_action", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	menuObj.Set("create_item", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	menuObj.Set("create_separator", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	menuObj.Set("create_menu", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })
	menuObj.Set("create_submenu", func(call goja.FunctionCall) goja.Value { return newStubWidget(rt) })

	// Legacy register
	menuObj.Set("register_agent", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("register_sessions", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("register_listener", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Session
	menuObj.Set("add_session_main", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_session_agent", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_session_browser", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_session_access", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Browsers
	menuObj.Set("add_filebrowser", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_processbrowser", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Downloads
	menuObj.Set("add_downloads_running", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_downloads_finished", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_downloads_completed", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Tasks
	menuObj.Set("add_tasks", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_tasks_job", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_tasks_task", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Targets & Credentials
	menuObj.Set("add_targets", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_credentials", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	// Main menu
	menuObj.Set("add_main", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_main_projects", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_main_axscript", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	menuObj.Set("add_main_settings", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	rt.Set("menu", menuObj)
}

func registerEventStubs(engine *ScriptEngine) {
	rt := engine.runtime

	eventObj := rt.NewObject()

	eventObj.Set("on_filebrowser_disks", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_filebrowser_files", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_filebrowser_list", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_filebrowser_upload", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_processbrowser", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_processbrowser_list", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_new_agent", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_disconnect", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_ready", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_interval", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("on_timeout", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	eventObj.Set("list", func(goja.FunctionCall) goja.Value { return rt.ToValue([]interface{}{}) })
	eventObj.Set("remove", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	rt.Set("event", eventObj)
}

func fileBasename(path string) string {
	return filepath.Base(path)
}
