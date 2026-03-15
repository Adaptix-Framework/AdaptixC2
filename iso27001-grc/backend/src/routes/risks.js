const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

function calculateRiskLevel(likelihood, impact) {
  const score = likelihood * impact;
  if (score >= 16) return 'critical';
  if (score >= 11) return 'high';
  if (score >= 6) return 'medium';
  return 'low';
}

// List risks
router.get('/', authenticate, async (req, res) => {
  try {
    const { status, riskLevel } = req.query;
    const where = { organizationId: req.organizationId };
    if (status) where.status = status;
    if (riskLevel) where.riskLevel = riskLevel;

    const risks = await prisma.riskAssessment.findMany({
      where,
      orderBy: { createdAt: 'desc' }
    });
    res.json(risks);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch risks' });
  }
});

// Create risk
router.post('/', authenticate, async (req, res) => {
  try {
    const { assetName, threatSource, vulnerability, likelihood, impact, treatment, treatmentPlan, owner } = req.body;
    const riskLevel = calculateRiskLevel(likelihood, impact);

    const risk = await prisma.riskAssessment.create({
      data: {
        organizationId: req.organizationId,
        assetName, threatSource, vulnerability,
        likelihood, impact, riskLevel,
        treatment, treatmentPlan, owner
      }
    });

    res.status(201).json(risk);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to create risk' });
  }
});

// Update risk
router.put('/:id', authenticate, async (req, res) => {
  try {
    const data = { ...req.body };
    if (data.likelihood && data.impact) {
      data.riskLevel = calculateRiskLevel(data.likelihood, data.impact);
    }
    const risk = await prisma.riskAssessment.update({
      where: { id: req.params.id },
      data
    });
    res.json(risk);
  } catch (err) {
    res.status(500).json({ error: 'Failed to update risk' });
  }
});

// Delete risk
router.delete('/:id', authenticate, async (req, res) => {
  try {
    await prisma.riskAssessment.delete({ where: { id: req.params.id } });
    res.json({ message: 'Risk deleted' });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete risk' });
  }
});

// Risk summary/stats
router.get('/summary', authenticate, async (req, res) => {
  try {
    const risks = await prisma.riskAssessment.findMany({
      where: { organizationId: req.organizationId }
    });

    const summary = {
      total: risks.length,
      byLevel: { low: 0, medium: 0, high: 0, critical: 0 },
      byTreatment: { accept: 0, mitigate: 0, transfer: 0, avoid: 0 },
      byStatus: { identified: 0, assessed: 0, treated: 0, monitored: 0 }
    };

    risks.forEach(r => {
      summary.byLevel[r.riskLevel] = (summary.byLevel[r.riskLevel] || 0) + 1;
      summary.byTreatment[r.treatment] = (summary.byTreatment[r.treatment] || 0) + 1;
      summary.byStatus[r.status] = (summary.byStatus[r.status] || 0) + 1;
    });

    res.json(summary);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch risk summary' });
  }
});

module.exports = router;
