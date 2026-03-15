const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// Get full SoA
router.get('/', authenticate, async (req, res) => {
  try {
    const controls = await prisma.control.findMany({
      include: {
        soaEntries: { where: { organizationId: req.organizationId } },
        statuses: { where: { organizationId: req.organizationId } }
      },
      orderBy: { number: 'asc' }
    });

    const soa = controls.map(c => ({
      controlId: c.id,
      controlNumber: c.number,
      controlTitle: c.title,
      category: c.category,
      description: c.description,
      applicable: c.soaEntries[0]?.applicable ?? true,
      justification: c.soaEntries[0]?.justification || '',
      implemented: c.soaEntries[0]?.implemented ?? false,
      implementNote: c.soaEntries[0]?.implementNote || '',
      status: c.statuses[0]?.status || 'not_started',
      completionPct: c.statuses[0]?.completionPct || 0
    }));

    res.json(soa);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to fetch SoA' });
  }
});

// Update SoA entry
router.put('/:controlId', authenticate, async (req, res) => {
  try {
    const { applicable, justification, implemented, implementNote } = req.body;
    const entry = await prisma.soAEntry.upsert({
      where: {
        controlId_organizationId: {
          controlId: req.params.controlId,
          organizationId: req.organizationId
        }
      },
      update: { applicable, justification, implemented, implementNote },
      create: {
        controlId: req.params.controlId,
        organizationId: req.organizationId,
        applicable, justification, implemented, implementNote
      }
    });

    await prisma.auditLog.create({
      data: {
        userId: req.user.id,
        organizationId: req.organizationId,
        action: 'update_soa',
        entityType: 'soa',
        entityId: req.params.controlId,
        details: JSON.stringify({ applicable, implemented })
      }
    });

    res.json(entry);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to update SoA entry' });
  }
});

// Export SoA as JSON (for report generation)
router.get('/export', authenticate, async (req, res) => {
  try {
    const controls = await prisma.control.findMany({
      include: {
        soaEntries: { where: { organizationId: req.organizationId } },
        statuses: { where: { organizationId: req.organizationId } }
      },
      orderBy: { number: 'asc' }
    });

    const org = await prisma.organization.findUnique({ where: { id: req.organizationId } });

    const report = {
      title: 'Statement of Applicability - ISO 27001:2022',
      organization: org?.name,
      generatedAt: new Date().toISOString(),
      summary: {
        totalControls: controls.length,
        applicable: controls.filter(c => c.soaEntries[0]?.applicable !== false).length,
        notApplicable: controls.filter(c => c.soaEntries[0]?.applicable === false).length,
        implemented: controls.filter(c => c.soaEntries[0]?.implemented).length
      },
      controls: controls.map(c => ({
        number: c.number,
        title: c.title,
        category: c.category,
        applicable: c.soaEntries[0]?.applicable ?? true,
        justification: c.soaEntries[0]?.justification || '',
        implemented: c.soaEntries[0]?.implemented ?? false,
        implementNote: c.soaEntries[0]?.implementNote || '',
        status: c.statuses[0]?.status || 'not_started'
      }))
    };

    res.json(report);
  } catch (err) {
    res.status(500).json({ error: 'Failed to export SoA' });
  }
});

module.exports = router;
