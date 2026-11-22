# TeamForge on the web

The web version runs **the same TeamForge app** (the C++/Qt Quick code in this repository,
compiled to WebAssembly) inside the browser, plus a small server for what a browser cannot do on
its own: **sign-in** and **data shared between people**.

```
Browser                                            Server (web/server.py, Render)
┌───────────────────────────────┐                  ┌──────────────────────────────────┐
│ /login  sign in / create acct │── POST /api/login ──▶ accounts (PBKDF2 password hashes)│
│ /app/   TeamForge (Qt → Wasm) │── GET /api/bootstrap ▶ shared data files + own session │
│   C++ core: matching, ranking,│◀─────────────────── │ PostgreSQL (Render) or SQLite    │
│   validation, permissions     │── PUT /api/files/… ─▶ versioned; stale uploads refused  │
│ /admin  roles, reset data     │                     │ static files (gzip)              │
└───────────────────────────────┘                  └──────────────────────────────────┘
```

* **Unchanged:** screens, workspaces, branding, sample data, the JSON data format and the matching
  weights. Scoring, ranking and validation still run in the C++ core (now in the browser).
* **Accounts:** email + password. New accounts are participants. Emails listed in `ADMIN_EMAILS`
  become admins. An admin can make anyone a host on the Admin page.

  | Role | TeamForge workspaces |
  |---|---|
  | participant | Participant |
  | host | Participant, Host |
  | admin | Participant, Host, Developer + Admin page |

  The account role only decides which workspace cards are open (checked in C++,
  `TeamForgeController::workspaceAllowed`). Inside a workspace the app's existing permissions apply.
* **Data:** the app's five JSON files and its `.sample-data` stamp are stored on the server
  exactly as the desktop app writes them. Each account also keeps its own `session.json` (last
  workspace, own profile, recent projects), so signing in again brings everything back.
  If two people change the same file, the second upload is refused and that person is asked to
  reload, instead of silently overwriting the first person's work.

What changed in the Qt app (all browser-only, behind `if(EMSCRIPTEN)` / `#ifdef Q_OS_WASM`, so the
Windows build behaves as before):

| File | Change |
|---|---|
| `src/web/WebBridge.h/.cpp` | New. Writes the files from the server into the app's data folder before start; hands every changed file to the page for upload. |
| `src/main.cpp` | Calls the bridge; loads a bundled monospace font. |
| `src/app/TeamForgeController.*` | `workspaceAllowed()`; `enterWorkspace()` refuses workspaces the account may not open (`$TEAMFORGE_ALLOWED_WORKSPACES`; unset = all, as on the desktop). |
| `ui/pages/EntryScreen.qml` | Cards the account may not open are dimmed and do nothing. |
| `ui/Main.qml` | Fills the browser area (`showFullScreen()` on wasm only). |
| `ui/Theme.qml`, developer pages | `Theme.monoFamily`: Consolas on Windows, Noto Sans Mono in the browser. |
| `CMakeLists.txt` | Browser-only sources, font and `-lembind`; tests skipped in the browser build. |
| `assets/fonts/NotoSansMono-Regular.ttf` | New font (SIL Open Font License, `NotoSansMono-OFL.txt`). |
| `tests/tst_controller.cpp` | New test `workspacesLimitedByAccountRole`. |

---

## 1. Build the app for WebAssembly (on your laptop)

Versions must match exactly. The official Qt 6.12 documentation lists **Emscripten 5.0.5** for
**Qt 6.12.0** (https://doc.qt.io/qt-6/wasm.html). Allow about 8–10 GB of free disk space.

1. **Qt:** open *Qt Maintenance Tool* → *Add or remove components* → Qt 6.12.0 → tick
   **WebAssembly (single-threaded)**. Keep the desktop kit (MinGW 64-bit) installed; the WebAssembly
   build needs it for its host tools.
2. **Emscripten** (needs Git and Python), in PowerShell:
   ```powershell
   git clone https://github.com/emscripten-core/emsdk.git C:\emsdk
   cd C:\emsdk
   .\emsdk.bat install 5.0.5
   .\emsdk.bat activate 5.0.5
   ```
3. **Build** — in a new PowerShell window, in the TeamForge folder (replace `C:\Qt` with your Qt
   folder, e.g. `D:\QT`):
   ```powershell
   C:\emsdk\emsdk_env.ps1
   $env:PATH = "C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH
   C:\Qt\6.12.0\wasm_singlethread\bin\qt-cmake.bat -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build-wasm
   ```
   If CMake asks for `QT_HOST_PATH`, add `-DQT_HOST_PATH=C:/Qt/6.12.0/mingw_64` to the
   `qt-cmake.bat` line.
4. **Copy the build into the web folder:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File web\tools\copy-wasm.ps1 -BuildDir build-wasm
   ```
   This copies `TeamForge.js`, `TeamForge.wasm` and `qtloader.js` into `web/static/app/`.
5. **Check the desktop build still works** (your usual build), and run the tests, including
   the new `workspacesLimitedByAccountRole`:
   ```powershell
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

## 1b. Or let GitHub build it (no Qt on your laptop)

`.github/workflows/build-web.yml` does step 1 on GitHub's machines. Push the project to GitHub
(branch `main`); the **Actions** tab shows a run called **Build web app** (about 10–20 minutes the
first time). When it is green, it has committed `TeamForge.js`, `TeamForge.wasm` and `qtloader.js`
into `web/static/app/`, and Render deploys that commit automatically. If it is red, open the run,
click the failed step and copy its log. You can start it again with **Run workflow**.

## 2. Run it locally

Python 3.9 or newer; nothing to install for local use (SQLite is built in).

```powershell
$env:ADMIN_EMAILS = "you@example.com"
python web\server.py
```

Open http://localhost:8000. Create an account with the admin email to see all three workspaces
and the Admin page. Local data is kept in `web/teamforge-local.db` (delete it to start over).

> Open the page through `http://localhost:8000`, never by double-clicking an HTML file:
> WebAssembly cannot load from `file://`.

## 3. Put it on Render

> **Moving to Azure?** `web/AZURE-DEPLOY.md` sets it up on Azure Container Apps with Azure
> PostgreSQL and your own domain (free on Azure for Students for 12 months), and copies
> accounts and data over from Render with `tools/copy_database.py`.

Commit everything **including `web/static/app/TeamForge.wasm`, `TeamForge.js` and `qtloader.js`**
and push to GitHub. (GitHub refuses single files over 100 MB; a Release build of TeamForge should
be far smaller. If it is not, build with `-DCMAKE_BUILD_TYPE=MinSizeRel`.)

**Option A — Blueprint (new service + database in one go)**

1. Render Dashboard → **New → Blueprint** → pick the repository. Render reads `render.yaml`.
2. When asked for `ADMIN_EMAILS`, enter your email (several: comma-separated).
3. Wait for the deploy; open the `onrender.com` URL Render shows, create your account.

**Option B — keep the existing URL (`teamforge-geu.onrender.com`)**

1. **New → Postgres** → plan **Free** → create. Copy its **Internal Database URL**.
2. Open the existing **teamforge-geu** service → **Settings**:
   * Repository / branch: the repository you pushed.
   * Language/runtime **Python 3**; Root directory: *(empty)*.
   * Build command `pip install -r web/requirements.txt && python web/tools/precompress.py`;
     Start command `python web/server.py`.
   * Health check path `/healthz`.
   If Render does not let you change the language of the old service, delete it and create a new
   **Web Service** (plan **Free**) named `teamforge-geu` with these settings.
3. **Environment**: add `DATABASE_URL` = the Internal Database URL, and `ADMIN_EMAILS` = your email.
4. **Manual Deploy → Deploy latest commit.**

**After deploying, check:** `/healthz` shows `ok`; you can create an account, sign out, and sign in
again with the same email and password; a change you make in TeamForge is still there after
reloading and after signing in from another browser.

### Optional: read photos and scans of resumes (Azure AI Document Intelligence)

Text documents (PDF, DOCX, ODT, RTF, text, HTML) are read in the app itself and need nothing
here. To also read **photos and scanned PDFs**, the server passes them to Azure's *Read* model
(`prebuilt-read`, REST API 2024-11-30). Without these settings the app explains that pictures
can't be read on this site, and everything else works as before.

1. Azure portal → **Create a resource** → search **Document Intelligence** → **Create**.
   Pick your subscription (Azure for Students works), a resource group, a region near you, a
   name, and pricing tier **Free F0**. **Review + create.** (F0 costs nothing; a subscription
   can have one F0 resource of this kind. Check the current free page allowance on Azure's
   pricing page.)
2. Open the resource → **Keys and Endpoint**. Copy the **Endpoint** and **KEY 1**.
3. Render → the TeamForge service → **Environment** → add
   `AZURE_DOCINTEL_ENDPOINT` = the endpoint, and `AZURE_DOCINTEL_KEY` = the key.
   The key is a secret: keep it in Render's environment only, never in the repository, a chat or
   a screenshot. If it leaks, use **Regenerate Key 1** in Azure and update Render.
4. **Manual Deploy**. The log no longer says `AZURE_DOCINTEL_ENDPOINT / AZURE_DOCINTEL_KEY are not set`.
5. Check: in onboarding step 01, choose a photo of a printed resume. The card asks before
   sending it; after **Read the text** the usual review opens. A blurred photo is refused on the
   card without being sent.

Limits, all optional environment variables: `TEAMFORGE_READ_MAX_MB` (default 4, F0's limit),
`TEAMFORGE_READS_PER_HOUR` per account (default 10), `TEAMFORGE_READS_PER_MINUTE` for the whole
server (default 15; F0 allows 20). On F0, Azure reads only the first two pages of a PDF.

### Render free plan: what to expect

* The free web service **sleeps after 15 minutes without visitors**; the next visit waits about a
  minute while it starts. Open the site a few minutes before a demo.
* Files on the free web service are wiped on every restart — that is why accounts and data live in
  Render Postgres.
* **Free Render Postgres expires 30 days after it is created** (Render deletes it 14 days later
  unless it is upgraded). Upgrade it, or create a new free database and set the new
  `DATABASE_URL`, before then. A new database starts again from the sample data with no accounts.

## Community: teams, chat, applications, anonymous requests, notifications

The **Community** page (Participant and Host workspaces, plus the bell in the top bar) adds the
people side of TeamForge, with real accounts:

| Feature | What it does |
|---|---|
| Teams | Anyone creates a team and becomes its leader. Leaders make members admins, hand over leadership, remove members, edit or delete the team. |
| Team chat | Private to members. Chronological, older messages on request, new ones arrive live (long polling, below). Unread counts and a "New messages" line. |
| Team applications | Introduction, skills, experience, interests, reason. Leaders and team admins approve (the person joins at once) or reject, with an optional message. Pending, approved, rejected, withdrawn. One pending application per team. |
| Events | The app's projects (requirements.json). A host opens one for applications and becomes its organiser: description, requirements, eligibility, positions, number of places, deadline. Applicants pick a position; once the event is full new applications join the waitlist. Pending, approved, rejected, withdrawn, waitlisted. |
| Anonymous requests | "Message anonymously" on a team (to its leaders), an event (to its organiser) or a person who accepts them. The recipient never sees who sent it; both sides can reply; the recipient marks it reviewed / interested / declined, invites the sender to apply, reports it or blocks the sender. |
| Applications dashboard | "To review" (teams you run, events you organise) and "My applications", with type and status filters, search and pages. |
| Notifications | New applications, decisions, invitations, anonymous messages and replies, team messages (one combined notification per chat), removal, role changes. Unread count, read/unread, links. |
| Moderation | Admin page → *Reported anonymous conversations*: the sender's account is shown there only, with Block or Dismiss. |

How it is built:

* **Server (`web/hub.py`, routes under `/api/hub/`)**: relational tables (`profiles`, `teams`,
  `team_members`, `team_messages`, `team_applications`, `event_settings`, `event_applications`,
  `anon_threads`, `anon_messages`, `anon_reports`, `anon_blocks`, `notifications`), created with
  `CREATE TABLE IF NOT EXISTS` next to the existing ones, so existing data is untouched.
  **Every request is authorised on the server** against the signed-in account (membership for
  chat, leader/team admin for team decisions, organiser or admin for events, recipient or sender
  for anonymous conversations). Unlike the five JSON files, nothing here relies on the app's own
  checks.
* **Safety**: duplicate active applications are refused (checked inside a transaction, and by a
  partial unique index). Decisions re-read the status inside a transaction and update only
  `WHERE status = <expected>`, so two leaders approving at once give one approval and one refusal,
  and approvals can never exceed an event's places. Text is cleaned and limited; profile pictures
  are checked by their bytes (PNG/JPEG/WebP, at most 80 KB). Rate limits per account (chat 20 per
  minute, applications 10 per hour, new anonymous conversations 5 per hour and 10 per day, one open
  conversation per recipient). The in-memory limits reset when the server restarts; the daily
  anonymous limit is kept in the database.
* **Anonymity**: anonymous conversations have random ids, messages carry only a "from the sender"
  flag, and recipient answers are built from explicit fields, never from the database row; the
  tests check that no email, name or account id reaches the recipient. The sender's account is
  stored (for the sender's own "Sent" list, limits, blocks and moderation) and the form says so.
* **Live chat**: `GET /api/hub/teams/<id>/messages?after=<id>&wait=25` waits until a new message
  arrives or 25 s pass (long polling with the standard library; no WebSocket server needed). It
  works with one server process, as now; several processes would need a shared notifier.
* **App (C++)**: rules in `src/core/Hub.h` (statuses and allowed moves, capacity and waitlist,
  team roles, text cleaning, the chat log as an id-ordered structure, notifications), tested by
  `tests/tst_hub`. `src/app/HubClient.h` is the QML singleton `Hub` and the chat model. The browser
  build sends requests through the page (`window.TF_hub` in `app.js`, with the page's sign-in
  cookie); the desktop build uses Qt Network and signs in on the Community page with an account
  from the web page. Screens: `ui/community/`.

Try it locally with demo data (five accounts, teams, chat, applications, an open event,
anonymous requests; the shared demo password is printed):

```powershell
$env:PORT = "8000"; $env:ADMIN_EMAILS = "dev@demo.test"; python web\server.py
python web	ools\demo_hub_data.py http://localhost:8000
```

Tests: `python web/tests/test_hub.py` (106 API checks: every endpoint, permissions, duplicates,
deadlines, capacity under simultaneous approvals, anonymity, rate limits, restart);
`tests/tst_hub` (core rules); `tests/tst_community` (the screens offscreen against a demo server:
set `TEAMFORGE_TEST_SERVER`, optionally `TEAMFORGE_SHOTS` for screenshots);
`node web/tests/test_appjs.mjs` (also `TF_hub`).

## Data and security notes

* Passwords are stored only as PBKDF2-SHA256 hashes with a random salt. Sessions are random tokens
  in an HttpOnly cookie (Secure on HTTPS), valid 30 days. Repeated failed sign-ins are slowed down.
* The server accepts each role's uploads only for the files that role's workspaces can change
  (participant: profiles and interest requests; host: also projects and teams; admin: everything).
* The rules *inside* the five JSON files (for example "a participant edits only their own
  profile") are enforced by the C++ app in the browser, as on the desktop. (The Community's data
  is different: the server checks every request, see above.) Someone who calls the API by hand
  could bypass them for the files their role may change. This is acceptable for a class demo; a
  production system would move those checks to the server.
* Admin → **Reset shared data** reloads the sample data from `data/` (for example after running
  `tools/generate_sample_data.py`). Accounts are kept.
* Photos and scans of resumes (`POST /api/read-document`, signed-in accounts only, same-origin
  writes only): the picture is passed to Azure and the text returned to the app; neither is
  stored or logged (the log has the file type, size, page and word counts). The server deletes
  Azure's copy of the result right after reading it (Azure would otherwise keep it 24 hours).
  The Azure key is sent only to the configured endpoint, never to an address in a response.

## Files

```
web/
  server.py              accounts, data API, static files (Python standard library + psycopg)
  hub.py                 Community: teams, chat, applications, anonymous requests, notifications
  requirements.txt       psycopg (only for PostgreSQL)
  static/login.html      sign-in / create account (browser can save the password)
  static/admin.html      roles, reported anonymous conversations, data reset (admins)
  static/app/index.html  loads TeamForge; app.js = data sync; app.css
  static/app/TeamForge.{js,wasm}, qtloader.js   ← copied from build-wasm/
  static/css, js, fonts, img
  tools/copy-wasm.ps1 / copy-wasm.sh
  tools/precompress.py   gzip copies of the app files (run at build time)
  tools/copy_database.py copies accounts and data between databases (Render -> Azure)
  tools/demo_hub_data.py demo Community data for a local server
  AZURE-DEPLOY.md        Azure Container Apps + PostgreSQL + own domain, step by step
render.yaml              Render Blueprint (web service + Postgres, both free)
Dockerfile, .dockerignore  the server image (Azure Container Apps; any Docker host)
../.github/workflows/deploy-azure.yml  builds the image to ghcr.io and rolls it out to Azure
```
