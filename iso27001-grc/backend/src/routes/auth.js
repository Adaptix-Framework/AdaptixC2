const express = require('express');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const { PrismaClient } = require('@prisma/client');
const { authenticate, JWT_SECRET } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// Register
router.post('/register', async (req, res) => {
  try {
    const { email, password, name, organizationName } = req.body;
    if (!email || !password || !name) {
      return res.status(400).json({ error: 'Email, password, and name are required' });
    }

    const existing = await prisma.user.findUnique({ where: { email } });
    if (existing) return res.status(409).json({ error: 'Email already registered' });

    const hashed = await bcrypt.hash(password, 10);
    const user = await prisma.user.create({
      data: { email, password: hashed, name, role: 'admin' }
    });

    // Create organization and link user
    const org = await prisma.organization.create({
      data: {
        name: organizationName || `${name}'s Organization`,
        users: { create: { userId: user.id, role: 'owner' } }
      }
    });

    // Initialize clause statuses for this org
    const clauses = await prisma.clause.findMany();
    if (clauses.length > 0) {
      await prisma.clauseStatus.createMany({
        data: clauses.map(c => ({ clauseId: c.id, organizationId: org.id }))
      });
    }

    // Initialize control statuses
    const controls = await prisma.control.findMany();
    if (controls.length > 0) {
      await prisma.controlStatus.createMany({
        data: controls.map(c => ({ controlId: c.id, organizationId: org.id }))
      });
      // Initialize SoA entries
      await prisma.soAEntry.createMany({
        data: controls.map(c => ({ controlId: c.id, organizationId: org.id }))
      });
    }

    const token = jwt.sign({ userId: user.id, organizationId: org.id }, JWT_SECRET, { expiresIn: '7d' });
    res.status(201).json({
      token,
      user: { id: user.id, email: user.email, name: user.name, role: user.role },
      organization: { id: org.id, name: org.name }
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Registration failed' });
  }
});

// Login
router.post('/login', async (req, res) => {
  try {
    const { email, password } = req.body;
    const user = await prisma.user.findUnique({
      where: { email },
      include: { organizations: { include: { organization: true } } }
    });
    if (!user) return res.status(401).json({ error: 'Invalid credentials' });

    const valid = await bcrypt.compare(password, user.password);
    if (!valid) return res.status(401).json({ error: 'Invalid credentials' });

    const org = user.organizations[0]?.organization;
    const token = jwt.sign({ userId: user.id, organizationId: org?.id }, JWT_SECRET, { expiresIn: '7d' });

    res.json({
      token,
      user: { id: user.id, email: user.email, name: user.name, role: user.role },
      organization: org ? { id: org.id, name: org.name } : null
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Login failed' });
  }
});

// Get current user
router.get('/me', authenticate, async (req, res) => {
  const user = await prisma.user.findUnique({
    where: { id: req.user.id },
    include: { organizations: { include: { organization: true } } }
  });
  res.json({
    id: user.id, email: user.email, name: user.name, role: user.role,
    organizations: user.organizations.map(ou => ({ ...ou.organization, userRole: ou.role }))
  });
});

module.exports = router;
