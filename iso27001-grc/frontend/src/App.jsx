import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import { useAuth } from './context/AuthContext';
import Layout from './components/Layout';
import AuthPage from './pages/AuthPage';
import Dashboard from './pages/Dashboard';
import Clauses from './pages/Clauses';
import Controls from './pages/Controls';
import Policies from './pages/Policies';
import SoA from './pages/SoA';
import Risks from './pages/Risks';
import AuditReadiness from './pages/AuditReadiness';

function ProtectedRoute({ children }) {
  const { user, loading } = useAuth();
  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading...</div>;
  return user ? children : <Navigate to="/auth" />;
}

export default function App() {
  const { user, loading } = useAuth();

  if (loading) return <div style={{ padding: 40, textAlign: 'center' }}>Loading...</div>;

  return (
    <Routes>
      <Route path="/auth" element={user ? <Navigate to="/" /> : <AuthPage />} />
      <Route path="/" element={<ProtectedRoute><Layout /></ProtectedRoute>}>
        <Route index element={<Dashboard />} />
        <Route path="clauses" element={<Clauses />} />
        <Route path="controls" element={<Controls />} />
        <Route path="policies" element={<Policies />} />
        <Route path="soa" element={<SoA />} />
        <Route path="risks" element={<Risks />} />
        <Route path="readiness" element={<AuditReadiness />} />
      </Route>
    </Routes>
  );
}
