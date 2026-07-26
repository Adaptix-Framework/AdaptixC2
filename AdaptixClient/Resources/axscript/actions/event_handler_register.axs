let m = __handlerMeta();
if (!m.script) {
  editor.log('Script is empty — define function handler(event) { ... }');
} else {
  let meta = {
    name: m.name, event: m.event, group: m.group,
    description: m.description, script: m.script, enabled: true
  };
  if (m.id) meta.id = m.id;
  if (Object.keys(m.filters).length) meta.filters = m.filters;
  editor.log('Registering handler \'' + m.name + '\' for ' + m.event + '…');
  if (ax.event_handler_register(meta))
    editor.log('Register request sent — Scripts → Events will refresh.');
  else
    editor.log('Register failed to send — see log.');
}
