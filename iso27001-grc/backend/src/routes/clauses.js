const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// Get all clauses with hierarchy and status
router.get('/', authenticate, async (req, res) => {
  try {
    const clauses = await prisma.clause.findMany({
      where: { level: 0 },
      include: {
        children: {
          include: {
            children: {
              include: {
                statuses: { where: { organizationId: req.organizationId } }
              }
            },
            statuses: { where: { organizationId: req.organizationId } }
          },
          orderBy: { number: 'asc' }
        },
        statuses: { where: { organizationId: req.organizationId } }
      },
      orderBy: { number: 'asc' }
    });
    res.json(clauses);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to fetch clauses' });
  }
});

// Get single clause detail
router.get('/:id', authenticate, async (req, res) => {
  try {
    const clause = await prisma.clause.findUnique({
      where: { id: req.params.id },
      include: {
        children: {
          include: {
            statuses: { where: { organizationId: req.organizationId } }
          },
          orderBy: { number: 'asc' }
        },
        statuses: { where: { organizationId: req.organizationId } }
      }
    });
    if (!clause) return res.status(404).json({ error: 'Clause not found' });
    res.json(clause);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch clause' });
  }
});

// Update clause status for organization
router.put('/:clauseId/status', authenticate, async (req, res) => {
  try {
    const { status, notes, completionPct } = req.body;
    const updated = await prisma.clauseStatus.upsert({
      where: {
        clauseId_organizationId: {
          clauseId: req.params.clauseId,
          organizationId: req.organizationId
        }
      },
      update: { status, notes, completionPct },
      create: {
        clauseId: req.params.clauseId,
        organizationId: req.organizationId,
        status, notes, completionPct
      }
    });

    await prisma.auditLog.create({
      data: {
        userId: req.user.id,
        organizationId: req.organizationId,
        action: 'update_clause_status',
        entityType: 'clause',
        entityId: req.params.clauseId,
        details: JSON.stringify({ status, completionPct })
      }
    });

    res.json(updated);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to update clause status' });
  }
});

module.exports = router;
