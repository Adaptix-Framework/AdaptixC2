import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';

const CATEGORIES = ['All', 'Organizational', 'People', 'Physical', 'Technological'];

export default function SoA() {
  const { apiFetch } = useAuth();
  const [soa, setSoa] = useState([]);
  const [loading, setLoading] = useState(true);
  const [categoryFilter, setCategoryFilter] = useState('All');
  const [showOnlyApplicable, setShowOnlyApplicable] = useState(false);
  const [editingId, setEditingId] = useState(null);
  const [editForm, setEditForm] = useState({});

  const fetchSoA = () => {
    apiFetch('/soa').then(setSoa).catch(console.error).finally(() => setLoading(false));
  };

  useEffect(() => { fetchSoA(); }, []);

  const updateSoA = async (controlId, data) => {
    try {
      await apiFetch(`/soa/${controlId}`, {
        method: 'PUT',
        body: JSON.stringify(data)
      });
      fetchSoA();
      setEditingId(null);
    } catch (err) {
      alert(err.message);
    }
  };

  const filtered = soa.filter(s => {
    if (categoryFilter !== 'All' && s.category !== categoryFilter) return false;
    if (showOnlyApplicable && !s.applicable) return false;
    return true;
  });

  const stats = {
    total: soa.length,
    applicable: soa.filter(s => s.applicable).length,
    notApplicable: soa.filter(s => !s.applicable).length,
    implemented: soa.filter(s => s.implemented).length,
  };

  const exportSoA = async () => {
    try {
      const report = await apiFetch('/soa/export');
      const blob = new Blob([JSON.stringify(report, null, 2)], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'soa-report.json';
      a.click();
      URL.revokeObjectURL(url);
    } catch (err) {
      alert(err.message);
    }
  };

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading SoA...</div>;

  return (
    <div>
      <div className="page-header">
        <h2>Statement of Applicability</h2>
        <p>Define which Annex A controls are applicable to your organization and track their implementation.</p>
      </div>

      {/* Stats */}
      <div className="stats-grid">
        <div className="stat-card primary">
          <div className="stat-value">{stats.total}</div>
          <div className="stat-label">Total Controls</div>
        </div>
        <div className="stat-card success">
          <div className="stat-value">{stats.applicable}</div>
          <div className="stat-label">Applicable</div>
        </div>
        <div className="stat-card warning">
          <div className="stat-value">{stats.notApplicable}</div>
          <div className="stat-label">Not Applicable</div>
        </div>
        <div className="stat-card info">
          <div className="stat-value">{stats.implemented}</div>
          <div className="stat-label">Implemented</div>
        </div>
      </div>

      {/* Filters */}
      <div className="filter-bar">
        <select value={categoryFilter} onChange={e => setCategoryFilter(e.target.value)}>
          {CATEGORIES.map(c => <option key={c} value={c}>{c}</option>)}
        </select>
        <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 14 }}>
          <input type="checkbox" checked={showOnlyApplicable} onChange={e => setShowOnlyApplicable(e.target.checked)} />
          Show only applicable
        </label>
        <button className="btn btn-sm btn-secondary" onClick={exportSoA}>Export SoA</button>
      </div>

      {/* SoA Table */}
      <div className="card">
        <div className="table-wrapper">
          <table>
            <thead>
              <tr>
                <th>Control</th>
                <th>Title</th>
                <th>Category</th>
                <th>Applicable</th>
                <th>Implemented</th>
                <th>Justification</th>
                <th>Action</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map(entry => (
                <tr key={entry.controlId}>
                  <td><strong style={{ color: 'var(--primary)' }}>{entry.controlNumber}</strong></td>
                  <td style={{ fontSize: 13 }}>{entry.controlTitle}</td>
                  <td><span style={{ fontSize: 12 }}>{entry.category}</span></td>
                  <td>
                    <input
                      type="checkbox"
                      checked={entry.applicable}
                      onChange={e => updateSoA(entry.controlId, { ...entry, applicable: e.target.checked })}
                    />
                  </td>
                  <td>
                    <input
                      type="checkbox"
                      checked={entry.implemented}
                      disabled={!entry.applicable}
                      onChange={e => updateSoA(entry.controlId, { ...entry, implemented: e.target.checked })}
                    />
                  </td>
                  <td style={{ fontSize: 12, maxWidth: 200 }}>
                    {editingId === entry.controlId ? (
                      <div style={{ display: 'flex', gap: 4 }}>
                        <input
                          style={{ fontSize: 12, padding: '2px 4px', flex: 1, border: '1px solid var(--border)', borderRadius: 4 }}
                          value={editForm.justification || ''}
                          onChange={e => setEditForm({ ...editForm, justification: e.target.value })}
                          placeholder="Justification..."
                        />
                        <button className="btn btn-sm btn-primary" onClick={() => updateSoA(entry.controlId, { ...entry, justification: editForm.justification })}>Save</button>
                      </div>
                    ) : (
                      <span style={{ color: entry.justification ? 'var(--text)' : 'var(--text-light)' }}>
                        {entry.justification || 'Click edit to add'}
                      </span>
                    )}
                  </td>
                  <td>
                    <button
                      className="btn btn-sm btn-secondary"
                      onClick={() => {
                        setEditingId(entry.controlId);
                        setEditForm({ justification: entry.justification, implementNote: entry.implementNote });
                      }}
                    >
                      Edit
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
