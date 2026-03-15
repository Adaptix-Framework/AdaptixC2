const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// Main dashboard with compliance metrics
router.get('/', authenticate, async (req, res) => {
  try {
    const orgId = req.organizationId;

    // Clause compliance
    const clauseStatuses = await prisma.clauseStatus.findMany({
      where: { organizationId: orgId }
    });
    const totalClauses = clauseStatuses.length;
    const clausesByStatus = { not_started: 0, in_progress: 0, implemented: 0, verified: 0 };
    let clauseCompletionSum = 0;
    clauseStatuses.forEach(s => {
      clausesByStatus[s.status] = (clausesByStatus[s.status] || 0) + 1;
      clauseCompletionSum += s.completionPct;
    });

    // Control compliance
    const controlStatuses = await prisma.controlStatus.findMany({
      where: { organizationId: orgId }
    });
    const totalControls = controlStatuses.length;
    const controlsByStatus = { not_started: 0, in_progress: 0, implemented: 0, verified: 0, not_applicable: 0 };
    const controlsByCategory = {};
    let controlCompletionSum = 0;
    controlStatuses.forEach(s => {
      controlsByStatus[s.status] = (controlsByStatus[s.status] || 0) + 1;
      controlCompletionSum += s.completionPct;
    });

    // Get controls with their categories for category breakdown
    const controlsWithCategory = await prisma.control.findMany({
      include: { statuses: { where: { organizationId: orgId } } }
    });
    controlsWithCategory.forEach(c => {
      if (!controlsByCategory[c.category]) {
        controlsByCategory[c.category] = { total: 0, implemented: 0, verified: 0 };
      }
      controlsByCategory[c.category].total++;
      const status = c.statuses[0]?.status;
      if (status === 'implemented' || status === 'verified') {
        controlsByCategory[c.category].implemented++;
      }
      if (status === 'verified') {
        controlsByCategory[c.category].verified++;
      }
    });

    // SoA summary
    const soaEntries = await prisma.soAEntry.findMany({
      where: { organizationId: orgId }
    });
    const soaSummary = {
      total: soaEntries.length,
      applicable: soaEntries.filter(s => s.applicable).length,
      notApplicable: soaEntries.filter(s => !s.applicable).length,
      implemented: soaEntries.filter(s => s.implemented).length
    };

    // Policies
    const policies = await prisma.policy.findMany({ where: { organizationId: orgId } });
    const policySummary = { total: policies.length, draft: 0, review: 0, approved: 0, retired: 0 };
    policies.forEach(p => {
      policySummary[p.status] = (policySummary[p.status] || 0) + 1;
    });

    // Risks
    const risks = await prisma.riskAssessment.findMany({ where: { organizationId: orgId } });
    const riskSummary = {
      total: risks.length,
      byLevel: { low: 0, medium: 0, high: 0, critical: 0 },
      untreated: risks.filter(r => r.status === 'identified' || r.status === 'assessed').length
    };
    risks.forEach(r => {
      riskSummary.byLevel[r.riskLevel] = (riskSummary.byLevel[r.riskLevel] || 0) + 1;
    });

    // Overall compliance score
    const applicableControls = totalControls - controlsByStatus.not_applicable;
    const overallClauseScore = totalClauses > 0 ? Math.round(clauseCompletionSum / totalClauses) : 0;
    const overallControlScore = applicableControls > 0 ? Math.round(controlCompletionSum / applicableControls) : 0;
    const overallScore = Math.round((overallClauseScore + overallControlScore) / 2);

    // Audit readiness
    const auditReadiness = {
      score: overallScore,
      level: overallScore >= 80 ? 'Ready' : overallScore >= 60 ? 'Nearly Ready' : overallScore >= 40 ? 'In Progress' : 'Early Stage',
      gaps: [],
    };

    // Identify gaps
    if (policySummary.approved < 5) {
      auditReadiness.gaps.push('Fewer than 5 approved policies - auditors expect core policies to be approved');
    }
    if (controlsByStatus.not_started > totalControls * 0.3) {
      auditReadiness.gaps.push(`${controlsByStatus.not_started} controls not yet started (${Math.round(controlsByStatus.not_started / totalControls * 100)}%)`);
    }
    if (riskSummary.total === 0) {
      auditReadiness.gaps.push('No risk assessments performed - this is a mandatory requirement');
    }
    if (riskSummary.untreated > 0) {
      auditReadiness.gaps.push(`${riskSummary.untreated} risks need treatment decisions`);
    }
    if (soaSummary.total > 0 && soaSummary.implemented < soaSummary.applicable * 0.5) {
      auditReadiness.gaps.push('Less than 50% of applicable controls are implemented in SoA');
    }

    // Remaining items
    const remaining = {
      clausesNotDone: clausesByStatus.not_started + clausesByStatus.in_progress,
      controlsNotDone: controlsByStatus.not_started + controlsByStatus.in_progress,
      policiesNotApproved: policySummary.total - policySummary.approved,
      risksUntreated: riskSummary.untreated,
      soaNotImplemented: soaSummary.applicable - soaSummary.implemented
    };

    res.json({
      overallScore,
      clauses: { total: totalClauses, byStatus: clausesByStatus, score: overallClauseScore },
      controls: { total: totalControls, byStatus: controlsByStatus, byCategory: controlsByCategory, score: overallControlScore },
      soa: soaSummary,
      policies: policySummary,
      risks: riskSummary,
      auditReadiness,
      remaining
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to fetch dashboard data' });
  }
});

// Audit log
router.get('/audit-log', authenticate, async (req, res) => {
  try {
    const logs = await prisma.auditLog.findMany({
      where: { organizationId: req.organizationId },
      include: { user: { select: { name: true, email: true } } },
      orderBy: { createdAt: 'desc' },
      take: 50
    });
    res.json(logs);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch audit log' });
  }
});

module.exports = router;
