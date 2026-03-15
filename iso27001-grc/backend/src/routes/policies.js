const express = require('express');
const { PrismaClient } = require('@prisma/client');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const prisma = new PrismaClient();

// List policies
router.get('/', authenticate, async (req, res) => {
  try {
    const { type, status } = req.query;
    const where = { organizationId: req.organizationId };
    if (type) where.type = type;
    if (status) where.status = status;

    const policies = await prisma.policy.findMany({
      where,
      orderBy: { updatedAt: 'desc' }
    });
    res.json(policies);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch policies' });
  }
});

// Get single policy
router.get('/:id', authenticate, async (req, res) => {
  try {
    const policy = await prisma.policy.findFirst({
      where: { id: req.params.id, organizationId: req.organizationId }
    });
    if (!policy) return res.status(404).json({ error: 'Policy not found' });
    res.json(policy);
  } catch (err) {
    res.status(500).json({ error: 'Failed to fetch policy' });
  }
});

// Create policy
router.post('/', authenticate, async (req, res) => {
  try {
    const { title, type, category, content, relatedClauses, relatedControls } = req.body;
    const policy = await prisma.policy.create({
      data: {
        organizationId: req.organizationId,
        title, type, category, content,
        relatedClauses, relatedControls
      }
    });

    await prisma.auditLog.create({
      data: {
        userId: req.user.id,
        organizationId: req.organizationId,
        action: 'create_policy',
        entityType: 'policy',
        entityId: policy.id,
        details: `Created ${type}: ${title}`
      }
    });

    res.status(201).json(policy);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Failed to create policy' });
  }
});

// Update policy
router.put('/:id', authenticate, async (req, res) => {
  try {
    const { title, type, category, content, status, version, relatedClauses, relatedControls } = req.body;
    const data = {};
    if (title !== undefined) data.title = title;
    if (type !== undefined) data.type = type;
    if (category !== undefined) data.category = category;
    if (content !== undefined) data.content = content;
    if (version !== undefined) data.version = version;
    if (relatedClauses !== undefined) data.relatedClauses = relatedClauses;
    if (relatedControls !== undefined) data.relatedControls = relatedControls;
    if (status !== undefined) {
      data.status = status;
      if (status === 'approved') {
        data.approvedBy = req.user.name;
        data.approvedAt = new Date();
      }
    }

    const policy = await prisma.policy.update({
      where: { id: req.params.id },
      data
    });
    res.json(policy);
  } catch (err) {
    res.status(500).json({ error: 'Failed to update policy' });
  }
});

// Delete policy
router.delete('/:id', authenticate, async (req, res) => {
  try {
    await prisma.policy.delete({ where: { id: req.params.id } });
    res.json({ message: 'Policy deleted' });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete policy' });
  }
});

// Get policy templates
router.get('/templates/list', authenticate, async (req, res) => {
  res.json(POLICY_TEMPLATES);
});

const POLICY_TEMPLATES = [
  {
    title: "Information Security Policy",
    type: "policy",
    category: "Organizational",
    relatedClauses: "5.1,5.2",
    relatedControls: "A.5.1,A.5.2,A.5.3",
    content: `# Information Security Policy

## 1. Purpose
This policy establishes the framework for managing information security within [Organization Name], ensuring the confidentiality, integrity, and availability of information assets.

## 2. Scope
This policy applies to all employees, contractors, consultants, and third parties who access organizational information and information systems.

## 3. Policy Statement
[Organization Name] is committed to protecting its information assets from all threats, whether internal or external, deliberate or accidental. Management shall:
- Approve and publish an information security policy
- Assign information security responsibilities
- Ensure adequate resources for information security
- Review the policy at planned intervals

## 4. Objectives
- Protect the confidentiality of information
- Ensure the integrity of information
- Maintain the availability of information systems
- Comply with legal, regulatory, and contractual obligations
- Support continual improvement of the ISMS

## 5. Roles and Responsibilities
- **Top Management**: Provide leadership and commitment to the ISMS
- **Information Security Manager**: Oversee day-to-day security operations
- **All Staff**: Comply with security policies and report incidents

## 6. Review
This policy shall be reviewed annually or when significant changes occur.`
  },
  {
    title: "Access Control Policy",
    type: "policy",
    category: "Technological",
    relatedControls: "A.5.15,A.5.16,A.5.17,A.5.18,A.8.2,A.8.3,A.8.5",
    content: `# Access Control Policy

## 1. Purpose
Define the rules for granting, managing, and revoking access to information systems and data.

## 2. Scope
All information systems, applications, and data repositories within the organization.

## 3. Policy
### 3.1 Access Control Principles
- Access shall be granted on a need-to-know and least-privilege basis
- All access rights shall be formally authorized before being provisioned
- Access rights shall be reviewed periodically

### 3.2 User Registration and De-registration
- Formal user registration and de-registration procedures shall be implemented
- Unique user IDs shall be assigned to each user
- Shared/group IDs shall not be permitted except where operationally necessary

### 3.3 Privileged Access Management
- Privileged access rights shall be restricted and controlled
- Privileged access shall use separate accounts from normal user accounts
- All privileged access shall be logged and monitored

### 3.4 Authentication
- Multi-factor authentication shall be required for remote access and privileged accounts
- Password requirements: minimum 12 characters, complexity enforced
- Password reuse shall be prevented for at least 10 previous passwords

### 3.5 Access Review
- User access rights shall be reviewed at least quarterly
- Privileged access shall be reviewed monthly
- Access shall be revoked immediately upon termination

## 4. Compliance
Violations of this policy may result in disciplinary action.`
  },
  {
    title: "Risk Assessment Procedure",
    type: "procedure",
    category: "Organizational",
    relatedClauses: "6.1,8.2,8.3",
    relatedControls: "A.5.1",
    content: `# Risk Assessment Procedure

## 1. Purpose
Define the methodology for identifying, analyzing, and evaluating information security risks.

## 2. Risk Assessment Process

### 2.1 Asset Identification
- Identify all information assets within scope
- Assign asset owners
- Determine asset value (confidentiality, integrity, availability)

### 2.2 Threat Identification
- Identify potential threats to each asset
- Consider threat sources: natural, human, environmental
- Document threat scenarios

### 2.3 Vulnerability Assessment
- Identify vulnerabilities that could be exploited by threats
- Review technical vulnerabilities, process weaknesses, and personnel gaps
- Use vulnerability scanning tools where applicable

### 2.4 Risk Analysis
- Likelihood: Rate 1-5 (Rare to Almost Certain)
- Impact: Rate 1-5 (Negligible to Severe)
- Risk Level = Likelihood × Impact
  - 1-5: Low (Green)
  - 6-10: Medium (Yellow)
  - 11-15: High (Orange)
  - 16-25: Critical (Red)

### 2.5 Risk Evaluation
- Compare risks against risk acceptance criteria
- Prioritize risks for treatment
- Document risk owners

### 2.6 Risk Treatment
Options:
- **Mitigate**: Apply controls to reduce risk
- **Accept**: Formally accept the risk with management approval
- **Transfer**: Transfer risk via insurance or outsourcing
- **Avoid**: Eliminate the activity causing the risk

## 3. Documentation
- Maintain risk register with all identified risks
- Record treatment decisions and plans
- Review and update at least annually`
  },
  {
    title: "Incident Response Procedure",
    type: "procedure",
    category: "Organizational",
    relatedControls: "A.5.24,A.5.25,A.5.26,A.5.27,A.6.8",
    content: `# Incident Response Procedure

## 1. Purpose
Establish a structured approach to managing information security incidents.

## 2. Incident Categories
- **P1 Critical**: Data breach, ransomware, system-wide outage
- **P2 High**: Unauthorized access, malware infection, targeted attack
- **P3 Medium**: Policy violation, suspicious activity, minor vulnerability exploitation
- **P4 Low**: Failed login attempts, spam, social engineering attempts

## 3. Response Process

### 3.1 Detection & Reporting
- All staff must report suspected incidents immediately
- Report via: [incident email/phone/ticketing system]
- Automated detection via monitoring tools and SIEM

### 3.2 Triage & Classification
- Security team classifies severity within 30 minutes
- Assign incident handler
- Initiate communication plan

### 3.3 Containment
- Isolate affected systems
- Preserve evidence
- Implement temporary fixes

### 3.4 Eradication
- Remove root cause
- Patch vulnerabilities
- Verify malware removal

### 3.5 Recovery
- Restore systems from clean backups
- Validate system functionality
- Monitor for recurrence

### 3.6 Post-Incident Review
- Conduct lessons learned within 5 business days
- Update risk register
- Improve controls as needed
- Document findings

## 4. Reporting Obligations
- Report notifiable breaches to relevant authorities within required timeframes
- Notify affected data subjects where required by law`
  },
  {
    title: "Acceptable Use Policy",
    type: "policy",
    category: "People",
    relatedControls: "A.5.10,A.5.11,A.5.12,A.8.1",
    content: `# Acceptable Use Policy

## 1. Purpose
Define acceptable use of organizational information systems and assets.

## 2. Scope
All employees, contractors, and third parties using organizational systems.

## 3. General Use
- Information systems are provided primarily for business purposes
- Limited personal use is permitted if it does not impact productivity or security
- Users must not engage in any illegal activities using organizational systems

## 4. Email and Communications
- Do not send sensitive information via unencrypted email
- Do not open suspicious attachments or click unknown links
- Business communications should use approved platforms only

## 5. Internet Use
- Do not access inappropriate or illegal websites
- Do not download unauthorized software
- Use of VPN is required for remote access

## 6. Data Handling
- Classify data according to the data classification policy
- Do not store sensitive data on personal devices without authorization
- Use encryption for sensitive data in transit and at rest

## 7. Passwords
- Never share your password with anyone
- Use unique passwords for each system
- Report suspected credential compromise immediately

## 8. Physical Security
- Lock your workstation when unattended
- Do not leave sensitive documents on desks (clear desk policy)
- Report lost or stolen devices immediately

## 9. Violations
Violation of this policy may result in disciplinary action up to and including termination.`
  },
  {
    title: "Business Continuity Plan",
    type: "procedure",
    category: "Organizational",
    relatedControls: "A.5.29,A.5.30,A.8.13,A.8.14",
    content: `# Business Continuity Plan

## 1. Purpose
Ensure the organization can continue to operate during and after a disruptive event.

## 2. Scope
All critical business functions and supporting IT systems.

## 3. Business Impact Analysis
- Identify critical business processes
- Determine Recovery Time Objectives (RTOs)
- Determine Recovery Point Objectives (RPOs)
- Identify dependencies between processes

## 4. Continuity Strategies
### 4.1 IT Systems
- Maintain redundant infrastructure
- Implement automated backup solutions
- Test backups regularly
- Maintain disaster recovery site/cloud failover

### 4.2 Personnel
- Cross-train staff for critical functions
- Maintain succession plans
- Enable remote working capability

### 4.3 Facilities
- Identify alternate work locations
- Maintain essential supplies

## 5. Plan Activation
- Activation criteria defined per scenario
- Communication tree for key personnel
- Decision authority for activation

## 6. Testing
- Conduct tabletop exercises annually
- Full simulation test every 2 years
- Review and update plan after each test

## 7. Maintenance
- Review plan at least annually
- Update after significant organizational changes
- Distribute updated copies to key personnel`
  }
];

module.exports = router;
