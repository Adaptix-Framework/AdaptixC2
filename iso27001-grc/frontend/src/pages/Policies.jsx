import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';

const TYPES = ['All', 'policy', 'procedure', 'guideline', 'standard'];

export default function Policies() {
  const { apiFetch } = useAuth();
  const [policies, setPolicies] = useState([]);
  const [templates, setTemplates] = useState([]);
  const [loading, setLoading] = useState(true);
  const [typeFilter, setTypeFilter] = useState('All');
  const [showEditor, setShowEditor] = useState(false);
  const [editing, setEditing] = useState(null);
  const [form, setForm] = useState({ title: '', type: 'policy', category: '', content: '', relatedClauses: '', relatedControls: '' });

  const fetchPolicies = () => {
    apiFetch('/policies').then(setPolicies).catch(console.error).finally(() => setLoading(false));
  };

  useEffect(() => {
    fetchPolicies();
    apiFetch('/policies/templates/list').then(setTemplates).catch(() => {});
  }, []);

  const savePol = async () => {
    try {
      if (editing) {
        await apiFetch(`/policies/${editing}`, { method: 'PUT', body: JSON.stringify(form) });
      } else {
        await apiFetch('/policies', { method: 'POST', body: JSON.stringify(form) });
      }
      fetchPolicies();
      setShowEditor(false);
      setEditing(null);
      setForm({ title: '', type: 'policy', category: '', content: '', relatedClauses: '', relatedControls: '' });
    } catch (err) {
      alert(err.message);
    }
  };

  const deletePol = async (id) => {
    if (!confirm('Delete this policy?')) return;
    await apiFetch(`/policies/${id}`, { method: 'DELETE' });
    fetchPolicies();
  };

  const updateStatus = async (id, status) => {
    await apiFetch(`/policies/${id}`, { method: 'PUT', body: JSON.stringify({ status }) });
    fetchPolicies();
  };

  const loadTemplate = (t) => {
    setForm({ title: t.title, type: t.type, category: t.category || '', content: t.content, relatedClauses: t.relatedClauses || '', relatedControls: t.relatedControls || '' });
    setEditing(null);
    setShowEditor(true);
  };

  const editPolicy = async (pol) => {
    setForm({ title: pol.title, type: pol.type, category: pol.category || '', content: pol.content, relatedClauses: pol.relatedClauses || '', relatedControls: pol.relatedControls || '' });
    setEditing(pol.id);
    setShowEditor(true);
  };

  const filtered = policies.filter(p => typeFilter === 'All' || p.type === typeFilter);

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading...</div>;

  return (
    <div>
      <div className="page-header">
        <h2>Policies, Procedures & Guidelines</h2>
        <p>Create and manage documentation required for ISO 27001:2022 compliance.</p>
      </div>

      {/* Stats */}
      <div className="stats-grid">
        <div className="stat-card primary">
          <div className="stat-value">{policies.length}</div>
          <div className="stat-label">Total Documents</div>
        </div>
        <div className="stat-card success">
          <div className="stat-value">{policies.filter(p => p.status === 'approved').length}</div>
          <div className="stat-label">Approved</div>
        </div>
        <div className="stat-card warning">
          <div className="stat-value">{policies.filter(p => p.status === 'draft').length}</div>
          <div className="stat-label">Draft</div>
        </div>
        <div className="stat-card info">
          <div className="stat-value">{policies.filter(p => p.status === 'review').length}</div>
          <div className="stat-label">In Review</div>
        </div>
      </div>

      {/* Actions */}
      <div className="filter-bar">
        <select value={typeFilter} onChange={e => setTypeFilter(e.target.value)}>
          {TYPES.map(t => <option key={t} value={t}>{t === 'All' ? 'All Types' : t.charAt(0).toUpperCase() + t.slice(1)}</option>)}
        </select>
        <button className="btn btn-primary" onClick={() => { setForm({ title: '', type: 'policy', category: '', content: '', relatedClauses: '', relatedControls: '' }); setEditing(null); setShowEditor(true); }}>
          + New Document
        </button>
      </div>

      {/* Templates */}
      {!showEditor && templates.length > 0 && (
        <div className="card">
          <div className="card-header">
            <h3>Quick Start Templates</h3>
          </div>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: 8 }}>
            {templates.map((t, i) => (
              <button key={i} className="btn btn-sm btn-secondary" onClick={() => loadTemplate(t)}>
                {t.title}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Editor Modal */}
      {showEditor && (
        <div className="modal-overlay" onClick={() => setShowEditor(false)}>
          <div className="modal" onClick={e => e.stopPropagation()} style={{ maxWidth: 800 }}>
            <h3>{editing ? 'Edit Document' : 'Create Document'}</h3>
            <div className="grid-2">
              <div className="form-group">
                <label>Title</label>
                <input value={form.title} onChange={e => setForm({ ...form, title: e.target.value })} />
              </div>
              <div className="form-group">
                <label>Type</label>
                <select value={form.type} onChange={e => setForm({ ...form, type: e.target.value })}>
                  <option value="policy">Policy</option>
                  <option value="procedure">Procedure</option>
                  <option value="guideline">Guideline</option>
                  <option value="standard">Standard</option>
                </select>
              </div>
            </div>
            <div className="grid-2">
              <div className="form-group">
                <label>Related Clauses (e.g. 5.1,5.2)</label>
                <input value={form.relatedClauses} onChange={e => setForm({ ...form, relatedClauses: e.target.value })} />
              </div>
              <div className="form-group">
                <label>Related Controls (e.g. A.5.1,A.5.2)</label>
                <input value={form.relatedControls} onChange={e => setForm({ ...form, relatedControls: e.target.value })} />
              </div>
            </div>
            <div className="form-group">
              <label>Content (Markdown supported)</label>
              <textarea value={form.content} onChange={e => setForm({ ...form, content: e.target.value })} rows={15} />
            </div>
            <div className="modal-actions">
              <button className="btn btn-secondary" onClick={() => setShowEditor(false)}>Cancel</button>
              <button className="btn btn-primary" onClick={savePol}>
                {editing ? 'Update' : 'Create'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Policies List */}
      <div className="card">
        <div className="table-wrapper">
          <table>
            <thead>
              <tr>
                <th>Title</th>
                <th>Type</th>
                <th>Status</th>
                <th>Version</th>
                <th>Related Controls</th>
                <th>Updated</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {filtered.length === 0 ? (
                <tr><td colSpan={7} style={{ textAlign: 'center', padding: 20, color: 'var(--text-light)' }}>No documents yet. Use a template or create a new document to get started.</td></tr>
              ) : filtered.map(pol => (
                <tr key={pol.id}>
                  <td><strong>{pol.title}</strong></td>
                  <td><span className={`badge ${pol.type}`} style={{ background: '#f1f5f9', color: '#475569' }}>{pol.type}</span></td>
                  <td><span className={`badge ${pol.status}`}>{pol.status}</span></td>
                  <td>v{pol.version}</td>
                  <td style={{ fontSize: 12 }}>{pol.relatedControls || '-'}</td>
                  <td style={{ fontSize: 12 }}>{new Date(pol.updatedAt).toLocaleDateString()}</td>
                  <td>
                    <div style={{ display: 'flex', gap: 4 }}>
                      <button className="btn btn-sm btn-secondary" onClick={() => editPolicy(pol)}>Edit</button>
                      {pol.status === 'draft' && (
                        <button className="btn btn-sm btn-primary" onClick={() => updateStatus(pol.id, 'review')}>Submit</button>
                      )}
                      {pol.status === 'review' && (
                        <button className="btn btn-sm btn-success" onClick={() => updateStatus(pol.id, 'approved')}>Approve</button>
                      )}
                      <button className="btn btn-sm btn-danger" onClick={() => deletePol(pol.id)}>Del</button>
                    </div>
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
