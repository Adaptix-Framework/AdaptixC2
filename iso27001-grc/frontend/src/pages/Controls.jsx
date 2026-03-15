import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';

const CATEGORIES = ['All', 'Organizational', 'People', 'Physical', 'Technological'];
const STATUSES = ['All', 'not_started', 'in_progress', 'implemented', 'verified', 'not_applicable'];

export default function Controls() {
  const { apiFetch } = useAuth();
  const [controls, setControls] = useState([]);
  const [loading, setLoading] = useState(true);
  const [categoryFilter, setCategoryFilter] = useState('All');
  const [statusFilter, setStatusFilter] = useState('All');
  const [search, setSearch] = useState('');
  const [selected, setSelected] = useState(null);

  const fetchControls = () => {
    apiFetch('/controls').then(setControls).catch(console.error).finally(() => setLoading(false));
  };

  useEffect(() => { fetchControls(); }, []);

  const updateStatus = async (controlId, data) => {
    try {
      await apiFetch(`/controls/${controlId}/status`, {
        method: 'PUT',
        body: JSON.stringify(data)
      });
      fetchControls();
    } catch (err) {
      alert(err.message);
    }
  };

  const getStatus = (ctrl) => ctrl.statuses?.[0] || { status: 'not_started', completionPct: 0, implementation: '', notes: '' };

  const filtered = controls.filter(c => {
    if (categoryFilter !== 'All' && c.category !== categoryFilter) return false;
    if (statusFilter !== 'All') {
      const s = getStatus(c);
      if (s.status !== statusFilter) return false;
    }
    if (search && !c.number.toLowerCase().includes(search.toLowerCase()) && !c.title.toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  // Group by category
  const grouped = {};
  filtered.forEach(c => {
    if (!grouped[c.category]) grouped[c.category] = [];
    grouped[c.category].push(c);
  });

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading controls...</div>;

  return (
    <div>
      <div className="page-header">
        <h2>Annex A Controls</h2>
        <p>ISO 27001:2022 Annex A - 93 controls across 4 categories. Track implementation status for each control.</p>
      </div>

      {/* Filters */}
      <div className="filter-bar">
        <select value={categoryFilter} onChange={e => setCategoryFilter(e.target.value)}>
          {CATEGORIES.map(c => <option key={c} value={c}>{c}</option>)}
        </select>
        <select value={statusFilter} onChange={e => setStatusFilter(e.target.value)}>
          {STATUSES.map(s => <option key={s} value={s}>{s === 'All' ? 'All Statuses' : s.replace('_', ' ')}</option>)}
        </select>
        <input
          placeholder="Search controls..."
          value={search}
          onChange={e => setSearch(e.target.value)}
        />
        <span style={{ fontSize: 13, color: 'var(--text-light)' }}>
          Showing {filtered.length} of {controls.length} controls
        </span>
      </div>

      {/* Controls list */}
      {Object.entries(grouped).map(([category, ctrls]) => (
        <div key={category} className="card" style={{ marginBottom: 16 }}>
          <div className="card-header">
            <h3>{category} Controls ({ctrls.length})</h3>
          </div>
          <div className="table-wrapper">
            <table>
              <thead>
                <tr>
                  <th style={{ width: 80 }}>Number</th>
                  <th>Title</th>
                  <th style={{ width: 120 }}>Status</th>
                  <th style={{ width: 100 }}>Progress</th>
                  <th style={{ width: 80 }}>Action</th>
                </tr>
              </thead>
              <tbody>
                {ctrls.map(ctrl => {
                  const status = getStatus(ctrl);
                  return (
                    <tr key={ctrl.id}>
                      <td><strong style={{ color: 'var(--primary)' }}>{ctrl.number}</strong></td>
                      <td>{ctrl.title}</td>
                      <td>
                        <select
                          value={status.status}
                          className={`badge ${status.status}`}
                          onChange={e => updateStatus(ctrl.id, { ...status, status: e.target.value })}
                          style={{ border: 'none', cursor: 'pointer', fontSize: 11, padding: '3px 6px' }}
                        >
                          <option value="not_started">Not Started</option>
                          <option value="in_progress">In Progress</option>
                          <option value="implemented">Implemented</option>
                          <option value="verified">Verified</option>
                          <option value="not_applicable">N/A</option>
                        </select>
                      </td>
                      <td>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                          <div className="progress-bar" style={{ flex: 1, height: 6 }}>
                            <div className="fill green" style={{ width: `${status.completionPct}%` }} />
                          </div>
                          <span style={{ fontSize: 12, minWidth: 32 }}>{status.completionPct}%</span>
                        </div>
                      </td>
                      <td>
                        <button className="btn btn-sm btn-secondary" onClick={() => setSelected(ctrl)}>
                          Details
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      ))}

      {/* Detail Modal */}
      {selected && (
        <div className="modal-overlay" onClick={() => setSelected(null)}>
          <div className="modal" onClick={e => e.stopPropagation()}>
            <h3>{selected.number} - {selected.title}</h3>
            <span className={`badge ${selected.category.toLowerCase()}`} style={{ background: '#dbeafe', color: '#1d4ed8', marginBottom: 12, display: 'inline-block' }}>
              {selected.category}
            </span>

            <div style={{ marginBottom: 16 }}>
              <h4 style={{ fontSize: 14, marginBottom: 4 }}>Description</h4>
              <p style={{ fontSize: 14, lineHeight: 1.7, color: 'var(--text)' }}>{selected.description}</p>
            </div>

            {selected.purpose && (
              <div style={{ marginBottom: 16, background: '#f0fdf4', padding: 12, borderRadius: 6 }}>
                <h4 style={{ fontSize: 14, marginBottom: 4, color: '#065f46' }}>Purpose</h4>
                <p style={{ fontSize: 13, color: '#047857' }}>{selected.purpose}</p>
              </div>
            )}

            {selected.guidance && (
              <div style={{ marginBottom: 16, background: '#eff6ff', padding: 12, borderRadius: 6 }}>
                <h4 style={{ fontSize: 14, marginBottom: 4, color: '#1d4ed8' }}>Implementation Guidance</h4>
                <p style={{ fontSize: 13, color: '#1e40af' }}>{selected.guidance}</p>
              </div>
            )}

            {/* Update status */}
            <div style={{ borderTop: '1px solid var(--border)', paddingTop: 16, marginTop: 16 }}>
              <h4 style={{ fontSize: 14, marginBottom: 8 }}>Update Status</h4>
              {(() => {
                const status = getStatus(selected);
                return (
                  <>
                    <div className="form-group">
                      <label>Status</label>
                      <select value={status.status} onChange={e => updateStatus(selected.id, { ...status, status: e.target.value })}>
                        <option value="not_started">Not Started</option>
                        <option value="in_progress">In Progress</option>
                        <option value="implemented">Implemented</option>
                        <option value="verified">Verified</option>
                        <option value="not_applicable">Not Applicable</option>
                      </select>
                    </div>
                    <div className="form-group">
                      <label>Completion: {status.completionPct}%</label>
                      <input type="range" min="0" max="100" value={status.completionPct}
                        onChange={e => updateStatus(selected.id, { ...status, completionPct: parseInt(e.target.value) })} />
                    </div>
                    <div className="form-group">
                      <label>Implementation Notes</label>
                      <textarea value={status.implementation || ''} rows={3}
                        onChange={e => updateStatus(selected.id, { ...status, implementation: e.target.value })}
                        placeholder="Describe how this control is implemented..." />
                    </div>
                  </>
                );
              })()}
            </div>

            <div className="modal-actions">
              <button className="btn btn-secondary" onClick={() => setSelected(null)}>Close</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
