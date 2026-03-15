import React, { useState } from 'react';
import { useAuth } from '../context/AuthContext';

export default function AuthPage() {
  const { login, register } = useAuth();
  const [isRegister, setIsRegister] = useState(false);
  const [form, setForm] = useState({ name: '', email: '', password: '', organizationName: '' });
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setLoading(true);
    try {
      if (isRegister) {
        await register(form.name, form.email, form.password, form.organizationName);
      } else {
        await login(form.email, form.password);
      }
    } catch (err) {
      setError(err.message);
    }
    setLoading(false);
  };

  const set = (field) => (e) => setForm({ ...form, [field]: e.target.value });

  return (
    <div className="auth-page">
      <div className="auth-card">
        <h1>{isRegister ? 'Create Account' : 'Sign In'}</h1>
        <p>ISO 27001:2022 GRC Platform</p>

        {error && <div className="error">{error}</div>}

        <form onSubmit={handleSubmit}>
          {isRegister && (
            <>
              <div className="form-group">
                <label>Full Name</label>
                <input type="text" value={form.name} onChange={set('name')} required />
              </div>
              <div className="form-group">
                <label>Organization Name</label>
                <input type="text" value={form.organizationName} onChange={set('organizationName')} required />
              </div>
            </>
          )}
          <div className="form-group">
            <label>Email</label>
            <input type="email" value={form.email} onChange={set('email')} required />
          </div>
          <div className="form-group">
            <label>Password</label>
            <input type="password" value={form.password} onChange={set('password')} required minLength={6} />
          </div>
          <button className="btn btn-primary" type="submit" disabled={loading}>
            {loading ? 'Please wait...' : (isRegister ? 'Create Account' : 'Sign In')}
          </button>
        </form>

        <div className="toggle">
          {isRegister ? 'Already have an account?' : "Don't have an account?"}{' '}
          <a onClick={() => { setIsRegister(!isRegister); setError(''); }}>
            {isRegister ? 'Sign In' : 'Register'}
          </a>
        </div>
      </div>
    </div>
  );
}
