const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// List evidence for a control or clause
router.get('/', authenticate, async (req, res) => {
  try {
    const { entityType, entityId } = req.query;
    const where = { organizationId: req.organizationId };
    if (entityType) where.entityType = entityType;
    if (entityId) where.entityId = entityId;

    const evidence = await prisma.evidence.findMany({
      where,
      orderBy: { createdAt: 'desc' }
    });
    res.json(evidence);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch evidence' });
  }
});

// Add evidence record
router.post('/', authenticate, async (req, res) => {
  try {
    const { name, description, fileType, filePath, entityType, entityId } = req.body;
    const evidence = await prisma.evidence.create({
      data: {
        organizationId: req.organizationId,
        name, description, fileType, filePath, entityType, entityId
      }
    });
    res.status(201).json(evidence);
  } catch (err) {
    res.status(500).json({ error: 'Failed to add evidence' });
  }
});

// Delete evidence
router.delete('/:id', authenticate, async (req, res) => {
  try {
    await prisma.evidence.delete({ where: { id: req.params.id } });
    res.json({ message: 'Evidence deleted' });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete evidence' });
  }
});

module.exports = router;
