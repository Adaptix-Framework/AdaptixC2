import React from 'react';
import { NavLink, Outlet } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

const NAV_ITEMS = [
  { path: '/', label: 'Dashboard', icon: '\u{1F4CA}' },
  { path: '/clauses', label: 'ISMS Clauses (4-10)', icon: '\u{1F4CB}' },
  { path: '/controls', label: 'Annex A Controls', icon: '\u{1F6E1}' },
  { path: '/soa', label: 'Statement of Applicability', icon: '\u{1F4DD}' },
  { path: '/policies', label: 'Policies & Procedures', icon: '\u{1F4C4}' },
  { path: '/risks', label: 'Risk Register', icon: '\u{26A0}' },
  { path: '/readiness', label: 'Audit Readiness', icon: '\u{2705}' },
];

export default function Layout() {
  const { user, organization, logout } = useAuth();

  return (
    <div className="app-layout">
      <aside className="sidebar">
        <div className="sidebar-header">
          <h1>ISO 27001 GRC</h1>
          <p>{organization?.name || 'Organization'}</p>
        </div>
        <nav className="sidebar-nav">
          {NAV_ITEMS.map(item => (
            <NavLink
              key={item.path}
              to={item.path}
              end={item.path === '/'}
              className={({ isActive }) => isActive ? 'active' : ''}
            >
              <span className="nav-icon">{item.icon}</span>
              {item.label}
            </NavLink>
          ))}
        </nav>
        <div className="sidebar-footer">
          <div className="user-name">{user?.name}</div>
          <div className="user-email">{user?.email}</div>
          <button onClick={logout}>Sign Out</button>
        </div>
      </aside>
      <main className="main-content">
        <Outlet />
      </main>
    </div>
  );
}
