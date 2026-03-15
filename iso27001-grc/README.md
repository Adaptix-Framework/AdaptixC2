# ISO 27001:2022 GRC Platform

A full-stack web application to help organizations comply with ISO 27001:2022 and prepare for external certification audits.

## Features

- **ISMS Clauses Tracker (4-10)**: Track compliance with all mandatory ISO 27001:2022 clauses with detailed explanations and guidance
- **Annex A Controls (93 controls)**: Manage all 93 Annex A controls across 4 categories (Organizational, People, Physical, Technological)
- **Statement of Applicability (SoA)**: Design and maintain your SoA with applicability decisions and justifications
- **Policy/Procedure/Guideline Management**: Create, review, and approve security documentation with built-in templates
- **Risk Register**: Full risk assessment with 5x5 risk matrix, treatment plans, and risk tracking
- **Audit Readiness Dashboard**: Real-time compliance scoring, gap analysis, and preparation checklist
- **Compliance Measurement**: Track overall compliance percentage and identify remaining work

## Tech Stack

- **Frontend**: React 18 + Vite + React Router
- **Backend**: Node.js + Express + Prisma ORM
- **Database**: PostgreSQL
- **Auth**: JWT-based authentication

## Quick Start

### Prerequisites
- Node.js 18+
- PostgreSQL 14+

### Setup

1. **Database**: Create a PostgreSQL database:
   ```sql
   CREATE DATABASE iso27001_grc;
   ```

2. **Backend**:
   ```bash
   cd backend
   cp .env.example .env
   # Edit .env with your database credentials
   npm install
   npx prisma migrate dev --name init
   npm run db:seed
   npm run dev
   ```

3. **Frontend**:
   ```bash
   cd frontend
   npm install
   npm run dev
   ```

4. Open http://localhost:3000 and register a new account.

### Docker (Alternative)
```bash
docker-compose up -d
```

## ISO 27001:2022 Coverage

### ISMS Requirements (Clauses 4-10)
- Clause 4: Context of the Organization
- Clause 5: Leadership
- Clause 6: Planning
- Clause 7: Support
- Clause 8: Operation
- Clause 9: Performance Evaluation
- Clause 10: Improvement

### Annex A Controls
- A.5: Organizational Controls (37 controls)
- A.6: People Controls (8 controls)
- A.7: Physical Controls (14 controls)
- A.8: Technological Controls (34 controls)

## License

MIT
