"""API tests for web/server.py. Run from the repository root:  python web/tests/test_server.py"""
import json, os, subprocess, sys, time, hashlib, gzip, urllib.request, urllib.error, http.cookiejar, tempfile, shutil
from pathlib import Path
WEB = str(Path(__file__).resolve().parent.parent); DATA = str(Path(WEB).parent / "data"); PORT = 8765; BASE = f"http://127.0.0.1:{PORT}"
DB = tempfile.mktemp(suffix=".db")
results = []
def check(name, cond, detail=""):
    results.append((name, bool(cond))); print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))

def start():
    env = dict(os.environ, PORT=str(PORT), TEAMFORGE_SQLITE=DB, ADMIN_EMAILS="admin@test.edu", HOST="127.0.0.1")
    p = subprocess.Popen([sys.executable, f"{WEB}/server.py"], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for _ in range(50):
        try: urllib.request.urlopen(BASE + "/healthz", timeout=1); return p
        except Exception: time.sleep(0.1)
    raise SystemExit(p.stdout.read().decode())

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k): return None

class Client:
    def __init__(self):
        self.jar = http.cookiejar.CookieJar()
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar), NoRedirect)
    def req(self, method, path, body=None, headers=None, raw=False):
        h = {"X-TeamForge": "1", "Content-Type": "application/json"} if body is not None else {}
        h.update(headers or {})
        data = json.dumps(body).encode() if body is not None else None
        r = urllib.request.Request(BASE + path, data=data, method=method, headers=h)
        try: resp = self.op.open(r, timeout=30)
        except urllib.error.HTTPError as e: resp = e
        payload = resp.read()
        if raw: return resp.status, resp.headers, payload
        try: payload = json.loads(payload or b"{}")
        except ValueError: pass
        return resp.status, resp.headers, payload

seed = {n: open(f"{DATA}/{n}", encoding="utf-8").read() for n in ["students.json","requirements.json","teams.json","skills.json","interest_requests.json"]}
srv = start()
try:
    anon = Client()
    s, h, _ = anon.req("GET", "/")
    check("root redirects to /login when signed out", s == 303 and h["Location"] == "/login", (s, h.get("Location")))
    s, h, b = anon.req("GET", "/login", raw=True)
    check("login page served with CSP", s == 200 and b"Sign in" in b and "Content-Security-Policy" in h, s)
    s, h, _ = anon.req("GET", "/app/")
    check("app page needs sign-in", s == 303 and h["Location"] == "/login", s)
    s, _, b = anon.req("GET", "/api/bootstrap")
    check("bootstrap needs sign-in", s == 401, s)

    p = Client()
    s, _, b = p.req("POST", "/api/register", {"email": "bad", "password": "longenough"})
    check("invalid email refused", s == 400, b)
    s, _, b = p.req("POST", "/api/register", {"email": "p@test.edu", "password": "short"})
    check("short password refused", s == 400, b)
    s, _, b = p.req("POST", "/api/register", {"email": "p@test.edu", "password": "longenough"}, headers={"X-TeamForge": "0"})
    check("write without TeamForge header refused", s == 403, b)
    s, _, b = p.req("POST", "/api/register", {"email": "p@test.edu", "password": "longenough"}, headers={"Origin": "https://evil.example"})
    check("cross-origin write refused", s == 403, b)
    s, h, b = p.req("POST", "/api/register", {"email": "P@Test.edu ", "password": "longenough"})
    check("register participant", s == 201 and b["user"]["role"] == "participant", (s, b))
    check("session cookie is HttpOnly", "HttpOnly" in h.get("Set-Cookie", ""), h.get("Set-Cookie"))
    s, _, b = Client().req("POST", "/api/register", {"email": "p@test.edu", "password": "another123"})
    check("duplicate email refused", s == 409, b)

    s, _, boot = p.req("GET", "/api/bootstrap")
    check("bootstrap ok", s == 200, boot)
    check("participant may open participant only", boot["allowedWorkspaces"] == "participant", boot.get("allowedWorkspaces"))
    check("bootstrap has 6 shared files at v1", len(boot["files"]) == 6 and set(boot["versions"].values()) == {1}, boot["versions"])
    check("shared files are the seed files byte-for-byte", all(boot["files"][n] == seed[n] for n in seed))
    expected = hashlib.sha256(b"".join(n.encode() + b":" + hashlib.sha256(open(f"{DATA}/{n}", "rb").read()).hexdigest().encode() + b";"
                                       for n in ["students.json","requirements.json","teams.json","skills.json","interest_requests.json"])).hexdigest()
    check("seed stamp computed like SampleData.cpp", boot["files"][".sample-data"].strip() == expected)

    students = json.loads(seed["students.json"]); students.pop("note")
    students["students"][0]["summary"] = "Edited in the browser"
    text1 = json.dumps(students, indent=4)
    s, _, b = p.req("PUT", "/api/files/students.json", {"content": text1, "baseVersion": 1})
    check("participant saves students.json", s == 200 and b == {"status": "saved", "version": 2}, (s, b))
    req = json.loads(seed["requirements.json"]); req["requirements"][0]["name"] = "Hacked"
    s, _, b = p.req("PUT", "/api/files/requirements.json", {"content": json.dumps(req), "baseVersion": 1})
    check("participant change to requirements.json ignored", s == 200 and b["status"] == "ignored", b)
    s, _, boot2 = p.req("GET", "/api/bootstrap")
    check("requirements.json unchanged on server", boot2["files"]["requirements.json"] == seed["requirements.json"])
    stale = json.loads(seed["students.json"]); stale["students"][1]["summary"] = "Other change"
    s, _, b = p.req("PUT", "/api/files/students.json", {"content": json.dumps(stale), "baseVersion": 1})
    check("stale upload refused with 409", s == 409 and b["version"] == 2, (s, b))
    reordered = json.loads(text1); reordered["students"].reverse()
    s, _, b = p.req("PUT", "/api/files/students.json", {"content": json.dumps(reordered, indent=2), "baseVersion": 1})
    check("re-formatted / re-ordered copy counts as unchanged", s == 200 and b["status"] == "unchanged" and b["version"] == 2, b)
    s, _, b = p.req("PUT", "/api/files/students.json", {"content": "not json", "baseVersion": 2})
    check("invalid JSON refused", s == 400, b)
    s, _, b = p.req("PUT", "/api/files/../server.py", {"content": "{}", "baseVersion": 0})
    check("unknown file name refused", s in (404,), (s, b))
    session = json.dumps({"version": 1, "lastWorkspace": "participant", "myProfileId": "tf-001", "recentProjects": []})
    s, _, b = p.req("PUT", "/api/files/session.json", {"content": session, "baseVersion": 0})
    check("own session saved", s == 200 and b["status"] == "saved", b)

    s, _, b = p.req("POST", "/api/logout", {})
    s2, _, _ = p.req("GET", "/api/bootstrap")
    check("logout ends the session", s == 200 and s2 == 401, (s, s2))
    s, _, b = p.req("POST", "/api/login", {"email": "p@test.edu", "password": "wrongpass"})
    check("wrong password refused", s == 401, b)
    s, _, b = p.req("POST", "/api/login", {"email": "p@test.edu", "password": "longenough"})
    check("sign in again with the same email + password", s == 200 and b["user"]["email"] == "p@test.edu", b)
    s, _, boot3 = p.req("GET", "/api/bootstrap")
    check("after re-login: own session.json is back", json.loads(boot3["files"]["session.json"])["myProfileId"] == "tf-001")
    check("after re-login: saved profile edit is there", json.loads(boot3["files"]["students.json"])["students"][0]["summary"] == "Edited in the browser")

    a = Client()
    s, _, b = a.req("POST", "/api/register", {"email": "admin@test.edu", "password": "adminpass1"})
    check("ADMIN_EMAILS account becomes admin", s == 201 and b["user"]["role"] == "admin", b)
    s, _, b = p.req("GET", "/api/admin/users")
    check("participant cannot list users", s == 403, s)
    s, h, page = p.req("GET", "/admin")
    check("participant sent away from /admin", s == 303, s)
    s, _, page = a.req("GET", "/admin", raw=True)
    check("admin page for admin", s == 200 and b"Reset shared data" in page, s)
    s, _, users = a.req("GET", "/api/admin/users")
    pid = [u["id"] for u in users["users"] if u["email"] == "p@test.edu"][0]
    aid = users["me"]
    s, _, b = a.req("POST", f"/api/admin/users/{aid}/role", {"role": "host"})
    check("last admin cannot be demoted", s == 409, b)
    s, _, b = a.req("POST", f"/api/admin/users/{pid}/role", {"role": "host"})
    check("admin makes participant a host", s == 200, b)
    s, _, boot4 = p.req("GET", "/api/bootstrap")
    check("host may open participant + host", boot4["allowedWorkspaces"] == "participant,host", boot4["allowedWorkspaces"])
    s, _, b = p.req("PUT", "/api/files/requirements.json", {"content": json.dumps(req), "baseVersion": 1})
    check("host can save requirements.json", s == 200 and b["status"] == "saved", b)
    s, _, b = p.req("PUT", "/api/files/skills.json", {"content": json.dumps({"version": 1, "skills": []}), "baseVersion": 1})
    check("host change to skills.json ignored", b.get("status") == "ignored", b)
    s, _, aboot = a.req("GET", "/api/bootstrap")
    check("admin may open all three workspaces", aboot["allowedWorkspaces"] == "participant,host,developer")
    s, _, b = a.req("POST", "/api/admin/reset-data", {"confirm": "nope"})
    check("reset needs RESET", s == 400, b)
    s, _, b = a.req("POST", "/api/admin/reset-data", {"confirm": "RESET"})
    s, _, boot5 = a.req("GET", "/api/bootstrap")
    check("reset restores seed and bumps versions", boot5["files"]["students.json"] == seed["students.json"] and boot5["versions"]["students.json"] == 3, boot5["versions"])

    t = Client()
    codes = [t.req("POST", "/api/login", {"email": "p@test.edu", "password": f"bad{i}xxxx"})[0] for i in range(9)]
    check("login throttled after repeated failures", codes[-1] == 429 and codes[0] == 401, codes)

    # static files
    s, h, b = p.req("GET", "/app/", raw=True)
    check("app page for signed-in user", s == 200 and b"qtloader.js" in b, s)
    s, h, b = p.req("GET", "/app/app.js", raw=True, headers={"Accept-Encoding": "gzip"})
    check("app.js gzip-compressed", s == 200 and h.get("Content-Encoding") == "gzip" and b"TF_upload" in gzip.decompress(b), h.get("Content-Encoding"))
    etag = h["ETag"]
    s, h, b = p.req("GET", "/app/app.js", raw=True, headers={"If-None-Match": etag})
    check("ETag revalidation gives 304", s == 304, s)
    for bad in ["/static/../server.py", "/static/%2e%2e/server.py", "/app/../../server.py", "/static/fonts/../../server.py"]:
        s, _, _ = anon.req("GET", bad, raw=True)
        check(f"traversal blocked {bad}", s == 404, s)
    s, h, _ = anon.req("GET", "/static/fonts/Manrope-Bold.ttf", raw=True)
    check("font served with font/ttf", s == 200 and h["Content-Type"].startswith("font/ttf"), h.get("Content-Type"))
finally:
    srv.terminate(); srv.wait()

# persistence across a restart (same database)
srv = start()
try:
    p = Client()
    s, _, b = p.req("POST", "/api/login", {"email": "p@test.edu", "password": "longenough"})
    check("account survives a server restart", s == 200 and b["user"]["role"] == "host", b)
    s, _, boot = p.req("GET", "/api/bootstrap")
    check("data and session survive a server restart", "session.json" in boot["files"] and boot["versions"]["requirements.json"] >= 2, boot["versions"])
finally:
    srv.terminate(); out = srv.communicate()[0].decode()
os.remove(DB) if os.path.exists(DB) else None
print(f"\n{sum(ok for _, ok in results)}/{len(results)} passed")
sys.exit(0 if all(ok for _, ok in results) else 1)
