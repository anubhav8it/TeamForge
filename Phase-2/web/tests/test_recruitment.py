"""API tests for team recruitment (web/recruitment.py through web/server.py).

Run from the repository root:  python web/tests/test_recruitment.py

Starts the real server on a temporary SQLite database (with the development email outbox) and
checks: required skills with levels, finding members (who may search, who is listed, search over
participant profiles), direct invitations (duplicates, permissions, accept / decline / withdraw /
expiry, the last place taken by two people at once), invitation links (token security, preview
without sign-in, sign-in round trip, application through a link, regenerate and revoke), the
leader's combined list, notifications and the invitation email.
"""
import http.cookiejar, json, os, sqlite3, subprocess, sys, tempfile, threading, time, urllib.error, urllib.request
from pathlib import Path

WEB = str(Path(__file__).resolve().parent.parent)
PORT = 8769
BASE = f"http://127.0.0.1:{PORT}"
DB = tempfile.mktemp(suffix=".db")
OUTBOX = Path(tempfile.mkdtemp(prefix="tf-outbox-"))
results = []


def check(name, cond, detail=""):
    results.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))


def start():
    env = dict(os.environ, PORT=str(PORT), TEAMFORGE_SQLITE=DB, ADMIN_EMAILS="admin@test.edu", HOST="127.0.0.1",
               EMAIL_PROVIDER="outbox", EMAIL_OUTBOX_DIR=str(OUTBOX), APP_BASE_URL="https://teamforge.example",
               EMAIL_FROM="team@teamforge.example")
    env.pop("EMAIL_API_KEY", None)
    env.pop("AZURE_DOCINTEL_ENDPOINT", None)
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


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


class Client:
    def __init__(self, email=None, password="longenough1"):
        self.jar = http.cookiejar.CookieJar()
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))
        self.raw = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar), NoRedirect)
        self.email = email
        if email:
            s, _, b, _ = self.req("POST", "/api/register", {"email": email, "password": password})
            assert s == 201, (s, b)
            self.id = self.req("GET", "/api/me")[2]["user"]["id"]

    def req(self, method, path, body=None, timeout=40, opener=None):
        h = {"X-TeamForge": "1", "Content-Type": "application/json"} if body is not None else {}
        data = json.dumps(body).encode() if body is not None else None
        r = urllib.request.Request(BASE + path, data=data, method=method, headers=h)
        try:
            resp = (opener or self.op).open(r, timeout=timeout)
        except urllib.error.HTTPError as e:
            resp = e
        raw = resp.read()
        try:
            payload = json.loads(raw or b"{}")
        except ValueError:
            payload = raw
        return resp.status, raw, payload, resp.headers

    def get(self, path):
        s, _, b, _ = self.req("GET", "/api/hub" + path)
        return s, b

    def post(self, path, body=None):
        s, _, b, _ = self.req("POST", "/api/hub" + path, body if body is not None else {})
        return s, b

    def profile(self, name, **extra):
        s, b = self.post("/profile", dict({"name": name, "headline": f"{name}'s headline", "skills": ["python"]}, **extra))
        assert s == 200, (s, b)


def notifications(client):
    return client.get("/notifications")[1]["items"]


def outbox(kind, wait=3.0):
    end = time.time() + wait
    while time.time() < end:
        found = [p for p in OUTBOX.glob("*.html") if kind in p.name]
        if found:
            return found
        time.sleep(0.1)
    return []


srv = start()
try:
    leader = Client("leader@test.edu")
    helper = Client("helper@test.edu")
    alice = Client("alice@test.edu")
    bob = Client("bob@test.edu")
    carol = Client("carol@test.edu")
    hidden = Client("hidden@test.edu")
    newbie = Client("newbie@test.edu")   # no profile yet
    for c, n in ((leader, "Lena Leader"), (helper, "Hari Helper"), (alice, "Alice Rao"), (bob, "Bob Iyer"),
                 (carol, "Carol Das")):
        c.profile(n)
    hidden.profile("Hidden Person", openToInvites=False)

    # --- required skills with levels -------------------------------------------------------
    s, b = leader.post("/teams", {"name": "Razor", "tagline": "Campus app", "maxMembers": 3,
                                  "requiredSkills": {"C++": 3, " Qt  Quick ": 2, "ui design": 1}})
    check("team created with required skills", s == 201, b)
    team = b["team"]
    tid = team["id"]
    check("skills normalised, levels kept", team["requiredSkills"] == {"c++": 3, "qt quick": 2, "ui design": 1},
          team.get("requiredSkills"))
    check("vacancies shown", team["vacancies"] == 2 and team["maxMembers"] == 3, team)
    s, b = leader.post("/teams", {"name": "Bad levels", "requiredSkills": {"c++": 9}})
    check("level outside 1-5 refused", s == 400, b)
    s, b = leader.post("/teams", {"name": "Old app team", "skills": ["Rust", "go"]})
    check("older apps (names only) still work: any level", s == 201 and b["team"]["requiredSkills"] == {"rust": 1, "go": 1},
          b)
    s, b = leader.post(f"/teams/{tid}", {"requiredSkills": {"c++": 3, "qt quick": 2, "ui design": 2, "testing": 2}})
    check("required skills edited", s == 200 and b["team"]["requiredSkills"]["testing"] == 2, b)
    s, b = leader.get("/teams?q=razor")
    check("team list carries required skills", b["items"][0]["requiredSkills"].get("c++") == 3, b)

    # helper becomes an admin of Razor through an application
    s, b = helper.post(f"/teams/{tid}/applications", {"introduction": "Hi", "reason": "Fun"})
    leader.post(f"/team-applications/{b['application']['id']}/decision", {"decision": "approve"})
    leader.post(f"/teams/{tid}/members/{helper.id}/role", {"role": "admin"})

    # --- finding members -------------------------------------------------------------------
    s, b = alice.get(f"/teams/{tid}/candidates")
    check("non-members cannot search for candidates", s == 403, b)
    s, b = leader.get(f"/teams/{tid}/candidates")
    names = [p["name"] for p in b.get("items", [])]
    check("leader sees registered people", s == 200 and {"Alice Rao", "Bob Iyer", "Carol Das"} <= set(names), names)
    check("members, the leader and people without a profile are not listed",
          "Lena Leader" not in names and "Hari Helper" not in names and len([n for n in names if n == "New member"]) == 0,
          names)
    check("people who turned invitations off are not listed", "Hidden Person" not in names, names)
    check("no email addresses in candidates", "@test.edu" not in json.dumps(b), "")
    s, b = helper.get(f"/teams/{tid}/candidates?q=alice")
    check("team admins can search too; search by name", s == 200 and [p["name"] for p in b["items"]] == ["Alice Rao"], b)
    # search reaches the participant profile (skills, projects) linked to the account
    alice.post("/profile/sync", {"name": "Alice Rao", "program": "B.Tech CSE", "studentId": "tf-001"})
    s, b = leader.get(f"/teams/{tid}/candidates?q=system%20design")
    check("search covers the linked participant profile's skills",
          [p["name"] for p in b["items"]] == ["Alice Rao"] and b["items"][0]["studentId"] == "tf-001", b)
    s, b = leader.get(f"/teams/{tid}/candidates?q=zzzz-nothing")
    check("no match: empty list", s == 200 and b["items"] == [], b)

    # portfolio links on profiles
    s, b = bob.post("/profile", {"name": "Bob Iyer", "links": ["http://insecure.example"]})
    check("only https links accepted", s == 400, b)
    s, b = bob.post("/profile", {"name": "Bob Iyer", "links": ["javascript:alert(1)"]})
    check("script links refused", s == 400, b)
    s, b = bob.post("/profile", {"name": "Bob Iyer", "skills": ["python", "testing"],
                                 "links": ["https://github.com/bob", "https://bob.dev/portfolio"]})
    check("links saved", s == 200 and b["profile"]["links"] == ["https://github.com/bob", "https://bob.dev/portfolio"], b)
    s, b = bob.post("/profile", {"name": "Bob Iyer", "skills": ["python", "testing"]})
    check("links kept when an older app saves without them", b["profile"]["links"] == ["https://github.com/bob",
                                                                                        "https://bob.dev/portfolio"], b)

    # --- direct invitations ----------------------------------------------------------------
    s, b = alice.post(f"/teams/{tid}/invitations", {"userId": bob.id})
    check("non-members cannot invite", s == 403, b)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": hidden.id})
    check("cannot invite someone who turned invitations off", s == 403, b)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": newbie.id})
    check("cannot invite an account without a profile", s == 404, b)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": helper.id})
    check("cannot invite a member", s == 409, b)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": bob.id, "message": "We need a tester!"})
    check("leader invites Bob", s == 201 and b["invitation"]["status"] == "pending", b)
    bob_inv = b["invitation"]["id"]
    s, b = helper.post(f"/teams/{tid}/invitations", {"userId": bob.id})
    check("no second open invitation", s == 409, b)
    n = notifications(bob)
    check("Bob is notified", any(x["link"] == "#/invitations" and "Razor" in x["title"] for x in n), n)
    check("invitation email written (outbox)", outbox("team_invitation"), list(OUTBOX.iterdir()))
    s, b = bob.get("/invitations/mine")
    item = b["items"][0] if b.get("items") else {}
    check("Bob sees the invitation with the team and message",
          s == 200 and item.get("id") == bob_inv and item["team"]["name"] == "Razor" and item["message"] == "We need a tester!"
          and item["invitedBy"]["name"] == "Lena Leader", b)
    check("invitation shows the team's needs, not its members", "members" not in item.get("team", {})
          and item["team"]["requiredSkills"].get("c++") == 3, item.get("team"))
    s, b = bob.get(f"/teams/{tid}")
    check("team page shows Bob his open invitation", b["team"]["myInvitation"]["id"] == bob_inv, b["team"].get("myInvitation"))
    s, b = bob.get("/summary")
    check("summary counts open invitations", b["pendingInvitations"] == 1, b.get("pendingInvitations"))
    s, b = alice.post(f"/team-invitations/{bob_inv}/respond", {"decision": "accept"})
    check("someone else cannot answer Bob's invitation", s == 404, b)

    # carol applies; inviting her afterwards points to the application
    s, b = carol.post(f"/teams/{tid}/applications", {"introduction": "Hello", "reason": "I like C++"})
    carol_app = b["application"]["id"]
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": carol.id})
    check("inviting someone who applied: review the application instead", s == 409 and b.get("code") == "already_applied", b)

    # the leader's combined list
    s, b = leader.get(f"/teams/{tid}/recruitment?filter=pending")
    kinds = sorted((i["kind"], i["status"]) for i in b["items"])
    check("pending list holds the application and the invitation", kinds == [("invitation", "pending"), ("team", "pending")], b)
    check("counts for the tabs", b["counts"] == {"pendingApplications": 1, "pendingInvitations": 1}, b.get("counts"))
    s, b = alice.get(f"/teams/{tid}/recruitment")
    check("only managers see the list", s == 403, b)

    # Bob accepts: he joins (the leader chose him); capacity is now full (3/3)
    s, b = bob.post(f"/team-invitations/{bob_inv}/respond", {"decision": "accept", "response": "Happy to!"})
    check("Bob accepts and joins", s == 200 and b["invitation"]["status"] == "accepted", b)
    s, b = leader.get(f"/teams/{tid}")
    check("Bob is a member; team full", any(m["id"] == bob.id for m in b["team"]["members"]) and b["team"]["full"], b)
    check("leader told Bob joined", any("Bob Iyer joined Razor" in x["title"] for x in notifications(leader)), "")
    s, b = bob.post(f"/team-invitations/{bob_inv}/respond", {"decision": "accept"})
    check("an answered invitation cannot be answered again", s == 409, b)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": alice.id})
    check("no invitations while the team is full", s == 409, b)
    s, b = leader.post(f"/team-applications/{carol_app}/decision", {"decision": "approve"})
    check("full team: approving an application refused too", s == 409, b)
    leader.post(f"/team-applications/{carol_app}/decision", {"decision": "reject"})

    # two people accept the last place at the same moment: exactly one gets it
    leader.post(f"/teams/{tid}", {"maxMembers": 4})
    ids = {}
    for c in (alice, carol):
        ids[c.email] = leader.post(f"/teams/{tid}/invitations", {"userId": c.id})[1]["invitation"]["id"]
    outcomes = []
    barrier = threading.Barrier(2)

    def accept(c):
        barrier.wait()
        outcomes.append(c.post(f"/team-invitations/{ids[c.email]}/respond", {"decision": "accept"})[0])

    threads = [threading.Thread(target=accept, args=(c,)) for c in (alice, carol)]
    [t.start() for t in threads]
    [t.join() for t in threads]
    s, b = leader.get(f"/teams/{tid}")
    check("last place: one accepted, one refused", sorted(outcomes) == [200, 409], outcomes)
    check("never more members than the team size", b["team"]["memberCount"] == 4, b["team"]["memberCount"])
    loser = alice if any(m["id"] == carol.id for m in b["team"]["members"]) else carol
    s, b = loser.get("/invitations/mine?status=pending")
    check("the refused invitation stays open (in case a place opens)", len(b["items"]) == 1, b)

    # withdraw, decline
    s, b = loser.post(f"/team-invitations/{ids[loser.email]}/respond", {"decision": "decline", "response": "Maybe later"})
    check("decline works", s == 200 and b["invitation"]["status"] == "declined", b)
    check("leader told about the decline", any("declined" in x["title"] for x in notifications(leader)), "")
    leader.post(f"/teams/{tid}", {"maxMembers": 6})
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": loser.id})
    check("after a decline the person can be invited again", s == 201, b)
    again = b["invitation"]["id"]
    s, b = alice.post(f"/team-invitations/{again}/revoke") if loser is not alice else carol.post(f"/team-invitations/{again}/revoke")
    check("only managers withdraw invitations", s in (403, 404), b)
    s, b = helper.post(f"/team-invitations/{again}/revoke")
    check("an admin withdraws it", s == 200 and b["invitation"]["status"] == "revoked", b)
    s, b = loser.post(f"/team-invitations/{again}/respond", {"decision": "accept"})
    check("a withdrawn invitation cannot be accepted", s == 409, b)
    check("the person is told it was withdrawn", any("withdrawn" in x["title"] for x in notifications(loser)), "")

    # expiry (moved into the past directly in the database)
    s, b = leader.post(f"/teams/{tid}/invitations", {"userId": loser.id})
    old = b["invitation"]["id"]
    con = sqlite3.connect(DB)
    con.execute("UPDATE team_invitations SET expires_at = 1 WHERE id = ?", (old,))
    con.commit()
    con.close()
    s, b = loser.get("/invitations/mine")
    check("expired invitations show as expired", next(i for i in b["items"] if i["id"] == old)["status"] == "expired", b)
    s, b = loser.post(f"/team-invitations/{old}/respond", {"decision": "accept"})
    check("an expired invitation cannot be accepted", s == 409, b)
    check("the person is told it expired", any("expired" in x["title"] for x in notifications(loser)), "")
    s, b = leader.get(f"/teams/{tid}/recruitment?filter=declined")
    check("declined / withdrawn / expired listed together", {i["status"] for i in b["items"]} == {"declined", "revoked", "expired"},
          [i["status"] for i in b["items"]])
    s, b = leader.get(f"/teams/{tid}/recruitment?filter=accepted")
    check("accepted invitations listed", len(b["items"]) == 2 and all(i["status"] == "accepted" for i in b["items"]), b)

    # --- invitation links ------------------------------------------------------------------
    s, b = alice.post(f"/teams/{tid}/invite-link") if loser is alice else carol.post(f"/teams/{tid}/invite-link")
    check("ordinary members cannot make links", s == 403, b)
    s, b = leader.post(f"/teams/{tid}/invite-link", {"days": 3})
    check("only 1, 7, 14 or 30 days", s == 400, b)
    s, b = leader.post(f"/teams/{tid}/invite-link", {"days": 7})
    check("leader makes a link", s == 201 and b["path"].startswith("/invite/"), b)
    path = b["path"]
    token = path.split("/")[-1]
    check("token is long and random, no ids", len(token) >= 40 and str(tid) not in path.replace(token, ""), path)
    con = sqlite3.connect(DB)
    stored = [r[0] for r in con.execute("SELECT token_hash FROM team_invite_links")]
    con.close()
    check("only the token's hash is stored", token not in stored and len(stored[-1]) == 64, stored)
    s, b = leader.get(f"/teams/{tid}/invite-link")
    check("link status: active, no token shown again", s == 200 and b["active"] and "path" not in b, b)

    anon = Client()
    s, raw, _, headers = anon.req("GET", path)
    check("invite page served without sign-in, with CSP", s == 200 and b"invite.js" in raw
          and "default-src 'self'" in headers.get("Content-Security-Policy", ""), s)
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": token})
    check("preview without sign-in shows the team", s == 200 and b["team"]["name"] == "Razor"
          and b["team"]["requiredSkills"]["c++"] == 3 and b["team"]["leader"]["name"] == "Lena Leader", b)
    check("preview has no member list and no ids", "members" not in b["team"] and "id" not in b["team"], b)
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": token[:-2] + ("AA" if token[-2:] != "AA" else "BB")})
    check("an altered token is refused", s == 404, b)
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": "../../etc"})
    check("junk tokens refused", s == 404, b)
    s, _, b, _ = anon.req("POST", "/api/hub/invite-link/claim", {"token": token})
    check("claiming needs sign-in", s == 401, b)

    # sign-in round trip: /login?next= sends signed-in people back to the invitation only
    s, _, _, headers = newbie.req("GET", "/login?next=" + path, opener=newbie.raw)
    check("signed in: /login returns to the invitation", s in (302, 303) and headers.get("Location", "").endswith(path),
          (s, headers.get("Location")))
    s, _, _, headers = newbie.req("GET", "/login?next=https://evil.example/x", opener=newbie.raw)
    check("no open redirect", headers.get("Location", "").endswith("/app/"), headers.get("Location"))

    # a new person (no profile yet) opens the link, signs in, sets up a profile, applies
    s, b = newbie.post("/invite-link/claim", {"token": token})
    check("claim remembered", s == 200 and b["team"]["name"] == "Razor" and not b["member"], b)
    s, b = newbie.get("/summary")
    check("summary tells the app about the invitation", b["linkClaim"] == {"teamName": "Razor"}, b.get("linkClaim"))
    s, b = newbie.get("/invite-link/claim")
    check("claim details for the app (team needs, members' profile ids)",
          s == 200 and b["claim"]["team"]["name"] == "Razor" and isinstance(b["claim"]["team"]["memberStudentIds"], list), b)
    s, b = newbie.post("/invite-link/claim/apply", {"introduction": "Hi, I'm new", "reason": "Your project"})
    check("applying needs a profile first", s == 409 and b.get("code") == "profile_needed", b)
    newbie.profile("Nina New")
    s, b = newbie.post("/invite-link/claim/apply", {"introduction": "Hi, I'm new", "reason": "Your project"})
    check("application through the link", s == 201 and b["application"]["source"] == "link", b)
    s, b = newbie.get(f"/teams/{tid}")
    check("not a member yet: the leader decides", b["team"]["myRole"] is None, b["team"].get("myRole"))
    check("leader told it came through the link",
          any("through your invitation link" in x["body"] for x in notifications(leader)), notifications(leader)[:3])
    s, b = leader.get(f"/teams/{tid}/recruitment?filter=link")
    check("link applications listed separately", len(b["items"]) == 1 and b["items"][0]["source"] == "link"
          and b["items"][0]["person"]["name"] == "Nina New", b)
    s, b = newbie.get("/invite-link/claim")
    check("claim cleared after applying", b["claim"] is None, b)
    newbie.post("/invite-link/claim", {"token": token})
    s, b = newbie.post("/invite-link/claim/apply", {"introduction": "Again", "reason": "Again"})
    check("no duplicate application through the link", s == 409, b)
    s, b = leader.get(f"/teams/{tid}/invite-link")
    check("leader sees how often the link was opened", b["opened"] == 2 and b["applicationsViaLinks"] == 1, b)

    # regenerate and revoke
    s, b = leader.post(f"/teams/{tid}/invite-link", {"days": 14})
    new_token = b["path"].split("/")[-1]
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": token})
    check("regenerating stops the old link", s == 404, b)
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": new_token})
    check("the new link works", s == 200, b)
    s, b = newbie.get("/invite-link/claim")
    check("a claim on the old link no longer counts", b["claim"] is None and b["expired"], b)
    s, b = helper.post(f"/teams/{tid}/invite-link/revoke")
    check("revoke", s == 200 and not b["active"], b)
    s, _, b, _ = anon.req("POST", "/api/invite/preview", {"token": new_token})
    check("a revoked link is refused", s == 404, b)

    # another team's data is not reachable through a link or the candidate search
    s, b = leader.post("/teams", {"name": "Second team", "maxMembers": 2})
    other = b["team"]["id"]
    s, b = helper.get(f"/teams/{other}/candidates")
    check("an admin of one team cannot search for another", s == 403, b)
    s, b = helper.post(f"/teams/{other}/invite-link")
    check("...or make its links", s == 403, b)

    # removed members lose the team chat
    s, b = leader.post(f"/teams/{tid}/members/{bob.id}/remove")
    s, b = bob.get(f"/teams/{tid}/messages")
    check("a removed member loses the team chat", s == 403, b)
finally:
    srv.terminate()
    srv.wait()

passed = sum(1 for _, ok in results if ok)
print(f"\n{passed}/{len(results)} passed")
sys.exit(0 if passed == len(results) else 1)
