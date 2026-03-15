import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';

export default function Risks() {
  const { apiFetch } = useAuth();
  const [risks, setRisks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [showForm, setShowForm] = useState(false);
  const [editing, setEditing] = useState(null);
  const [form, setForm] = useState({
    assetName: '', threatSource: '', vulnerability: '',
    likelihood: 3, impact: 3, treatment: 'mitigate', treatmentPlan: '', owner: ''
  });

  const fetchRisks = () => {
    apiFetch('/risks').then(setRisks).catch(console.error).finally(() => setLoading(false));
  };

  useEffect(() => { fetchRisks(); }, []);

  const riskLevel = (l, i) => {
    const s = l * i;
    if (s >= 16) return 'critical';
    if (s >= 11) return 'high';
    if (s >= 6) return 'medium';
    return 'low';
  };

  const saveRisk = async () => {
    try {
      if (editing) {
        await apiFetch(`/risks/${editing}`, { method: 'PUT', body: JSON.stringify(form) });
      } else {
        await apiFetch('/risks', { method: 'POST', body: JSON.stringify(form) });
      }
      fetchRisks();
      setShowForm(false);
      setEditing(null);
      setForm({ assetName: '', threatSource: '', vulnerability: '', likelihood: 3, impact: 3, treatment: 'mitigate', treatmentPlan: '', owner: '' });
    } catch (err) {
      alert(err.message);
    }
  };

  const deleteRisk = async (id) => {
    if (!confirm('Delete this risk?')) return;
    await apiFetch(`/risks/${id}`, { method: 'DELETE' });
    fetchRisks();
  };

  const updateRiskStatus = async (id, status) => {
    await apiFetch(`/risks/${id}`, { method: 'PUT', body: JSON.stringify({ status }) });
    fetchRisks();
  };

  const stats = {
    total: risks.length,
    critical: risks.filter(r => r.riskLevel === 'critical').length,
    high: risks.filter(r => r.riskLevel === 'high').length,
    medium: risks.filter(r => r.riskLevel === 'medium').length,
    low: risks.filter(r => r.riskLevel === 'low').length,
  };

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading...</div>;

  return (
    <div>
      <div className="page-header">
        <h2>Risk Register</h2>
        <p>Identify, assess, and treat information security risks as required by ISO 27001:2022 Clause 6.1 and 8.2.</p>
      </div>

      <div className="stats-grid">
        <div className="stat-card danger">
          <div className="stat-value">{stats.critical}</div>
          <div className="stat-label">Critical</div>
        </div>
        <div className="stat-card warning">
          <div className="stat-value">{stats.high}</div>
          <div className="stat-label">High</div>
        </div>
        <div className="stat-card info">
          <div className="stat-value">{stats.medium}</div>
          <div className="stat-label">Medium</div>
        </div>
        <div className="stat-card success">
          <div className="stat-value">{stats.low}</div>
          <div className="stat-label">Low</div>
        </div>
      </div>

      {/* Risk Matrix */}
      <div className="card" style={{ marginBottom: 16 }}>
        <div className="card-header"><h3>Risk Matrix (Likelihood x Impact)</h3></div>
        <div style={{ overflowX: 'auto' }}>
          <table style={{ textAlign: 'center', minWidth: 400 }}>
            <thead>
              <tr>
                <th>L \ I</th>
                <th>1 - Negligible</th><th>2 - Minor</th><th>3 - Moderate</th><th>4 - Major</th><th>5 - Severe</th>
              </tr>
            </thead>
            <tbody>
              {[5, 4, 3, 2, 1].map(l => (
                <tr key={l}>
                  <td><strong>{l}</strong></td>
                  {[1, 2, 3, 4, 5].map(i => {
                    const score = l * i;
                    const level = riskLevel(l, i);
                    const count = risks.filter(r => r.likelihood === l && r.impact === i).length;
                    const colors = { low: '#d1fae5', medium: '#fef3c7', high: '#fed7aa', critical: '#fecaca' };
                    return (
                      <td key={i} style={{ background: colors[level], fontWeight: count > 0 ? 700 : 400, fontSize: 13 }}>
                        {score} {count > 0 && <span style={{ background: 'rgba(0,0,0,0.15)', borderRadius: 99, padding: '1px 6px', fontSize: 11 }}>{count}</span>}
                      </td>
                    );
                  })}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      <div className="filter-bar">
        <button className="btn btn-primary" onClick={() => { setForm({ assetName: '', threatSource: '', vulnerability: '', likelihood: 3, impact: 3, treatment: 'mitigate', treatmentPlan: '', owner: '' }); setEditing(null); setShowForm(true); }}>
          + Add Risk
        </button>
      </div>

      {/* Risk Form Modal */}
      {showForm && (
        <div className="modal-overlay" onClick={() => setShowForm(false)}>
          <div className="modal" onClick={e => e.stopPropagation()}>
            <h3>{editing ? 'Edit Risk' : 'Add New Risk'}</h3>
            <div className="form-group">
              <label>Asset Name</label>
              <input value={form.assetName} onChange={e => setForm({ ...form, assetName: e.target.value })} placeholder="e.g., Customer Database" />
            </div>
            <div className="grid-2">
              <div className="form-group">
                <label>Threat Source</label>
                <input value={form.threatSource} onChange={e => setForm({ ...form, threatSource: e.target.value })} placeholder="e.g., External Attacker" />
              </div>
              <div className="form-group">
                <label>Vulnerability</label>
                <input value={form.vulnerability} onChange={e => setForm({ ...form, vulnerability: e.target.value })} placeholder="e.g., Unpatched software" />
              </div>
            </div>
            <div className="grid-2">
              <div className="form-group">
                <label>Likelihood (1-5): {form.likelihood}</label>
                <input type="range" min="1" max="5" value={form.likelihood} onChange={e => setForm({ ...form, likelihood: parseInt(e.target.value) })} />
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, color: 'var(--text-light)' }}>
                  <span>Rare</span><span>Unlikely</span><span>Possible</span><span>Likely</span><span>Certain</span>
                </div>
              </div>
              <div className="form-group">
                <label>Impact (1-5): {form.impact}</label>
                <input type="range" min="1" max="5" value={form.impact} onChange={e => setForm({ ...form, impact: parseInt(e.target.value) })} />
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, color: 'var(--text-light)' }}>
                  <span>Negligible</span><span>Minor</span><span>Moderate</span><span>Major</span><span>Severe</span>
                </div>
              </div>
            </div>
            <div style={{ textAlign: 'center', margin: '8px 0 16px', fontSize: 14 }}>
              Risk Score: <strong>{form.likelihood * form.impact}</strong> -
              <span className={`badge ${riskLevel(form.likelihood, form.impact)}`} style={{ marginLeft: 6 }}>
                {riskLevel(form.likelihood, form.impact)}
              </span>
            </div>
            <div className="grid-2">
              <div className="form-group">
                <label>Treatment</label>
                <select value={form.treatment} onChange={e => setForm({ ...form, treatment: e.target.value })}>
                  <option value="mitigate">Mitigate</option>
                  <option value="accept">Accept</option>
                  <option value="transfer">Transfer</option>
                  <option value="avoid">Avoid</option>
                </select>
              </div>
              <div className="form-group">
                <label>Risk Owner</label>
                <input value={form.owner} onChange={e => setForm({ ...form, owner: e.target.value })} placeholder="e.g., IT Manager" />
              </div>
            </div>
            <div className="form-group">
              <label>Treatment Plan</label>
              <textarea value={form.treatmentPlan} onChange={e => setForm({ ...form, treatmentPlan: e.target.value })} placeholder="Describe actions to treat this risk..." rows={4} />
            </div>
            <div className="modal-actions">
              <button className="btn btn-secondary" onClick={() => setShowForm(false)}>Cancel</button>
              <button className="btn btn-primary" onClick={saveRisk}>{editing ? 'Update' : 'Add Risk'}</button>
            </div>
          </div>
        </div>
      )}

      {/* Risks Table */}
      <div className="card">
        <div className="table-wrapper">
          <table>
            <thead>
              <tr>
                <th>Asset</th><th>Threat</th><th>Vulnerability</th>
                <th>L</th><th>I</th><th>Score</th><th>Level</th>
                <th>Treatment</th><th>Status</th><th>Owner</th><th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {risks.length === 0 ? (
                <tr><td colSpan={11} style={{ textAlign: 'center', padding: 20, color: 'var(--text-light)' }}>No risks identified yet. Click "Add Risk" to begin your risk assessment.</td></tr>
              ) : risks.map(risk => (
                <tr key={risk.id}>
                  <td><strong>{risk.assetName}</strong></td>
                  <td style={{ fontSize: 13 }}>{risk.threatSource}</td>
                  <td style={{ fontSize: 13 }}>{risk.vulnerability}</td>
                  <td>{risk.likelihood}</td>
                  <td>{risk.impact}</td>
                  <td><strong>{risk.likelihood * risk.impact}</strong></td>
                  <td><span className={`badge ${risk.riskLevel}`}>{risk.riskLevel}</span></td>
                  <td style={{ fontSize: 12, textTransform: 'capitalize' }}>{risk.treatment}</td>
                  <td>
                    <select
                      value={risk.status}
                      onChange={e => updateRiskStatus(risk.id, e.target.value)}
                      style={{ fontSize: 12, padding: '2px 4px', border: '1px solid var(--border)', borderRadius: 4 }}
                    >
                      <option value="identified">Identified</option>
                      <option value="assessed">Assessed</option>
                      <option value="treated">Treated</option>
                      <option value="monitored">Monitored</option>
                    </select>
                  </td>
                  <td style={{ fontSize: 12 }}>{risk.owner || '-'}</td>
                  <td>
                    <div style={{ display: 'flex', gap: 4 }}>
                      <button className="btn btn-sm btn-secondary" onClick={() => { setForm(risk); setEditing(risk.id); setShowForm(true); }}>Edit</button>
                      <button className="btn btn-sm btn-danger" onClick={() => deleteRisk(risk.id)}>Del</button>
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
