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
  let agentId = Number(first(filters.agent_ids, 42)) || 42;
  let agentName = String(first(filters.agent_names, 'beacon'));
  let user = String(first(filters.users, 'user'));
  let listener = String(first(filters.listeners, 'http'));
  let listenerType = String(first(filters.listener_types, 'beacon_http'));
  let os = String(first(filters.os, 'windows'));
  let client = String(first(filters.clients, 'operator'));
  let computer = String(first(filters.computers, 'TEST-PC'));
  let tags = String(first(filters.tags, ''));
  let port = Number(first(filters.ports, 1080)) || 1080;
  let tunnelType = String(first(filters.tunnel_types, '1'));
  let filename = String(first(filters.filenames, 'file.bin'));
  let fileId = Number(first(filters.file_ids, 1)) || 1;
  let taskId = Number(first(filters.task_ids, 1001)) || 1001;
  let realm = String(first(filters.realms, 'CORP'));
  let host = String(first(filters.hosts, 'dc01.corp.local'));
  let domain = String(first(filters.domains, 'corp.local'));
  let address = String(first(filters.addresses, '10.0.0.10'));
  let credType = String(first(filters.cred_types, 'password'));
  let alive = (typeof filters.alive === 'boolean') ? filters.alive : true;

  let ev = {
    type: eventType,
    event: eventType,
    phase: 'post',
    agentId: agentId,
    agentName: agentName,
    user: user,
    username: client,
    listener: listener,
    listenerName: listener,
    listenerType: listenerType,
    os: os,
    client: client,
    computer: computer,
    tags: tags,
    taskId: taskId,
    fileId: fileId,
    filename: filename,
    fileName: filename,
    fileSize: 4096,
    port: port,
    tunnelType: tunnelType,
    tunnelId: 7,
    realm: realm,
    credType: credType,
    host: host,
    domain: domain,
    address: address,
    alive: alive,
    credId: 5,
    count: 1,
    screenId: 3,
    note: 'desktop',
    targetId: 9,
    pivotId: 'pivot-1',
    parentAgentId: agentId,
    childAgentId: agentId + 1,
    pivotName: 'smb',
    cmdline: 'whoami',
    remotePath: 'C:\\\\Windows\\\\Temp\\\\' + filename,
    canceled: false,
    info: 'socks',
    restore: false,
    agent: {
      a_id: agentId,
      a_name: agentName,
      a_listener: listener,
      a_computer: computer,
      a_username: user,
      a_tags: tags
    },
    task: {
      t_task_id: taskId,
      t_agent_id: agentId,
      t_client: client
    }
  };
  return ev;
}
