const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// Get all controls grouped by category
router.get('/', authenticate, async (req, res) => {
  try {
    const { category, status } = req.query;
    const where = {};
    if (category) where.category = category;

    const controls = await prisma.control.findMany({
      where,
      include: {
        statuses: { where: { organizationId: req.organizationId } }
      },
      orderBy: { number: 'asc' }
    });

    // Filter by status if requested
    let filtered = controls;
    if (status) {
      filtered = controls.filter(c => {
        const s = c.statuses[0];
        return s ? s.status === status : status === 'not_started';
      });
    }

    res.json(filtered);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to fetch controls' });
  }
});

// Get control detail
router.get('/:id', authenticate, async (req, res) => {
  try {
    const control = await prisma.control.findUnique({
      where: { id: req.params.id },
      include: {
        statuses: { where: { organizationId: req.organizationId } },
        soaEntries: { where: { organizationId: req.organizationId } }
      }
    });
    if (!control) return res.status(404).json({ error: 'Control not found' });
    res.json(control);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch control' });
  }
});

// Update control status
router.put('/:controlId/status', authenticate, async (req, res) => {
  try {
    const { status, implementation, evidence, notes, completionPct } = req.body;
    const updated = await prisma.controlStatus.upsert({
      where: {
        controlId_organizationId: {
          controlId: req.params.controlId,
          organizationId: req.organizationId
        }
      },
      update: { status, implementation, evidence, notes, completionPct },
      create: {
        controlId: req.params.controlId,
        organizationId: req.organizationId,
        status, implementation, evidence, notes, completionPct
      }
    });

    await prisma.auditLog.create({
      data: {
        userId: req.user.id,
        organizationId: req.organizationId,
        action: 'update_control_status',
        entityType: 'control',
        entityId: req.params.controlId,
        details: JSON.stringify({ status, completionPct })
      }
    });

    res.json(updated);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to update control status' });
  }
});

module.exports = router;
