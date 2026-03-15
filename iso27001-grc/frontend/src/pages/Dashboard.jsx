import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';
import { Link } from 'react-router-dom';

export default function Dashboard() {
  const { apiFetch } = useAuth();
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch('/dashboard')
      .then(setData)
      .catch(console.error)
      .finally(() => setLoading(false));
  }, []);

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading dashboard...</div>;
  if (!data) return <div>Failed to load dashboard</div>;

  const scoreColor = data.overallScore >= 80 ? 'var(--success)' : data.overallScore >= 60 ? 'var(--primary)' : data.overallScore >= 40 ? 'var(--warning)' : 'var(--danger)';

  return (
    <div>
      <div className="page-header">
        <h2>Compliance Dashboard</h2>
        <p>ISO 27001:2022 compliance overview and audit readiness</p>
      </div>

      {/* Readiness Bar */}
      <div className="readiness-bar">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div>
            <div className="sublabel">Audit Readiness Level</div>
            <div className="level">{data.auditReadiness.level}</div>
          </div>
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontSize: 48, fontWeight: 700 }}>{data.overallScore}%</div>
            <div className="sublabel">Overall Compliance Score</div>
          </div>
        </div>
        <div className="progress-bar" style={{ marginTop: 12, height: 10, background: 'rgba(255,255,255,0.2)' }}>
          <div className="fill" style={{ width: `${data.overallScore}%`, background: 'white' }} />
        </div>
      </div>

      {/* Stats Grid */}
      <div className="stats-grid">
        <div className="stat-card primary">
          <div className="stat-value">{data.clauses.score}%</div>
          <div className="stat-label">ISMS Clauses Compliance</div>
          <div className="progress-bar" style={{ marginTop: 8 }}>
            <div className="fill blue" style={{ width: `${data.clauses.score}%` }} />
          </div>
        </div>
        <div className="stat-card success">
          <div className="stat-value">{data.controls.score}%</div>
          <div className="stat-label">Annex A Controls</div>
          <div className="progress-bar" style={{ marginTop: 8 }}>
            <div className="fill green" style={{ width: `${data.controls.score}%` }} />
          </div>
        </div>
        <div className="stat-card info">
          <div className="stat-value">{data.soa.implemented}/{data.soa.applicable}</div>
          <div className="stat-label">SoA Implemented</div>
        </div>
        <div className="stat-card warning">
          <div className="stat-value">{data.policies.approved}/{data.policies.total}</div>
          <div className="stat-label">Policies Approved</div>
        </div>
        <div className="stat-card danger">
          <div className="stat-value">{data.risks.total}</div>
          <div className="stat-label">Risks Identified</div>
        </div>
      </div>

      <div className="grid-2">
        {/* Clause Status Breakdown */}
        <div className="card">
          <div className="card-header">
            <h3>ISMS Clauses Status</h3>
            <Link to="/clauses" className="btn btn-sm btn-secondary">View All</Link>
          </div>
          <table>
            <thead><tr><th>Status</th><th>Count</th><th>%</th></tr></thead>
            <tbody>
              {Object.entries(data.clauses.byStatus).map(([status, count]) => (
                <tr key={status}>
                  <td><span className={`badge ${status}`}>{status.replace('_', ' ')}</span></td>
                  <td>{count}</td>
                  <td>{data.clauses.total ? Math.round(count / data.clauses.total * 100) : 0}%</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Control Status by Category */}
        <div className="card">
          <div className="card-header">
            <h3>Controls by Category</h3>
            <Link to="/controls" className="btn btn-sm btn-secondary">View All</Link>
          </div>
          <table>
            <thead><tr><th>Category</th><th>Total</th><th>Done</th><th>%</th></tr></thead>
            <tbody>
              {Object.entries(data.controls.byCategory).map(([cat, info]) => (
                <tr key={cat}>
                  <td>{cat}</td>
                  <td>{info.total}</td>
                  <td>{info.implemented}</td>
                  <td>
                    <div className="progress-bar" style={{ width: 80 }}>
                      <div className="fill green" style={{ width: `${info.total ? Math.round(info.implemented / info.total * 100) : 0}%` }} />
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Risk Summary */}
        <div className="card">
          <div className="card-header">
            <h3>Risk Overview</h3>
            <Link to="/risks" className="btn btn-sm btn-secondary">View All</Link>
          </div>
          {data.risks.total === 0 ? (
            <p style={{ color: 'var(--text-light)', fontSize: 14 }}>No risks identified yet. Start your risk assessment to track risks.</p>
          ) : (
            <table>
              <thead><tr><th>Level</th><th>Count</th></tr></thead>
              <tbody>
                {Object.entries(data.risks.byLevel).map(([level, count]) => (
                  <tr key={level}>
                    <td><span className={`badge ${level}`}>{level}</span></td>
                    <td>{count}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* What's Remaining */}
        <div className="card">
          <div className="card-header">
            <h3>Remaining Work</h3>
          </div>
          <table>
            <thead><tr><th>Item</th><th>Remaining</th></tr></thead>
            <tbody>
              <tr><td>ISMS Clauses to complete</td><td><strong>{data.remaining.clausesNotDone}</strong></td></tr>
              <tr><td>Annex A Controls to implement</td><td><strong>{data.remaining.controlsNotDone}</strong></td></tr>
              <tr><td>Policies pending approval</td><td><strong>{data.remaining.policiesNotApproved}</strong></td></tr>
              <tr><td>Risks needing treatment</td><td><strong>{data.remaining.risksUntreated}</strong></td></tr>
              <tr><td>SoA controls not implemented</td><td><strong>{data.remaining.soaNotImplemented}</strong></td></tr>
            </tbody>
          </table>
        </div>
      </div>

      {/* Audit Readiness Gaps */}
      {data.auditReadiness.gaps.length > 0 && (
        <div className="card">
          <div className="card-header">
            <h3>Audit Readiness Gaps</h3>
          </div>
          <ul className="gap-list">
            {data.auditReadiness.gaps.map((gap, i) => (
              <li key={i}>{gap}</li>
            ))}
          </ul>
        </div>
      )}
    </div>
  );
}
