const express = require('express');
const cors = require('cors');
const morgan = require('morgan');

const authRoutes = require('./routes/auth');
const clauseRoutes = require('./routes/clauses');
const controlRoutes = require('./routes/controls');
const policyRoutes = require('./routes/policies');
const soaRoutes = require('./routes/soa');
const riskRoutes = require('./routes/risks');
const dashboardRoutes = require('./routes/dashboard');
const evidenceRoutes = require('./routes/evidence');

const app = express();
const PORT = process.env.PORT || 3001;

app.use(cors());
app.use(express.json({ limit: '10mb' }));
app.use(morgan('dev'));

app.use('/api/auth', authRoutes);
app.use('/api/clauses', clauseRoutes);
app.use('/api/controls', controlRoutes);
app.use('/api/policies', policyRoutes);
app.use('/api/soa', soaRoutes);
app.use('/api/risks', riskRoutes);
app.use('/api/dashboard', dashboardRoutes);
app.use('/api/evidence', evidenceRoutes);

app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).json({ error: 'Internal server error' });
});

app.listen(PORT, () => {
  console.log(`ISO 27001 GRC API running on port ${PORT}`);
});
