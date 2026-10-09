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

### Render free plan: what to expect

* The free web service **sleeps after 15 minutes without visitors**; the next visit waits about a
  minute while it starts. Open the site a few minutes before a demo.
* Files on the free web service are wiped on every restart — that is why accounts and data live in
  Render Postgres.
* **Free Render Postgres expires 30 days after it is created** (Render deletes it 14 days later
  unless it is upgraded). Upgrade it, or create a new free database and set the new
  `DATABASE_URL`, before then. A new database starts again from the sample data with no accounts.

## Data and security notes

* Passwords are stored only as PBKDF2-SHA256 hashes with a random salt. Sessions are random tokens
  in an HttpOnly cookie (Secure on HTTPS), valid 30 days. Repeated failed sign-ins are slowed down.
* The server accepts each role's uploads only for the files that role's workspaces can change
  (participant: profiles and interest requests; host: also projects and teams; admin: everything).
* The rules *inside* the data (for example "a participant edits only their own profile") are
  enforced by the C++ app in the browser, as on the desktop. Someone who calls the API by hand
  could bypass them for the files their role may change. This is acceptable for a class demo; a
  production system would move those checks to the server.
* Admin → **Reset shared data** reloads the sample data from `data/` (for example after running
  `tools/generate_sample_data.py`). Accounts are kept.

## Files

```
web/
  server.py              accounts, data API, static files (Python standard library + psycopg)
  requirements.txt       psycopg (only for PostgreSQL)
  static/login.html      sign-in / create account (browser can save the password)
  static/admin.html      roles and data reset (admins)
  static/app/index.html  loads TeamForge; app.js = data sync; app.css
  static/app/TeamForge.{js,wasm}, qtloader.js   ← copied from build-wasm/
  static/css, js, fonts, img
  tools/copy-wasm.ps1 / copy-wasm.sh
render.yaml              Render Blueprint (web service + Postgres, both free)
```
