function __handlerMeta() {
  let p = editor.get_panel_data();
  let event = String(p.event || 'agent.new').trim();
  let name = String(p.name || 'auto_handler').trim() || 'auto_handler';
  let desc = String(p.description || ('Handler for ' + event));
  let group = String(p.group || name).trim() || name;
  let script = String(editor.content() || '').trim();
  let id = String(p.id || '').trim();
  let filters = {};

  function numList(v) {
    if (v === undefined || v === null || String(v).trim() === '') return null;
    let n = Number(v);
    return isFinite(n) ? [n] : null;
  }
  function strList(v) {
    if (v === undefined || v === null) return null;
    let s = String(v).trim();
    if (!s || s === '(any)') return null;
    return [s];
  }

  let v;
  if ((v = numList(p.agent_id))) filters.agent_ids = v;
  if ((v = strList(p.agent_name))) filters.agent_names = v;
  if ((v = strList(p.user))) filters.users = v;
  if ((v = strList(p.os))) filters.os = v;
  if ((v = strList(p.computer))) filters.computers = v;
  if ((v = strList(p.tags))) filters.tags = v;
  if ((v = strList(p.listener))) filters.listeners = v;
  if ((v = strList(p.listener_type))) filters.listener_types = v;
  if ((v = strList(p.listener_tag))) filters.listener_tags = v;
  if ((v = numList(p.task_id))) filters.task_ids = v;
  if ((v = strList(p.client))) filters.clients = v;
  if ((v = numList(p.file_id))) filters.file_ids = v;
  if ((v = strList(p.filename))) filters.filenames = v;
  if ((v = numList(p.port))) filters.ports = v;
  if ((v = strList(p.tunnel_type))) filters.tunnel_types = v;
  if ((v = strList(p.realm))) filters.realms = v;
  if ((v = strList(p.cred_type))) filters.cred_types = v;
  if ((v = strList(p.host))) filters.hosts = v;
  if ((v = strList(p.domain))) filters.domains = v;
  if ((v = strList(p.address))) filters.addresses = v;
  if (p.alive && String(p.alive).trim() && String(p.alive).trim() !== '(any)') {
    let a = String(p.alive).trim().toLowerCase();
    if (a === 'true' || a === '1' || a === 'yes') filters.alive = true;
    else if (a === 'false' || a === '0' || a === 'no') filters.alive = false;
  }

  return { id: id, name: name, event: event, description: desc, group: group, script: script, filters: filters };
}

/// Synthetic event for "Test local" only — never sent to the teamserver.
function __mockEvent(eventType, filters) {
  filters = filters || {};
  function first(arr, dflt) {
    return (arr && arr.length) ? arr[0] : dflt;
  }
  let agentId = Number(first(filters.agent_ids, 0)) || 0;
  let agentName = String(first(filters.agent_names, 'mock'));
  let user = String(first(filters.users, 'user'));
  let listener = String(first(filters.listeners, 'mock'));
  let os = String(first(filters.os, 'windows'));
  let client = String(first(filters.clients, 'local'));
  let computer = String(first(filters.computers, 'TEST-PC'));
  let tags = String(first(filters.tags, ''));
  let port = Number(first(filters.ports, 0)) || 0;
  let tunnelType = String(first(filters.tunnel_types, '0'));
  let filename = String(first(filters.filenames, 'file.bin'));
  let fileId = Number(first(filters.file_ids, 1)) || 1;
  let taskId = Number(first(filters.task_ids, 1)) || 1;

  return {
    type: eventType,
    event: eventType,
    phase: 'post',
    agentId: agentId,
    agentName: agentName,
    user: user,
    listener: listener,
    listenerType: String(first(filters.listener_types, '')),
    os: os,
    client: client,
    computer: computer,
    tags: tags,
    taskId: taskId,
    fileId: fileId,
    filename: filename,
    port: port,
    tunnelType: tunnelType,
    realm: String(first(filters.realms, '')),
    host: String(first(filters.hosts, '')),
    domain: String(first(filters.domains, '')),
    address: String(first(filters.addresses, '')),
    alive: (typeof filters.alive === 'boolean') ? filters.alive : true,
    agent: {
      a_id: agentId,
      a_name: agentName,
      a_listener: listener,
      a_computer: computer,
      a_username: user,
      a_tags: tags
    },
    restore: false
  };
}
