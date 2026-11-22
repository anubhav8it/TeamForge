# TeamForge live platform: status and checklist

Progress log for turning TeamForge into a live platform with real users (8 blocks).
Each block is marked complete only after it has been tested. Newest findings first.

## Block 1: Azure audit (2026-11-07)

### Current architecture

| Layer | What it is | Where |
|---|---|---|
| App | Qt 6 QML/C++, compiled to WebAssembly by GitHub Actions (`build-web.yml`) | `Phase-2/src`, `Phase-2/ui` |
| Core logic (C++) | matching engine (weighted strategy, ranker, skill BST/graph/index), resume analyser, validation | `Phase-2/src/core` |
| Server | Python standard-library HTTP server: accounts (PBKDF2), cookie sessions (hashed tokens), roles, data sync, optional Azure OCR | `Phase-2/web/server.py` |
| Database | PostgreSQL via `DATABASE_URL` (psycopg), SQLite locally. 4 tables: `users`, `sessions`, `shared_files`, `user_files` | `server.py`, `Database` |
| Domain data | five JSON documents stored whole in `shared_files` (students, requirements, teams, skills, interest requests) | `Phase-2/data` (seed) |
| Hosting now | Render free web service + free PostgreSQL | `teamforge-geu.onrender.com` |
| Azure path | Container Apps + PostgreSQL Flexible Server B1ms, image from ghcr.io | `Dockerfile`, `deploy-azure.yml`, `web/AZURE-DEPLOY.md` |

### Azure status

| Component | Status (verified how) | Required action |
|---|---|---|
| Azure resources | **Checked with Azure CLI (2026-11-07).** Subscription *Azure for Students*, enabled. Resource group `teamforge` (Central India) exists and is **empty**. On 2026-10-31 creating PostgreSQL `teamforge-db-mukul` failed 4 times: the subscription policy *Allowed resource deployment regions* permits only Korea Central, UAE North, Poland Central, East Asia and India South Central. Central India is blocked. | Create resources in an allowed region (the group's own region does not matter) |
| Container image | **Done.** `ghcr.io/anubhav8it/teamforge-web:latest` and `:sha-663efdf592f1`, public (anonymous pull checked) | none |
| Database (Azure) | Not created (blocked by the region policy). B1ms is offered in all five allowed regions | create B1ms server + `teamforge` database (guide step 2) |
| Database (Render) | Works today (a sign-in with an unknown email returns 401, so the database answered). Render's free database lasts 30 days; it was added around 2026-10-10, so it **expires around 2026-11-09** | copy to Azure before then (guide step 3); check the exact date in Render's dashboard |
| Hosting | Container Apps chosen (scales to zero, monthly free allowance); App Service is not used. Container Apps runs in 4 of the 5 allowed regions (not India South Central). Providers `Microsoft.App` and `Microsoft.OperationalInsights` are not registered yet | create the Container App (guide step 6) |
| Blob Storage | Not in the architecture. Resumes are never stored (by design); there are no profile pictures yet | decision: PostgreSQL vs Blob (below) |
| Key Vault | Not used. Secrets live in the host's settings (Render env, Container Apps secrets); none are in the repository (history scanned) | decision (below) |
| Document Intelligence | Code done and tested against a fake Azure service; a real call not verified; resource existence unknown | optional (guide step 4) |
| Health checks | **Fixed now.** `/healthz` never touched the database, so a deploy with a broken `DATABASE_URL` passed. Added `/readyz` (runs `SELECT 1`); the deploy workflow now waits for it | done (76/76 server tests) |
| Automatic deploys | Image builds automatically; roll-out is manual | optional (guide step 8) |

### Problems found that affect later blocks

1. **The server trusts whole-file uploads.** A participant may upload all of `students.json` and
   `interest_requests.json`, so a modified client could change other people's profiles or
   applications. Permissions are only checked in the C++ app. This must be fixed before real
   users join (Blocks 2 and 5): server-side per-record checks or relational tables.
2. **Accounts and profiles are not linked.** `users` holds emails; profiles are records in
   `students.json` with their own ids. "Only registered people are shown" needs that link.
3. **An empty database is filled with sample data** (560 students, 8 projects, 86 interest
   requests) on first start. Block 2 changes this, after a backup and your confirmation.
4. **The database allows connections from any Azure service** (the free-tier setup). The strong
   password is the guard. Private networking would cost money.
5. **One database connection behind one lock**: fine at class scale, a limit for heavy use.
6. **"AI" matching today is deterministic weighted scoring in C++**, with no language model.
   Whether to add one (Azure OpenAI, paid) is a Block 4 decision.

### Checklist

- [x] Audit repository, server, database, deploy files, git history
- [x] Verify image on ghcr.io, last deploy run, Render health
- [x] Add `/readyz` (database readiness) + tests; deploy workflow and guide use it
- [x] Confirm which Azure resources exist (empty group; region policy found)
- [ ] Owner approval: region and resource plan
- [ ] Owner-approved: create PostgreSQL B1ms + database
- [ ] Copy Render data to Azure (before ~2026-11-09)
- [ ] Owner-approved: create Container App, set secrets, start
- [ ] Verify on Azure: `/readyz`, sign-in, save + reload, resume upload
- [ ] File storage decision implemented and tested
- [ ] Key Vault decision implemented
- [ ] Block 1 completion report

## Community: teams, messaging, applications (branch `team-hub`, 2026-11-22)

Built from the "Team Forge feature expansion" prompt; covers most of Blocks 5 and 6 below. Details:
`Phase-2/web/README-WEB.md`, section *Community*.

- [x] Server `web/hub.py`: 12 relational tables, `/api/hub/` routes, every request authorised on
      the server (this fixes problem 1 above for the new features; the five JSON files are unchanged)
- [x] Teams with leader / admin / member roles; private team chat with pages, unread counts, live
      updates (long polling)
- [x] Team join applications; event applications (organiser, deadline, places, waitlist)
- [x] Anonymous connection requests with replies, status, invite to apply, report, block; admin
      moderation on the Admin page
- [x] Notifications with unread counts and links; applications dashboard with filters and pages
- [x] C++ rules `src/core/Hub.h` + `tests/tst_hub`; QML screens `ui/community/`; `HubClient`
      (browser: `TF_hub` in app.js; desktop: Qt Network)
- [x] Tests: test_hub.py 106/106, test_server.py 76/76, copy_database 10/10, test_appjs 15/15,
      ctest 11/11 (incl. tst_community against a demo server)
- [ ] Not verified: the WebAssembly build (CI only; the browser transport mirrors the existing
      `TF_readDocument` pattern) and a run on Render/Azure
- [ ] Open: accounts are still not linked to the matching profiles (problem 2); events = projects,
      so approvals do not change interest_requests.json; rate limits reset on restart

## Blocks 2 to 8

Not started. Order: database reset and live authentication; profiles and portfolios; discovery
and matching; teams and invitation links; team messaging; comparing team offers; full testing.
