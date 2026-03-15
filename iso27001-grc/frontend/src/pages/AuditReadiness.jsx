import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';
import { Link } from 'react-router-dom';

export default function AuditReadiness() {
  const { apiFetch } = useAuth();
  const [data, setData] = useState(null);
  const [auditLog, setAuditLog] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    Promise.all([
      apiFetch('/dashboard'),
      apiFetch('/dashboard/audit-log')
    ]).then(([dashboard, logs]) => {
      setData(dashboard);
      setAuditLog(logs);
    }).catch(console.error).finally(() => setLoading(false));
  }, []);

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading...</div>;
  if (!data) return <div>Failed to load data</div>;

  const readinessColor = data.overallScore >= 80 ? 'var(--success)' : data.overallScore >= 60 ? 'var(--primary)' : data.overallScore >= 40 ? 'var(--warning)' : 'var(--danger)';

  // Checklist items
  const checklist = [
    { label: 'ISMS scope defined and documented', done: true, link: '/clauses', detail: 'Clause 4.3' },
    { label: 'Information security policy approved', done: data.policies.approved > 0, link: '/policies', detail: 'Clause 5.2' },
    { label: 'Risk assessment performed', done: data.risks.total > 0, link: '/risks', detail: 'Clause 6.1.2, 8.2' },
    { label: 'Risk treatment plan defined', done: data.risks.total > 0 && data.risks.byLevel.critical + data.risks.byLevel.high < data.risks.total, link: '/risks', detail: 'Clause 6.1.3, 8.3' },
    { label: 'Statement of Applicability completed', done: data.soa.total > 0 && data.soa.applicable > 0, link: '/soa', detail: 'Clause 6.1.3' },
    { label: 'Core policies created (minimum 5)', done: data.policies.total >= 5, link: '/policies', detail: 'A.5.1' },
    { label: 'Core policies approved', done: data.policies.approved >= 5, link: '/policies', detail: 'A.5.1' },
    { label: 'More than 50% of controls addressed', done: data.controls.byStatus.implemented + data.controls.byStatus.verified > data.controls.total * 0.5, link: '/controls', detail: 'Annex A' },
    { label: 'More than 50% of ISMS clauses addressed', done: data.clauses.byStatus.implemented + data.clauses.byStatus.verified > data.clauses.total * 0.5, link: '/clauses', detail: 'Clauses 4-10' },
    { label: 'Internal audit conducted', done: false, link: '/clauses', detail: 'Clause 9.2' },
    { label: 'Management review conducted', done: false, link: '/clauses', detail: 'Clause 9.3' },
    { label: 'Corrective actions tracked', done: false, link: '/clauses', detail: 'Clause 10.2' },
  ];

  const completedCount = checklist.filter(c => c.done).length;
  const checklistScore = Math.round(completedCount / checklist.length * 100);

  return (
    <div>
      <div className="page-header">
        <h2>Audit Readiness</h2>
        <p>Assess your organization's readiness for ISO 27001:2022 external certification audit.</p>
      </div>

      {/* Big readiness indicator */}
      <div className="readiness-bar" style={{ marginBottom: 24 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div>
            <div className="sublabel">Certification Readiness</div>
            <div className="level">{data.auditReadiness.level}</div>
          </div>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: 56, fontWeight: 700 }}>{data.overallScore}%</div>
            <div className="sublabel">Overall Score</div>
          </div>
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontSize: 28, fontWeight: 600 }}>{checklistScore}%</div>
            <div className="sublabel">Checklist ({completedCount}/{checklist.length})</div>
          </div>
        </div>
        <div className="progress-bar" style={{ marginTop: 16, height: 12, background: 'rgba(255,255,255,0.2)' }}>
          <div className="fill" style={{ width: `${data.overallScore}%`, background: 'white' }} />
        </div>
      </div>

      <div className="grid-2">
        {/* Audit Checklist */}
        <div className="card">
          <div className="card-header"><h3>Audit Preparation Checklist</h3></div>
          <table>
            <thead><tr><th style={{ width: 30 }}></th><th>Requirement</th><th>Reference</th><th></th></tr></thead>
            <tbody>
              {checklist.map((item, i) => (
                <tr key={i}>
                  <td style={{ fontSize: 18 }}>{item.done ? '\u2705' : '\u2B1C'}</td>
                  <td style={{ fontWeight: item.done ? 400 : 500, textDecoration: item.done ? 'line-through' : 'none', color: item.done ? 'var(--text-light)' : 'var(--text)' }}>
                    {item.label}
                  </td>
                  <td style={{ fontSize: 12, color: 'var(--text-light)' }}>{item.detail}</td>
                  <td><Link to={item.link} className="btn btn-sm btn-secondary">Go</Link></td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Compliance Breakdown */}
        <div>
          <div className="card">
            <div className="card-header"><h3>Compliance Breakdown</h3></div>
            <table>
              <thead><tr><th>Area</th><th>Score</th><th>Progress</th></tr></thead>
              <tbody>
                <tr>
                  <td>ISMS Clauses</td>
                  <td><strong>{data.clauses.score}%</strong></td>
                  <td><div className="progress-bar" style={{ width: 120 }}><div className="fill blue" style={{ width: `${data.clauses.score}%` }} /></div></td>
                </tr>
                <tr>
                  <td>Annex A Controls</td>
                  <td><strong>{data.controls.score}%</strong></td>
                  <td><div className="progress-bar" style={{ width: 120 }}><div className="fill green" style={{ width: `${data.controls.score}%` }} /></div></td>
                </tr>
                <tr>
                  <td>SoA Coverage</td>
                  <td><strong>{data.soa.applicable > 0 ? Math.round(data.soa.implemented / data.soa.applicable * 100) : 0}%</strong></td>
                  <td><div className="progress-bar" style={{ width: 120 }}><div className="fill green" style={{ width: `${data.soa.applicable > 0 ? Math.round(data.soa.implemented / data.soa.applicable * 100) : 0}%` }} /></div></td>
                </tr>
                <tr>
                  <td>Policies</td>
                  <td><strong>{data.policies.total > 0 ? Math.round(data.policies.approved / data.policies.total * 100) : 0}%</strong></td>
                  <td><div className="progress-bar" style={{ width: 120 }}><div className="fill yellow" style={{ width: `${data.policies.total > 0 ? Math.round(data.policies.approved / data.policies.total * 100) : 0}%` }} /></div></td>
                </tr>
              </tbody>
            </table>
          </div>

          {/* Gaps */}
          {data.auditReadiness.gaps.length > 0 && (
            <div className="card">
              <div className="card-header"><h3>Identified Gaps</h3></div>
              <ul className="gap-list">
                {data.auditReadiness.gaps.map((gap, i) => (
                  <li key={i}>{gap}</li>
                ))}
              </ul>
            </div>
          )}

          {/* Remaining Work */}
          <div className="card">
            <div className="card-header"><h3>Remaining Work to Complete</h3></div>
            <table>
              <tbody>
                <tr><td>ISMS Clauses incomplete</td><td style={{ fontWeight: 600 }}>{data.remaining.clausesNotDone}</td></tr>
                <tr><td>Controls not implemented</td><td style={{ fontWeight: 600 }}>{data.remaining.controlsNotDone}</td></tr>
                <tr><td>Policies awaiting approval</td><td style={{ fontWeight: 600 }}>{data.remaining.policiesNotApproved}</td></tr>
                <tr><td>Risks without treatment</td><td style={{ fontWeight: 600 }}>{data.remaining.risksUntreated}</td></tr>
                <tr><td>SoA controls pending</td><td style={{ fontWeight: 600 }}>{data.remaining.soaNotImplemented}</td></tr>
              </tbody>
            </table>
          </div>
        </div>
      </div>

      {/* Recent Activity */}
      <div className="card" style={{ marginTop: 16 }}>
        <div className="card-header"><h3>Recent Activity Log</h3></div>
        <div className="table-wrapper">
          <table>
            <thead><tr><th>Time</th><th>User</th><th>Action</th><th>Details</th></tr></thead>
            <tbody>
              {auditLog.length === 0 ? (
                <tr><td colSpan={4} style={{ textAlign: 'center', color: 'var(--text-light)' }}>No activity recorded yet.</td></tr>
              ) : auditLog.slice(0, 20).map(log => (
                <tr key={log.id}>
                  <td style={{ fontSize: 12 }}>{new Date(log.createdAt).toLocaleString()}</td>
                  <td style={{ fontSize: 13 }}>{log.user?.name || 'System'}</td>
                  <td style={{ fontSize: 13 }}>{log.action.replace(/_/g, ' ')}</td>
                  <td style={{ fontSize: 12, color: 'var(--text-light)' }}>{log.details || '-'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
