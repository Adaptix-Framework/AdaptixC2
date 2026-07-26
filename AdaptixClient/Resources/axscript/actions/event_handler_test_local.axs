let m = __handlerMeta();
if (!m.script) {
  editor.log('Script is empty');
} else {
  let event = __mockEvent(m.event, m.filters);
  editor.log('Local eval of handler(event) for ' + m.event + ' (synthetic payload)…');
  try {
    eval(m.script);
    let fn = (typeof handler === 'function') ? handler : ((typeof onEvent === 'function') ? onEvent : null);
    if (!fn) throw 'define function handler(event) { ... }';
    fn(event);
    editor.log('Local test OK — no errors thrown.');
  } catch (e) {
    editor.log('Local test failed: ' + e);
  }
}
