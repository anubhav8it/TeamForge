# TeamForge Web (C++ backend)

The TeamForge website with a C++17 backend. The browser frontend (`static/`) is the same as in the Python version. It talks to the server only through `/api/...` requests, so it didn't need any changes.

## What's inside

```
src/
  main.cpp       starts the web server (the only file that uses cpp-httplib)
  cli.hpp        commands: seed, create-admin, serve
  api.cpp/.hpp   every API route: login, roles, profiles, projects, matching, teams, admin
  matching.cpp   match %, "Why this match?", team coverage, Suggest team (pure C++)
  db.cpp/.hpp    small RAII wrapper around SQLite
  json.cpp/.hpp  small JSON parser/serializer
  crypto.cpp     SHA-256 + PBKDF2 password hashing (no OpenSSL needed)
static/          the website (HTML/CSS/JS)
data/            sample data from the desktop build
tests/           test_core.cpp (JSON, crypto, matching), test_api.cpp (all routes)
third_party/     download scripts for cpp-httplib and SQLite
devtools/        sandbox-only test helpers; not part of the real build
```

Libraries used:
- **cpp-httplib**: HTTP server, one header file, MIT licence.
- **SQLite**: database, one C file, public domain.

Everything else is standard C++17.

## Build and run on Windows

You already have a C++ compiler and CMake from your Qt install. Use the **"Qt 6.x (MinGW 64-bit)"** command prompt from the Start menu so they are on PATH. If `cmake` isn't found, add `C:\Qt\Tools\CMake_64\bin` to PATH.

```bat
cd teamforge-cpp
powershell -ExecutionPolicy Bypass -File third_party\get-deps.ps1

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

build\teamforge seed                               :: loads data\*.json into teamforge.db
build\teamforge create-admin you@example.com       :: asks for a password (8+ characters)
build\teamforge serve
```

Open http://localhost:8080 and sign in with the admin account.
To open it from your phone on the same Wi-Fi, use `build\teamforge serve --host 0.0.0.0` and go to `http://<your-PC's-IP>:8080`. Windows Firewall will ask for permission.

You can also open `CMakeLists.txt` directly in **Qt Creator** (File > Open File or Project) and build it like any other project. Set the run arguments to `serve` and the working directory to the project folder.

On MinGW the server is linked statically, so `teamforge.exe` runs on any Windows 10/11 PC without extra DLLs.

**Tests:** `ctest --test-dir build --output-on-failure`

**Linux/macOS:** run `sh third_party/get-deps.sh`, then the same cmake commands without `-G "MinGW Makefiles"`.

## Commands and settings

| Command | What it does |
|---|---|
| `teamforge seed` | Imports the sample data (safe to run again; it skips existing rows) |
| `teamforge seed --reset` | Wipes everything except user accounts, then re-imports |
| `teamforge create-admin EMAIL [--password PW]` | Creates an admin, or promotes an existing account |
| `teamforge serve [--host H] [--port P]` | Starts the website (default 127.0.0.1:8080) |

Options for every command:
- `--db FILE` (default `teamforge.db`, or the `DATABASE` environment variable)
- `--data DIR`
- `--static DIR`

Set `COOKIE_SECURE=1` when the site runs behind HTTPS.

## Roles

New accounts are **participants**. An admin can make someone a **host** on the Overview page.

- Hosts manage the projects they create.
- **Admins** manage every project, the skill catalogue, and accounts. This role replaces the desktop app's Developer workspace.

Roles are checked on the server for every request.

## How matching works (change it here)

In `src/matching.cpp`:
- For each required skill with minimum level R, a person with level L earns `min(L, R)`.
- Match % = earned ÷ total R.
- **Suggest team** is greedy. It adds whoever closes the most remaining skill gap. It stops when every skill is covered and the minimum size is reached, or when the maximum size is reached.

This is a simple rule, not the one in your desktop app. To use your own SkillIndex / SkillBST / SkillGraph code, put it in `matching.cpp` behind the same functions (`score`, `explain`, `coverage`, `suggest_team`). The API and tests will use it automatically. Run the tests afterwards.

## Putting it online

A C++ server must be built for the server's operating system, which is usually Linux. The simplest route is the included **Dockerfile**: it downloads the libraries, builds, runs the tests, and starts the server.

1. Push the folder to GitHub. `.gitignore` keeps the database and the downloaded libraries out.
2. Deploy it on a host that runs Docker images, or on a Linux VPS with Docker.
3. Attach a **persistent volume at `/data`**. Without it, the database is erased on every redeploy.
4. Set environment variables:
   - `COOKIE_SECURE=1`
   - `ADMIN_EMAIL` and `ADMIN_PASSWORD` for the first admin. Remove `ADMIN_PASSWORD` after the first start.

Check each host's current limits and prices; they change often.

## Known limits

- **One request at a time.** All requests share one SQLite connection behind a mutex. This is simple and safe, and fine for a college-sized app.
- **Sign-in is slow by design.** It takes about 0.6 s because passwords are hashed 600,000 times (PBKDF2). During that time other requests wait.
- **Not built yet:** sign-in rate limiting, password reset, email verification, and automatic backups. Add these before real students use it.
- **Left out on purpose:** the desktop app's 3D logo, SkillBST/SkillGraph tools, raw-data view, and a "reset data" button. The reset is a server command instead.
