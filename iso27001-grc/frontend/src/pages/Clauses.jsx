import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';

export default function Clauses() {
  const { apiFetch } = useAuth();
  const [clauses, setClauses] = useState([]);
  const [expanded, setExpanded] = useState({});
  const [editing, setEditing] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch('/clauses').then(setClauses).catch(console.error).finally(() => setLoading(false));
  }, []);

  const toggle = (id) => setExpanded(prev => ({ ...prev, [id]: !prev[id] }));

  const updateStatus = async (clauseId, data) => {
    try {
      await apiFetch(`/clauses/${clauseId}/status`, {
        method: 'PUT',
        body: JSON.stringify(data)
      });
      // Refresh
      const updated = await apiFetch('/clauses');
      setClauses(updated);
      setEditing(null);
    } catch (err) {
      alert(err.message);
    }
  };

  const getStatus = (clause) => clause.statuses?.[0] || { status: 'not_started', completionPct: 0, notes: '' };

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading clauses...</div>;

  return (
    <div>
      <div className="page-header">
        <h2>ISMS Clauses (4-10)</h2>
        <p>ISO 27001:2022 mandatory requirements. Track compliance for each clause and sub-clause.</p>
      </div>

      {clauses.map(clause => {
        const status = getStatus(clause);
        return (
          <div key={clause.id} className="accordion-item">
            <div className="accordion-header" onClick={() => toggle(clause.id)}>
              <div className="title">
                <span className="number">Clause {clause.number}</span>
                <span>{clause.title}</span>
              </div>
              <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                <span className={`badge ${status.status}`}>{status.status.replace('_', ' ')}</span>
                <span style={{ fontSize: 18 }}>{expanded[clause.id] ? '\u25B2' : '\u25BC'}</span>
              </div>
            </div>
            {expanded[clause.id] && (
              <div className="accordion-body">
                <p style={{ marginBottom: 12, fontSize: 14, lineHeight: 1.7 }}>{clause.description}</p>
                {clause.guidance && (
                  <div style={{ background: '#eff6ff', padding: 12, borderRadius: 6, marginBottom: 12, fontSize: 13 }}>
                    <strong>Guidance:</strong> {clause.guidance}
                  </div>
                )}

                {/* Status update for this clause */}
                <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 16 }}>
                  <select
                    value={status.status}
                    onChange={e => updateStatus(clause.id, { ...status, status: e.target.value })}
                    style={{ padding: '6px 10px', borderRadius: 4, border: '1px solid var(--border)', fontSize: 13 }}
                  >
                    <option value="not_started">Not Started</option>
                    <option value="in_progress">In Progress</option>
                    <option value="implemented">Implemented</option>
                    <option value="verified">Verified</option>
                  </select>
                  <input
                    type="range" min="0" max="100" value={status.completionPct}
                    onChange={e => updateStatus(clause.id, { ...status, completionPct: parseInt(e.target.value) })}
                    style={{ flex: 1 }}
                  />
                  <span style={{ fontSize: 13, fontWeight: 600, minWidth: 40 }}>{status.completionPct}%</span>
                </div>

                {/* Sub-clauses */}
                {clause.children?.map(sub => {
                  const subStatus = getStatus(sub);
                  return (
                    <div key={sub.id} style={{ marginLeft: 20, marginBottom: 12 }}>
                      <div className="accordion-item">
                        <div className="accordion-header" onClick={() => toggle(sub.id)}>
                          <div className="title">
                            <span className="number">{sub.number}</span>
                            <span>{sub.title}</span>
                          </div>
                          <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                            <span className={`badge ${subStatus.status}`}>{subStatus.status.replace('_', ' ')}</span>
                            <span style={{ fontSize: 14 }}>{expanded[sub.id] ? '\u25B2' : '\u25BC'}</span>
                          </div>
                        </div>
                        {expanded[sub.id] && (
                          <div className="accordion-body">
                            <p style={{ marginBottom: 8, fontSize: 14, lineHeight: 1.7 }}>{sub.description}</p>
                            {sub.guidance && (
                              <div style={{ background: '#eff6ff', padding: 12, borderRadius: 6, marginBottom: 12, fontSize: 13 }}>
                                <strong>Guidance:</strong> {sub.guidance}
                              </div>
                            )}
                            <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 8 }}>
                              <select
                                value={subStatus.status}
                                onChange={e => updateStatus(sub.id, { ...subStatus, status: e.target.value })}
                                style={{ padding: '6px 10px', borderRadius: 4, border: '1px solid var(--border)', fontSize: 13 }}
                              >
                                <option value="not_started">Not Started</option>
                                <option value="in_progress">In Progress</option>
                                <option value="implemented">Implemented</option>
                                <option value="verified">Verified</option>
                              </select>
                              <input
                                type="range" min="0" max="100" value={subStatus.completionPct}
                                onChange={e => updateStatus(sub.id, { ...subStatus, completionPct: parseInt(e.target.value) })}
                                style={{ flex: 1 }}
                              />
                              <span style={{ fontSize: 13, fontWeight: 600, minWidth: 40 }}>{subStatus.completionPct}%</span>
                            </div>

                            {/* Sub-sub clauses */}
                            {sub.children?.map(subsub => {
                              const ssubStatus = getStatus(subsub);
                              return (
                                <div key={subsub.id} style={{ marginLeft: 20, marginTop: 8, padding: 10, background: 'white', borderRadius: 6, border: '1px solid var(--border)' }}>
                                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
                                    <strong style={{ fontSize: 13 }}>{subsub.number} - {subsub.title}</strong>
                                    <span className={`badge ${ssubStatus.status}`}>{ssubStatus.status.replace('_', ' ')}</span>
                                  </div>
                                  <p style={{ fontSize: 13, lineHeight: 1.6, marginBottom: 8 }}>{subsub.description}</p>
                                  {subsub.guidance && (
                                    <div style={{ background: '#eff6ff', padding: 8, borderRadius: 4, marginBottom: 8, fontSize: 12 }}>
                                      <strong>Guidance:</strong> {subsub.guidance}
                                    </div>
                                  )}
                                  <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                                    <select
                                      value={ssubStatus.status}
                                      onChange={e => updateStatus(subsub.id, { ...ssubStatus, status: e.target.value })}
                                      style={{ padding: '4px 8px', borderRadius: 4, border: '1px solid var(--border)', fontSize: 12 }}
                                    >
                                      <option value="not_started">Not Started</option>
                                      <option value="in_progress">In Progress</option>
                                      <option value="implemented">Implemented</option>
                                      <option value="verified">Verified</option>
                                    </select>
                                    <input
                                      type="range" min="0" max="100" value={ssubStatus.completionPct}
                                      onChange={e => updateStatus(subsub.id, { ...ssubStatus, completionPct: parseInt(e.target.value) })}
                                      style={{ flex: 1 }}
                                    />
                                    <span style={{ fontSize: 12, fontWeight: 600 }}>{ssubStatus.completionPct}%</span>
                                  </div>
                                </div>
                              );
                            })}
                          </div>
                        )}
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        );
      })}
    </div>
  );
}
