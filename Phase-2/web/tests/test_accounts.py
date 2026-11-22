"""Tests for account emails and identity: password reset, email verification, the email service
(outbox provider), profile photos, the account <-> participant-profile link, migration.

Run from the repository root:  python web/tests/test_accounts.py
"""
import base64, http.cookiejar, json, os, re, sqlite3, subprocess, sys, tempfile, threading, time, urllib.error, urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import testdb  # noqa: E402  (SQLite or PostgreSQL, like the server)

WEB = str(Path(__file__).resolve().parent.parent)
PORT = 8768
BASE = f"http://127.0.0.1:{PORT}"
DB = tempfile.mktemp(suffix=".db")
OUTBOX = Path(tempfile.mkdtemp(prefix="tf-outbox-"))
results = []


def check(name, cond, detail=""):
    results.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))


def start(email=True):
    env = dict(os.environ, PORT=str(PORT), TEAMFORGE_SQLITE=DB, ADMIN_EMAILS="admin@test.edu", HOST="127.0.0.1")
    for k in ("EMAIL_PROVIDER", "EMAIL_API_KEY", "EMAIL_FROM", "APP_BASE_URL", "AZURE_DOCINTEL_ENDPOINT"):
        env.pop(k, None)
    if email:
        env.update(EMAIL_PROVIDER="outbox", EMAIL_OUTBOX_DIR=str(OUTBOX), APP_BASE_URL="https://teamforge.example")
    p = subprocess.Popen([sys.executable, f"{WEB}/server.py"], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    p.log = []
    threading.Thread(target=lambda: p.log.extend(p.stdout), daemon=True).start()
    for _ in range(80):
        try:
            urllib.request.urlopen(BASE + "/healthz", timeout=1)
            return p
        except Exception:
            time.sleep(0.1)
    raise SystemExit(b"".join(p.log).decode())


def stop(p):
    p.terminate(); p.wait(); time.sleep(0.2)
    return b"".join(p.log).decode(errors="replace")


class Client:
    def __init__(self):
        self.jar = http.cookiejar.CookieJar()
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))

    def req(self, method, path, body=None, headers=None):
        h = {"X-TeamForge": "1", "Content-Type": "application/json"} if body is not None else {}
        h.update(headers or {})
        r = urllib.request.Request(BASE + path, data=json.dumps(body).encode() if body is not None else None,
                                   method=method, headers=h)
        try:
            resp = self.op.open(r, timeout=20)
        except urllib.error.HTTPError as e:
            resp = e
        raw = resp.read()
        try:
            return resp.status, json.loads(raw or b"{}")
        except ValueError:
            return resp.status, raw


def emails(kind=None, to=None, wait=3.0):
    """Emails in the outbox (written on a background thread, so wait a little)."""
    deadline = time.time() + wait
    while True:
        found = []
        for f in sorted(OUTBOX.glob("*.html")):
            text = f.read_text(encoding="utf-8")
            head = text.split("\n", 1)[0]
            if (kind is None or f"-{kind}-" in f.name) and (to is None or f"To: {to} " in head):
                found.append(text)
        if found or time.time() > deadline:
            return found
        time.sleep(0.1)


def token_from(email_html, path):
    m = re.search(re.escape(f"https://teamforge.example{path}?token=") + r"([A-Za-z0-9_\-]+)", email_html)
    return m.group(1) if m else None


PNG = ("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==")

srv = start()
try:
    # ------------------------------------------------------------ pages
    for page in ("/forgot", "/reset", "/verify"):
        r = urllib.request.urlopen(BASE + page, timeout=5)
        check(f"{page} served with CSP", r.status == 200 and "Content-Security-Policy" in r.headers)
    for name in ("forgot.html", "reset.html", "verify.html", "login.html"):
        html = Path(WEB, "static", name).read_text(encoding="utf-8")
        inline = re.findall(r"<script(?![^>]*\bsrc=)[^>]*>", html) + re.findall(r"\sstyle=\"", html)
        check(f"{name} has no inline scripts or styles", not inline, inline)
    login = Path(WEB, "static", "login.html").read_text(encoding="utf-8")
    check("sign-in page has show/hide toggles and a Forgot password link",
          login.count('class="reveal"') == 3 and 'href="/forgot"' in login)

    # ------------------------------------------------------------ registration emails and verification
    a = Client()
    s, b = a.req("POST", "/api/register", {"email": "asha@test.edu", "password": "first-pass-1"})
    check("register", s == 201 and b["user"]["emailVerified"] is False, (s, b))
    verify = emails("verify_email", "asha@test.edu")
    check("verification and welcome emails written", verify and emails("welcome", "asha@test.edu"))
    vtoken = token_from(verify[0], "/verify") if verify else None
    check("verification link uses APP_BASE_URL", vtoken is not None)
    s, b = Client().req("POST", "/api/verify-email", {"token": vtoken})
    check("email confirmed with the link", s == 200, (s, b))
    s, b = Client().req("POST", "/api/verify-email", {"token": vtoken})
    check("verification link works only once", s == 400, (s, b))
    s, b = a.req("GET", "/api/me")
    check("account shows as verified", b["user"]["emailVerified"] is True, b)
    s, b = a.req("POST", "/api/verify-email/resend", {})
    check("resend when already verified says so", s == 200 and "already" in b["message"], (s, b))

    # ------------------------------------------------------------ forgot / reset
    before = len(list(OUTBOX.glob("*password_reset*")))
    s, unknown = Client().req("POST", "/api/password/forgot", {"email": "nobody@test.edu"})
    s2, known = Client().req("POST", "/api/password/forgot", {"email": "asha@test.edu"},
                             headers={"Host": "evil.example", "X-Forwarded-Host": "evil.example"})
    check("same answer whether or not the email has an account", s == s2 == 200 and unknown == known, (unknown, known))
    resets = emails("password_reset", "asha@test.edu")
    check("reset email only for the real account", len(resets) == 1 and before == 0, len(resets))
    rtoken = token_from(resets[0], "/reset") if resets else None
    check("reset link uses APP_BASE_URL, never the request's Host", rtoken and "evil.example" not in resets[0])
    check("reset email never contains a password", "first-pass-1" not in resets[0])
    s, b = Client().req("POST", "/api/password/check", {"token": rtoken})
    check("reset link checks as valid", b.get("valid") is True, b)
    s, b = Client().req("POST", "/api/password/reset", {"token": rtoken, "password": "short"})
    check("short new password refused", s == 400, (s, b))
    s, b = Client().req("POST", "/api/password/reset", {"token": "x" * 40, "password": "whatever-long"})
    check("unknown token refused", s == 400, (s, b))
    s, b = Client().req("POST", "/api/password/reset", {"token": rtoken, "password": "second-pass-2"})
    check("password reset with the link", s == 200, (s, b))
    s, b = a.req("GET", "/api/bootstrap")
    check("existing sessions end after a reset", s == 401, s)
    s, b = Client().req("POST", "/api/login", {"email": "asha@test.edu", "password": "first-pass-1"})
    check("old password no longer works", s == 401, s)
    s, b = a.req("POST", "/api/login", {"email": "asha@test.edu", "password": "second-pass-2"})
    check("new password works", s == 200, s)
    s, b = Client().req("POST", "/api/password/reset", {"token": rtoken, "password": "third-pass-3"})
    check("reset link works only once", s == 400, (s, b))
    # expiry: an older link stops working
    Client().req("POST", "/api/password/forgot", {"email": "asha@test.edu"})
    time.sleep(0.5)
    t2 = token_from(emails("password_reset", "asha@test.edu")[-1], "/reset")
    con = testdb.connect(DB); con.execute("UPDATE auth_tokens SET expires_at = 1 WHERE purpose = 'reset'"); con.commit(); con.close()
    s, b = Client().req("POST", "/api/password/reset", {"token": t2, "password": "third-pass-3"})
    check("expired reset link refused", s == 400 and "expired" in b["error"], (s, b))
    con = testdb.connect(DB)
    stored = [r[0] for r in con.execute("SELECT token_hash FROM auth_tokens")]
    con.close()
    check("only token hashes are stored", t2 not in stored and all(len(h) == 64 for h in stored), stored)
    codes = [Client().req("POST", "/api/password/forgot", {"email": f"x{i}@test.edu"})[0] for i in range(6)]
    check("reset requests are rate-limited", 429 in codes, codes)

    # ------------------------------------------------------------ photos and identity
    s, b = a.req("POST", "/api/hub/profile", {"name": "Asha Rao", "avatar": PNG})
    ref = b["profile"]["avatar"]
    check("photo stored apart; the profile carries a reference", s == 200 and ref and ref.startswith("/api/hub/photos/")
          and "base64" not in json.dumps(b), b)
    uid = b["profile"]["id"]
    s, b = Client().req("GET", f"/api/hub/photos/{uid}")
    check("photos need sign-in", s == 401, s)
    s, b = a.req("GET", f"/api/hub/photos/{uid}")
    check("photo served by its reference", s == 200 and b["image"] == PNG, s)
    s, b = a.req("POST", "/api/hub/profile/photo", {"image": PNG})
    check("a new photo replaces the old (new version)", s == 200 and b["avatar"] != ref and b["avatar"].endswith("v=2"), b)
    s, b = a.req("POST", "/api/hub/profile/photo", {"image": "data:image/png;base64,AAAA"})
    check("non-image refused", s == 400, (s, b))
    # the account uses participant profile tf-001 in the app
    session = json.dumps({"version": 1, "lastWorkspace": "participant", "myProfileId": "tf-001", "recentProjects": []})
    s, b = a.req("PUT", "/api/files/session.json", {"content": session})
    s, b = a.req("GET", "/api/hub/identities")
    mine = [i for i in b["items"] if i["id"] == uid]
    check("account linked to its participant profile", s == 200 and mine and mine[0]["studentId"] == "tf-001"
          and mine[0]["avatar"].endswith("v=2") and "asha@test.edu" not in json.dumps(b), b)
    other = Client()
    other.req("POST", "/api/register", {"email": "ravi@test.edu", "password": "ravi-pass-1"})
    other.req("PUT", "/api/files/session.json", {"content": session.replace("participant", "host")})
    s, b = other.req("GET", "/api/hub/profile")
    check("a new account takes its participant profile's name", b["profile"]["name"] == "Mukul Fartiyal"
          and b["profile"]["studentId"] == "tf-001", b["profile"])
    s, b = a.req("GET", "/api/hub/identities")
    check("a participant profile belongs to one account (the latest)", [i["id"] for i in b["items"] if i["studentId"] == "tf-001"]
          == [other.req("GET", "/api/me")[1]["user"]["id"]], b)
    s, b = a.req("POST", "/api/hub/profile/sync", {"name": "Asha R. Rao", "program": "B.Tech CSE · 2nd year", "studentId": "tf-002"})
    check("app keeps name and academic details in step", s == 200 and b["profile"]["name"] == "Asha R. Rao"
          and b["profile"]["program"] == "B.Tech CSE · 2nd year" and b["profile"]["studentId"] == "tf-002", b)
    s, b = a.req("POST", "/api/hub/profile/photo/remove", {})
    check("photo removed", s == 200 and b["avatar"] is None, b)

    # ------------------------------------------------------------ decision emails, opt-out
    other.req("POST", "/api/hub/profile", {"name": "Ravi Kumar"})
    s, team = a.req("POST", "/api/hub/teams", {"name": "Mail Team", "maxMembers": 4})
    tid = team["team"]["id"]
    s, app = other.req("POST", f"/api/hub/teams/{tid}/applications", {"introduction": "Hi", "reason": "Fun"})
    a.req("POST", f"/api/hub/team-applications/{app['application']['id']}/decision", {"decision": "approve", "response": "Welcome!"})
    decided = emails("application_decision", "ravi@test.edu")
    check("approval emailed to the applicant", decided and "Mail Team" in decided[0] and "Welcome!" in decided[0])
    other.req("POST", "/api/hub/profile", {"name": "Ravi Kumar", "emailUpdates": False})
    a.req("POST", f"/api/hub/teams/{tid}/members/{other.req('GET', '/api/me')[1]['user']['id']}/remove", {})
    time.sleep(1)
    check("no email after turning email updates off", not emails("team_notification", "ravi@test.edu", wait=0.5))
finally:
    log_text = stop(srv)
check("server log has no tokens or email contents", (not rtoken or rtoken not in log_text) and "Welcome!" not in log_text
      and "Traceback" not in log_text, log_text[-1500:])

# ------------------------------------------------------------ migration of old in-row photos, email off
con = testdb.connect(DB)
con.execute("UPDATE profiles SET avatar = ? WHERE user_id = (SELECT id FROM users WHERE email = 'ravi@test.edu')", (PNG,))
con.commit(); con.close()
srv = start(email=False)
try:
    c = Client()
    c.req("POST", "/api/login", {"email": "ravi@test.edu", "password": "ravi-pass-1"})
    s, b = c.req("GET", "/api/hub/profile")
    check("old in-row photos move to the photo table on start", b["profile"]["avatar"] and b["profile"]["avatar"].startswith("/api/hub/photos/"), b["profile"])
    s, b = c.req("POST", "/api/password/forgot", {"email": "ravi@test.edu"})
    check("without email set up, forgot still answers the same", s == 200 and b["ok"] is True, (s, b))
    s, b = c.req("POST", "/api/verify-email/resend", {})
    check("resend says email is not set up (never fakes delivery)", s == 503, (s, b))
finally:
    log_text = stop(srv)
check("server explains that email is off", "EMAIL_PROVIDER is not set" in log_text, log_text[-800:])

for leftover in Path(DB).parent.glob(Path(DB).name + "*"):
    try: leftover.unlink()
    except OSError: pass
print(f"\n{sum(ok for _, ok in results)}/{len(results)} passed")
sys.exit(0 if all(ok for _, ok in results) else 1)
