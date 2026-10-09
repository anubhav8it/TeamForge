#!/usr/bin/env python3
"""TeamForge web server.

Serves the TeamForge Qt for WebAssembly app and gives it what a browser cannot: accounts and
data shared between people.

* Accounts: email + password (PBKDF2-hashed), roles participant / host / admin. New accounts are
  participants; emails listed in ADMIN_EMAILS become admins; an admin can make anyone a host.
  The role decides which TeamForge workspaces the app opens (admin = all three, including
  Developer). The workspaces' own permissions are still enforced by the C++ app.
* Shared data: the app's five JSON files (students, requirements, teams, skills, interest
  requests) plus the seed stamp, stored unchanged so the C++ core reads them exactly as on the
  desktop. Every file has a version; an upload based on an older version is refused (HTTP 409)
  instead of silently overwriting someone else's change.
* Per-account session: each account keeps its own session.json (last workspace, own profile,
  recent projects).
* Storage: PostgreSQL when DATABASE_URL is set (Render), otherwise a local SQLite file.

Standard library only, except the PostgreSQL driver (psycopg), which is needed only with
DATABASE_URL. Run locally:  python web/server.py   then open http://localhost:8000
"""

from __future__ import annotations

import base64
import gzip
import hashlib
import hmac
import json
import logging
import mimetypes
import os
import re
import secrets
import sqlite3
import threading
import time
from http import HTTPStatus
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

# --------------------------------------------------------------------------- configuration

WEB_DIR = Path(__file__).resolve().parent
STATIC_DIR = WEB_DIR / "static"
SEED_DIR = Path(os.environ.get("TEAMFORGE_SEED_DIR", WEB_DIR.parent / "data")).resolve()
DATABASE_URL = os.environ.get("DATABASE_URL", "").strip()
SQLITE_PATH = Path(os.environ.get("TEAMFORGE_SQLITE", WEB_DIR / "teamforge-local.db"))
ADMIN_EMAILS = {e.strip().lower() for e in os.environ.get("ADMIN_EMAILS", "").split(",") if e.strip()}
PORT = int(os.environ.get("PORT", "8000"))
HOST = os.environ.get("HOST", "0.0.0.0")

# The app's data files (src/app/SampleData.h, sampledata::files()) and its seed stamp.
SHARED_FILES = ["students.json", "requirements.json", "teams.json", "skills.json", "interest_requests.json"]
STAMP_FILE = ".sample-data"
SESSION_FILE = "session.json"
ALL_SHARED = SHARED_FILES + [STAMP_FILE]

ROLES = ("participant", "host", "admin")
# Workspaces each role may open in the app (TEAMFORGE_ALLOWED_WORKSPACES).
WORKSPACES = {
    "participant": "participant",
    "host": "participant,host",
    "admin": "participant,host,developer",
}
# Files each role's workspaces can change (mirrors the C++ permissions: participants edit their
# profile and express interest; hosts also manage projects, teams and applicants; only the
# Developer workspace edits the skill catalogue or resets the data). Uploads of other files are
# ignored, which also covers the app re-saving a file it did not change.
WRITABLE = {
    "participant": {"students.json", "interest_requests.json"},
    "host": {"students.json", "interest_requests.json", "requirements.json", "teams.json"},
    "admin": set(ALL_SHARED),
}

SESSION_DAYS = 30
PBKDF2_ITERATIONS = 200_000
MAX_FILE_BYTES = 5 * 1024 * 1024
MAX_BODY_BYTES = MAX_FILE_BYTES + 64 * 1024
EMAIL_RE = re.compile(r"^[^@\s]{1,64}@[^@\s]+\.[^@\s]{2,}$")

log = logging.getLogger("teamforge")


# --------------------------------------------------------------------------- helpers

def now() -> float:
    return time.time()


def hash_password(password: str) -> str:
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, PBKDF2_ITERATIONS)
    return "pbkdf2_sha256${}${}${}".format(
        PBKDF2_ITERATIONS, base64.b64encode(salt).decode(), base64.b64encode(digest).decode())


def verify_password(password: str, stored: str) -> bool:
    try:
        scheme, iterations, salt, digest = stored.split("$")
        if scheme != "pbkdf2_sha256":
            return False
        expected = base64.b64decode(digest)
        actual = hashlib.pbkdf2_hmac("sha256", password.encode(), base64.b64decode(salt), int(iterations))
        return hmac.compare_digest(actual, expected)
    except (ValueError, TypeError):
        return False


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode()).hexdigest()


def seed_stamp(seed_dir: Path) -> str:
    """The same SHA-256 the app writes to .sample-data (src/app/SampleData.cpp, sampleHash)."""
    combined = b""
    for name in SHARED_FILES:
        combined += name.encode() + b":" + hashlib.sha256((seed_dir / name).read_bytes()).hexdigest().encode() + b";"
    return hashlib.sha256(combined).hexdigest()


def _canonical(value):
    if isinstance(value, dict):
        return {k: _canonical(v) for k, v in value.items()}
    if isinstance(value, list):
        items = [_canonical(v) for v in value]
        return sorted(items, key=lambda v: json.dumps(v, sort_keys=True))
    return value


def canonical(text: str) -> str:
    """Content key that ignores formatting, record order and the top-level "note"."""
    try:
        value = json.loads(text)
    except (ValueError, TypeError):
        return (text or "").strip()
    if isinstance(value, dict):
        value.pop("note", None)
    return json.dumps(_canonical(value), sort_keys=True)


def valid_file_content(name: str, content: str) -> str | None:
    """Returns an error message, or None when the upload looks like the app's own file."""
    if not isinstance(content, str):
        return "content must be text"
    if len(content.encode()) > MAX_FILE_BYTES:
        return "file too large"
    if name == STAMP_FILE:
        return None if re.fullmatch(r"[0-9a-f]{64}\s*", content) else "invalid seed stamp"
    try:
        value = json.loads(content)
    except ValueError:
        return "not valid JSON"
    if not isinstance(value, dict) or "version" not in value:
        return "not a TeamForge data file"
    return None


# --------------------------------------------------------------------------- database

class Database:
    """A small wrapper: SQLite locally, PostgreSQL (psycopg 3) when DATABASE_URL is set.

    Queries use "?" placeholders; they are translated for PostgreSQL. One connection, guarded by
    a lock, is plenty for a class demo and keeps version checks atomic.
    """

    def __init__(self, url: str, sqlite_path: Path):
        self.lock = threading.RLock()
        self.url = url
        self.is_pg = url.startswith(("postgres://", "postgresql://"))
        self.sqlite_path = sqlite_path
        self.conn = None
        self._connect()
        self._create_schema()

    def _connect(self):
        if self.is_pg:
            import psycopg  # only needed on Render (requirements.txt)
            self.conn = psycopg.connect(self.url, autocommit=True)
        else:
            self.conn = sqlite3.connect(str(self.sqlite_path), check_same_thread=False, isolation_level=None)
            self.conn.execute("PRAGMA journal_mode=WAL")
            self.conn.execute("PRAGMA foreign_keys=ON")

    def _sql(self, sql: str) -> str:
        return sql.replace("?", "%s") if self.is_pg else sql

    def _run(self, sql, params=(), fetch=None):
        with self.lock:
            for attempt in (1, 2):
                try:
                    cur = self.conn.cursor()
                    cur.execute(self._sql(sql), params)
                    if fetch == "one":
                        row = cur.fetchone()
                    elif fetch == "all":
                        row = cur.fetchall()
                    else:
                        row = cur.rowcount
                    cur.close()
                    return row
                except Exception as error:  # reconnect once if PostgreSQL dropped the connection
                    if self.is_pg and attempt == 1 and self._is_disconnect(error):
                        log.warning("database connection lost; reconnecting")
                        self._connect()
                        continue
                    raise

    @staticmethod
    def _is_disconnect(error) -> bool:
        return type(error).__name__ in ("OperationalError", "InterfaceError")

    def one(self, sql, params=()):
        return self._run(sql, params, "one")

    def all(self, sql, params=()):
        return self._run(sql, params, "all")

    def execute(self, sql, params=()) -> int:
        return self._run(sql, params)

    def _create_schema(self):
        user_id = "SERIAL PRIMARY KEY" if self.is_pg else "INTEGER PRIMARY KEY AUTOINCREMENT"
        real = "DOUBLE PRECISION" if self.is_pg else "REAL"
        statements = [
            f"""CREATE TABLE IF NOT EXISTS users (
                id {user_id}, email TEXT NOT NULL UNIQUE, password_hash TEXT NOT NULL,
                role TEXT NOT NULL, created_at {real} NOT NULL, last_login {real})""",
            f"""CREATE TABLE IF NOT EXISTS sessions (
                token_hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                expires_at {real} NOT NULL)""",
            f"""CREATE TABLE IF NOT EXISTS shared_files (
                name TEXT PRIMARY KEY, content TEXT NOT NULL, version INTEGER NOT NULL,
                updated_at {real} NOT NULL, updated_by TEXT)""",
            f"""CREATE TABLE IF NOT EXISTS user_files (
                user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, name TEXT NOT NULL,
                content TEXT NOT NULL, version INTEGER NOT NULL, updated_at {real} NOT NULL,
                PRIMARY KEY (user_id, name))""",
        ]
        for sql in statements:
            self.execute(sql)


# --------------------------------------------------------------------------- application logic

class TeamForgeStore:
    def __init__(self, db: Database, seed_dir: Path):
        self.db = db
        self.seed_dir = seed_dir

    # ---- shared data
    def seed_if_empty(self):
        row = self.db.one("SELECT COUNT(*) FROM shared_files")
        if row and row[0] > 0:
            current = self.db.one("SELECT content FROM shared_files WHERE name = ?", (STAMP_FILE,))
            if current and self.seed_dir.is_dir() and current[0].strip() != seed_stamp(self.seed_dir):
                log.warning("data/ in the repository differs from the stored shared data. "
                            "Use Admin > Reset shared data to load the new sample data.")
            return False
        self.reset_shared("server")
        return True

    def reset_shared(self, who: str):
        if not all((self.seed_dir / n).is_file() for n in SHARED_FILES):
            raise RuntimeError(f"seed data not found in {self.seed_dir}")
        contents = {n: (self.seed_dir / n).read_text(encoding="utf-8") for n in SHARED_FILES}
        contents[STAMP_FILE] = seed_stamp(self.seed_dir) + "\n"  # exactly what the app writes
        with self.db.lock:
            for name, content in contents.items():
                self.db.execute(
                    """INSERT INTO shared_files (name, content, version, updated_at, updated_by)
                       VALUES (?, ?, 1, ?, ?)
                       ON CONFLICT (name) DO UPDATE SET content = excluded.content,
                           version = shared_files.version + 1, updated_at = excluded.updated_at,
                           updated_by = excluded.updated_by""",
                    (name, content, now(), who))
        log.info("shared data seeded from %s", self.seed_dir)

    def shared_files(self):
        rows = self.db.all("SELECT name, content, version FROM shared_files")
        return {name: (content, version) for name, content, version in rows}

    def versions(self):
        rows = self.db.all("SELECT name, version FROM shared_files")
        return {name: version for name, version in rows}

    def save_shared(self, user, name, content, base_version):
        """Returns (status, version): saved / unchanged / ignored / conflict."""
        with self.db.lock:
            row = self.db.one("SELECT content, version FROM shared_files WHERE name = ?", (name,))
            if row and canonical(row[0]) == canonical(content):
                return "unchanged", row[1]
            if name not in WRITABLE[user["role"]]:
                log.info("ignored %s from %s (%s): not writable for this role", name, user["email"], user["role"])
                return "ignored", row[1] if row else 0
            if row is None:
                self.db.execute(
                    "INSERT INTO shared_files (name, content, version, updated_at, updated_by) VALUES (?, ?, 1, ?, ?)",
                    (name, content, now(), user["email"]))
                return "saved", 1
            if base_version != row[1]:
                return "conflict", row[1]
            updated = self.db.execute(
                """UPDATE shared_files SET content = ?, version = version + 1, updated_at = ?, updated_by = ?
                   WHERE name = ? AND version = ?""",
                (content, now(), user["email"], name, base_version))
            if updated != 1:
                current = self.db.one("SELECT version FROM shared_files WHERE name = ?", (name,))
                return "conflict", current[0] if current else 0
            return "saved", base_version + 1

    # ---- per-account files (session.json)
    def user_file(self, user_id, name):
        return self.db.one("SELECT content, version FROM user_files WHERE user_id = ? AND name = ?", (user_id, name))

    def save_user_file(self, user_id, name, content):
        with self.db.lock:
            self.db.execute(
                """INSERT INTO user_files (user_id, name, content, version, updated_at) VALUES (?, ?, ?, 1, ?)
                   ON CONFLICT (user_id, name) DO UPDATE SET content = excluded.content,
                       version = user_files.version + 1, updated_at = excluded.updated_at""",
                (user_id, name, content, now()))
            row = self.user_file(user_id, name)
            return row[1] if row else 1

    # ---- accounts
    def find_user_by_email(self, email):
        row = self.db.one("SELECT id, email, password_hash, role FROM users WHERE email = ?", (email,))
        return None if row is None else {"id": row[0], "email": row[1], "password_hash": row[2], "role": row[3]}

    def create_user(self, email, password):
        role = "admin" if email in ADMIN_EMAILS else "participant"
        self.db.execute("INSERT INTO users (email, password_hash, role, created_at) VALUES (?, ?, ?, ?)",
                        (email, hash_password(password), role, now()))
        return self.find_user_by_email(email)

    def create_session(self, user_id) -> str:
        token = secrets.token_urlsafe(32)
        self.db.execute("INSERT INTO sessions (token_hash, user_id, expires_at) VALUES (?, ?, ?)",
                        (token_hash(token), user_id, now() + SESSION_DAYS * 86400))
        self.db.execute("UPDATE users SET last_login = ? WHERE id = ?", (now(), user_id))
        if secrets.randbelow(20) == 0:
            self.db.execute("DELETE FROM sessions WHERE expires_at < ?", (now(),))
        return token

    def user_for_token(self, token):
        if not token:
            return None
        row = self.db.one(
            """SELECT u.id, u.email, u.role FROM sessions s JOIN users u ON u.id = s.user_id
               WHERE s.token_hash = ? AND s.expires_at > ?""",
            (token_hash(token), now()))
        if row is None:
            return None
        user = {"id": row[0], "email": row[1], "role": row[2]}
        if user["email"] in ADMIN_EMAILS and user["role"] != "admin":
            self.db.execute("UPDATE users SET role = 'admin' WHERE id = ?", (user["id"],))
            user["role"] = "admin"
        return user

    def delete_session(self, token):
        if token:
            self.db.execute("DELETE FROM sessions WHERE token_hash = ?", (token_hash(token),))

    def list_users(self):
        rows = self.db.all("SELECT id, email, role, created_at, last_login FROM users ORDER BY created_at")
        return [{"id": r[0], "email": r[1], "role": r[2], "createdAt": r[3], "lastLogin": r[4]} for r in rows]

    def set_role(self, user_id, role):
        with self.db.lock:
            target = self.db.one("SELECT role FROM users WHERE id = ?", (user_id,))
            if target is None:
                return "not_found"
            if target[0] == "admin" and role != "admin":
                admins = self.db.one("SELECT COUNT(*) FROM users WHERE role = 'admin'")[0]
                if admins <= 1:
                    return "last_admin"
            self.db.execute("UPDATE users SET role = ? WHERE id = ?", (role, user_id))
            return "ok"


# --------------------------------------------------------------------------- login throttling

class Throttle:
    """At most LIMIT failed sign-ins per (IP, email) in WINDOW seconds."""
    LIMIT, WINDOW = 8, 600

    def __init__(self):
        self.lock = threading.Lock()
        self.failures: dict[tuple[str, str], list[float]] = {}

    def blocked(self, key) -> bool:
        with self.lock:
            recent = [t for t in self.failures.get(key, []) if t > now() - self.WINDOW]
            self.failures[key] = recent
            return len(recent) >= self.LIMIT

    def fail(self, key):
        with self.lock:
            self.failures.setdefault(key, []).append(now())

    def clear(self, key):
        with self.lock:
            self.failures.pop(key, None)


# --------------------------------------------------------------------------- static files

COMPRESSIBLE = {".wasm", ".js", ".mjs", ".css", ".html", ".json", ".svg", ".txt"}
mimetypes.add_type("application/wasm", ".wasm")
mimetypes.add_type("text/javascript", ".js")
mimetypes.add_type("text/javascript", ".mjs")
mimetypes.add_type("font/ttf", ".ttf")
mimetypes.add_type("image/svg+xml", ".svg")


class GzipCache:
    """Compressed copies of static files, made once per file version (the .wasm is large)."""

    def __init__(self):
        self.lock = threading.Lock()
        self.cache: dict[Path, tuple[tuple[int, int], bytes]] = {}
        self.working: set[Path] = set()

    def get(self, path: Path, stat) -> bytes | None:
        key = (stat.st_mtime_ns, stat.st_size)
        with self.lock:
            hit = self.cache.get(path)
            if hit and hit[0] == key:
                return hit[1]
        # A copy made at build time (web/tools/precompress.py) saves the small server the work.
        prebuilt = path.with_name(path.name + ".gz")
        if prebuilt.is_file() and prebuilt.stat().st_mtime_ns >= stat.st_mtime_ns:
            data = prebuilt.read_bytes()
            with self.lock:
                self.cache[path] = (key, data)
            return data
        with self.lock:
            if path in self.working:
                return None
            if stat.st_size > 2 * 1024 * 1024:  # big file: compress in the background
                self.working.add(path)
                threading.Thread(target=self._compress, args=(path, key), daemon=True).start()
                return None
        data = gzip.compress(path.read_bytes(), 6)
        with self.lock:
            self.cache[path] = (key, data)
        return data

    def _compress(self, path, key):
        try:
            data = gzip.compress(path.read_bytes(), 6)
            with self.lock:
                self.cache[path] = (key, data)
            log.info("compressed %s: %d -> %d bytes", path.name, key[1], len(data))
        finally:
            with self.lock:
                self.working.discard(path)

    def warm(self, directory: Path):
        for path in directory.glob("*.wasm"):
            stat = path.stat()
            self.get(path, stat)


# --------------------------------------------------------------------------- HTTP handler

PAGE_CSP = ("default-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; "
            "connect-src 'self'; font-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")


class Handler(BaseHTTPRequestHandler):
    server_version = "TeamForge"
    sys_version = ""
    protocol_version = "HTTP/1.1"

    store: TeamForgeStore = None  # set in main()
    throttle = Throttle()
    gz = GzipCache()

    # ---- plumbing
    def log_message(self, fmt, *args):
        log.info("%s %s", self.address_string(), fmt % args)

    def address_string(self):
        forwarded = self.headers.get("X-Forwarded-For", "")
        return forwarded.split(",")[0].strip() or self.client_address[0]

    def is_https(self) -> bool:
        return self.headers.get("X-Forwarded-Proto", "").lower() == "https"

    def token(self):
        cookie = SimpleCookie(self.headers.get("Cookie", ""))
        morsel = cookie.get("tf_session")
        return morsel.value if morsel else None

    def current_user(self):
        return self.store.user_for_token(self.token())

    def common_headers(self):
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "same-origin")
        self.send_header("X-Frame-Options", "DENY")

    def send_json(self, status, payload, cookie=None):
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        if cookie is not None:
            self.send_header("Set-Cookie", cookie)
        self.common_headers()
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def redirect(self, location):
        self.send_response(HTTPStatus.SEE_OTHER)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.send_header("Cache-Control", "no-store")
        self.common_headers()
        self.end_headers()

    def session_cookie(self, token, max_age):
        parts = [f"tf_session={token}", "Path=/", "HttpOnly", "SameSite=Lax", f"Max-Age={max_age}"]
        if self.is_https():
            parts.append("Secure")
        return "; ".join(parts)

    def read_json(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length > MAX_BODY_BYTES:
            raise ValueError("request too large")
        raw = self.rfile.read(length) if length else b""
        try:
            value = json.loads(raw or b"{}")
        except ValueError:
            raise ValueError("invalid JSON")
        if not isinstance(value, dict):
            raise ValueError("invalid JSON")
        return value

    def same_origin_write(self) -> bool:
        """Writes must come from TeamForge's own pages (custom header + matching Origin)."""
        if self.headers.get("X-TeamForge") != "1":
            return False
        origin = self.headers.get("Origin")
        if origin:
            host = self.headers.get("X-Forwarded-Host") or self.headers.get("Host", "")
            if urlsplit(origin).netloc != host:
                return False
        return True

    # ---- dispatch
    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        try:
            self.route_get(urlsplit(self.path).path)
        except Exception:
            log.exception("GET %s failed", self.path)
            self.send_json(500, {"error": "Server error"})

    def do_POST(self):
        self.handle_write()

    def do_PUT(self):
        self.handle_write()

    def handle_write(self):
        path = urlsplit(self.path).path
        if not path.startswith("/api/"):
            return self.send_json(404, {"error": "Not found"})
        if not self.same_origin_write():
            return self.send_json(403, {"error": "Request refused"})
        try:
            body = self.read_json()
        except ValueError as error:
            return self.send_json(400, {"error": str(error)})
        try:
            self.route_write(path, body)
        except Exception:
            log.exception("%s %s failed", self.command, path)
            self.send_json(500, {"error": "Server error"})

    # ---- GET routes
    def route_get(self, path):
        if path == "/healthz":
            body = b"ok"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
            return
        if path == "/":
            return self.redirect("/app/" if self.current_user() else "/login")
        if path == "/login":
            if self.current_user():
                return self.redirect("/app/")
            return self.serve_file(STATIC_DIR / "login.html", csp=PAGE_CSP)
        if path == "/admin":
            user = self.current_user()
            if not user:
                return self.redirect("/login")
            if user["role"] != "admin":
                return self.redirect("/app/")
            return self.serve_file(STATIC_DIR / "admin.html", csp=PAGE_CSP)
        if path in ("/app", "/app/", "/app/index.html"):
            if path == "/app":
                return self.redirect("/app/")
            if not self.current_user():
                return self.redirect("/login")
            return self.serve_file(STATIC_DIR / "app" / "index.html")
        if path.startswith("/app/") or path.startswith("/static/"):
            base = STATIC_DIR / "app" if path.startswith("/app/") else STATIC_DIR
            rel = path[len("/app/"):] if path.startswith("/app/") else path[len("/static/"):]
            return self.serve_static(base, rel)
        if path == "/favicon.ico":
            return self.serve_file(STATIC_DIR / "img" / "teamforge_mark_dark.png")
        if path.startswith("/api/"):
            return self.api_get(path)
        self.send_json(404, {"error": "Not found"})

    def api_get(self, path):
        user = self.current_user()
        if path == "/api/me":
            return self.send_json(200, {"user": self.public_user(user)})
        if not user:
            return self.send_json(401, {"error": "Please sign in."})
        if path == "/api/bootstrap":
            files, versions = {}, {}
            for name, (content, version) in self.store.shared_files().items():
                files[name] = content
                versions[name] = version
            own = self.store.user_file(user["id"], SESSION_FILE)
            if own:
                files[SESSION_FILE] = own[0]
                versions[SESSION_FILE] = own[1]
            return self.send_json(200, {
                "user": self.public_user(user),
                "allowedWorkspaces": WORKSPACES[user["role"]],
                "files": files,
                "versions": versions,
            })
        if path == "/api/versions":
            return self.send_json(200, {"versions": self.store.versions()})
        if path == "/api/admin/users":
            if user["role"] != "admin":
                return self.send_json(403, {"error": "Admins only."})
            return self.send_json(200, {"users": self.store.list_users(), "me": user["id"]})
        self.send_json(404, {"error": "Not found"})

    @staticmethod
    def public_user(user):
        if not user:
            return None
        return {"email": user["email"], "role": user["role"], "workspaces": WORKSPACES[user["role"]].split(",")}

    # ---- write routes
    def route_write(self, path, body):
        if path == "/api/register":
            return self.register(body)
        if path == "/api/login":
            return self.login(body)
        if path == "/api/logout":
            self.store.delete_session(self.token())
            return self.send_json(200, {"ok": True}, cookie=self.session_cookie("", 0))
        user = self.current_user()
        if not user:
            return self.send_json(401, {"error": "Please sign in again."})
        match = re.fullmatch(r"/api/files/([A-Za-z0-9_.\-]+)", path)
        if match and self.command == "PUT":
            return self.save_file(user, match.group(1), body)
        if path.startswith("/api/admin/"):
            if user["role"] != "admin":
                return self.send_json(403, {"error": "Admins only."})
            role_match = re.fullmatch(r"/api/admin/users/(\d+)/role", path)
            if role_match:
                role = body.get("role")
                if role not in ROLES:
                    return self.send_json(400, {"error": "Unknown role."})
                result = self.store.set_role(int(role_match.group(1)), role)
                if result == "not_found":
                    return self.send_json(404, {"error": "No such account."})
                if result == "last_admin":
                    return self.send_json(409, {"error": "TeamForge needs at least one admin."})
                return self.send_json(200, {"ok": True})
            if path == "/api/admin/reset-data":
                if body.get("confirm") != "RESET":
                    return self.send_json(400, {"error": "Type RESET to confirm."})
                self.store.reset_shared(user["email"])
                return self.send_json(200, {"ok": True, "versions": self.store.versions()})
        self.send_json(404, {"error": "Not found"})

    def credentials(self, body):
        email = str(body.get("email", "")).strip().lower()
        password = str(body.get("password", ""))
        return email, password

    def register(self, body):
        email, password = self.credentials(body)
        if len(email) > 254 or not EMAIL_RE.match(email):
            return self.send_json(400, {"error": "Enter a valid email address."})
        if not 8 <= len(password) <= 128:
            return self.send_json(400, {"error": "Use a password of 8 to 128 characters."})
        if self.store.find_user_by_email(email):
            return self.send_json(409, {"error": "An account with this email already exists. Sign in instead."})
        try:
            user = self.store.create_user(email, password)
        except Exception as error:  # a parallel registration of the same email
            if "unique" in str(error).lower():
                return self.send_json(409, {"error": "An account with this email already exists. Sign in instead."})
            raise
        token = self.store.create_session(user["id"])
        log.info("registered %s as %s", email, user["role"])
        return self.send_json(201, {"user": self.public_user(user)},
                              cookie=self.session_cookie(token, SESSION_DAYS * 86400))

    def login(self, body):
        email, password = self.credentials(body)
        key = (self.address_string(), email)
        if self.throttle.blocked(key):
            return self.send_json(429, {"error": "Too many attempts. Wait a few minutes and try again."})
        user = self.store.find_user_by_email(email) if email else None
        if not user or not verify_password(password, user["password_hash"]):
            self.throttle.fail(key)
            return self.send_json(401, {"error": "Email or password is incorrect."})
        self.throttle.clear(key)
        token = self.store.create_session(user["id"])
        user = self.store.user_for_token(token)  # applies ADMIN_EMAILS
        return self.send_json(200, {"user": self.public_user(user)},
                              cookie=self.session_cookie(token, SESSION_DAYS * 86400))

    def save_file(self, user, name, body):
        content = body.get("content")
        if name == SESSION_FILE:
            error = valid_file_content(name, content)
            if error:
                return self.send_json(400, {"error": error})
            version = self.store.save_user_file(user["id"], name, content)
            return self.send_json(200, {"status": "saved", "version": version})
        if name not in ALL_SHARED:
            return self.send_json(404, {"error": "Unknown file."})
        error = valid_file_content(name, content)
        if error:
            return self.send_json(400, {"error": error})
        try:
            base_version = int(body.get("baseVersion", 0))
        except (TypeError, ValueError):
            return self.send_json(400, {"error": "baseVersion must be a number"})
        status, version = self.store.save_shared(user, name, content, base_version)
        if status == "conflict":
            return self.send_json(409, {"status": "conflict", "version": version,
                                        "error": "Someone else changed this data first."})
        if status == "saved":
            log.info("%s saved %s (v%d)", user["email"], name, version)
        return self.send_json(200, {"status": status, "version": version})

    # ---- static files
    def serve_static(self, base: Path, rel: str):
        if not rel or rel.endswith("/"):
            return self.send_json(404, {"error": "Not found"})
        target = (base / rel).resolve()
        if base.resolve() not in target.parents or not target.is_file():
            return self.send_json(404, {"error": "Not found"})
        if target.name.startswith(".") or target.suffix in (".py", ".db"):
            return self.send_json(404, {"error": "Not found"})
        self.serve_file(target)

    def serve_file(self, path: Path, csp: str | None = None):
        if not path.is_file():
            return self.send_json(404, {"error": "Not found"})
        stat = path.stat()
        etag = f'"{stat.st_mtime_ns:x}-{stat.st_size:x}"'
        ctype = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        if ctype.startswith("text/") or ctype in ("application/json", "image/svg+xml"):
            ctype += "; charset=utf-8"
        long_lived = path.suffix in (".ttf", ".png", ".svg", ".ico")
        cache = "public, max-age=86400" if long_lived else "no-cache"
        if self.headers.get("If-None-Match") == etag:
            self.send_response(304)
            self.send_header("ETag", etag)
            self.send_header("Cache-Control", cache)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        data = None
        encoding = None
        if path.suffix in COMPRESSIBLE and "gzip" in self.headers.get("Accept-Encoding", ""):
            data = self.gz.get(path, stat)
            if data is not None:
                encoding = "gzip"
        if data is None:
            data = path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("ETag", etag)
        self.send_header("Cache-Control", cache)
        self.send_header("Vary", "Accept-Encoding")
        if encoding:
            self.send_header("Content-Encoding", encoding)
        if csp:
            self.send_header("Content-Security-Policy", csp)
        self.common_headers()
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)


# --------------------------------------------------------------------------- main

def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    db = Database(DATABASE_URL, SQLITE_PATH)
    store = TeamForgeStore(db, SEED_DIR)
    if store.seed_if_empty():
        log.info("first start: shared data created from the sample data")
    Handler.store = store
    if not ADMIN_EMAILS:
        log.warning("ADMIN_EMAILS is not set: nobody can become admin (Developer workspace, user roles).")
    if not (STATIC_DIR / "app" / "TeamForge.wasm").is_file():
        log.warning("static/app/TeamForge.wasm is missing: build the app for WebAssembly and copy it "
                    "into web/static/app (see web/README-WEB.md).")
    Handler.gz.warm(STATIC_DIR / "app")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.daemon_threads = True
    log.info("TeamForge web on http://%s:%d (%s)", "localhost" if HOST == "0.0.0.0" else HOST, PORT,
             "PostgreSQL" if db.is_pg else f"SQLite {SQLITE_PATH}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
