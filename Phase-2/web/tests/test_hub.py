"""API tests for the TeamForge Hub (web/hub.py through web/server.py).

Run from the repository root:  python web/tests/test_hub.py

Starts the real server on a temporary SQLite database and drives it with several accounts:
teams, chat (incl. long polling), team and event applications (duplicates, deadlines, capacity
under simultaneous approvals), anonymous requests (no identity in what recipients receive),
notifications, moderation and every permission check.
"""
import http.cookiejar, json, os, subprocess, sys, tempfile, threading, time, urllib.error, urllib.request
from pathlib import Path

WEB = str(Path(__file__).resolve().parent.parent)
PORT = 8767
BASE = f"http://127.0.0.1:{PORT}"
DB = tempfile.mktemp(suffix=".db")
results = []


def check(name, cond, detail=""):
    results.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))


def start():
    env = dict(os.environ, PORT=str(PORT), TEAMFORGE_SQLITE=DB, ADMIN_EMAILS="admin@test.edu", HOST="127.0.0.1")
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


def stop(p):
    p.terminate()
    p.wait()
    time.sleep(0.2)
    return b"".join(p.log).decode(errors="replace")


class Client:
    def __init__(self, email=None, password="longenough1"):
        self.jar = http.cookiejar.CookieJar()
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))
        self.email = email
        if email:
            s, _, b = self.req("POST", "/api/register", {"email": email, "password": password})
            assert s == 201, (s, b)
            self.id = self.req("GET", "/api/me")[2]["user"]["id"]

    def req(self, method, path, body=None, headers=None, timeout=40):
        h = {"X-TeamForge": "1", "Content-Type": "application/json"} if body is not None else {}
        h.update(headers or {})
        data = json.dumps(body).encode() if body is not None else None
        r = urllib.request.Request(BASE + path, data=data, method=method, headers=h)
        try:
            resp = self.op.open(r, timeout=timeout)
        except urllib.error.HTTPError as e:
            resp = e
        raw = resp.read()
        try:
            payload = json.loads(raw or b"{}")
        except ValueError:
            payload = raw
        return resp.status, raw, payload

    def get(self, path):
        s, _, b = self.req("GET", "/api/hub" + path)
        return s, b

    def post(self, path, body=None):
        s, _, b = self.req("POST", "/api/hub" + path, body if body is not None else {})
        return s, b

    def profile(self, name, **extra):
        s, b = self.post("/profile", dict({"name": name, "headline": f"{name}'s headline", "skills": ["c++", "qt"]}, **extra))
        assert s == 200, (s, b)


srv = start()
try:
    leader = Client("leader@test.edu")
    member = Client("member@test.edu")
    outsider = Client("outsider@test.edu")
    host = Client("host@test.edu")
    admin = Client("admin@test.edu")
    # admin makes `host` a host
    s, _, b = admin.req("POST", f"/api/admin/users/{host.id}/role", {"role": "host"})
    check("admin makes an account a host", s == 200, b)

    # ------------------------------------------------------------ sign-in and profiles
    s, _, b = Client().req("GET", "/api/hub/summary")
    check("hub needs sign-in", s == 401, s)
    s, b = leader.get("/profile")
    check("new account needs profile setup", s == 200 and b["needsSetup"] and b["profile"]["name"] == "New member", b)
    s, b = leader.post("/teams", {"name": "Rocket"})
    check("creating a team needs a profile first", s == 409 and b.get("code") == "profile_needed", (s, b))
    s, b = leader.post("/profile", {"name": "  "})
    check("profile name is required", s == 400, (s, b))
    s, b = leader.post("/profile", {"name": "Lee", "avatar": "data:image/png;base64,AAAA"})
    check("avatar bytes must match the type", s == 400, (s, b))
    png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="
    s, b = leader.post("/profile", {"name": "Lee Leader", "headline": "Team lead", "skills": "c++, qt, C++", "avatar": png})
    check("profile saved; duplicate skills dropped", s == 200 and b["profile"]["skills"] == ["c++", "qt"]
          and b["profile"]["avatar"] == png, b)
    member.profile("Mia Member")
    outsider.profile("Oscar Outsider")
    host.profile("Hana Host", acceptsAnonymous=True)
    admin.profile("Ada Admin", acceptsAnonymous=True)
    s, b = outsider.get(f"/people/{leader.id}")
    check("public profile has no email", s == 200 and b["person"]["name"] == "Lee Leader"
          and "leader@test.edu" not in json.dumps(b), b)

    # ------------------------------------------------------------ teams
    s, b = leader.post("/teams", {"name": "Rocket Lab", "tagline": "We build things", "objectives": "Win",
                                  "openRoles": ["Frontend", "QA"], "skills": ["qml"], "maxMembers": 3, "eventId": "req-001"})
    check("create team", s == 201 and b["team"]["myRole"] == "leader" and b["team"]["memberCount"] == 1
          and b["team"]["event"]["id"] == "req-001", (s, b))
    team = b["team"]["id"]
    s, b = outsider.post("/teams", {"name": "rocket  LAB"})
    check("team names are unique (ignoring case and spacing)", s == 409, (s, b))
    s, b = outsider.post("/teams", {"name": "Bad", "eventId": "nope"})
    check("unknown event refused", s == 400, (s, b))
    s, b = outsider.get("/teams?q=rocket")
    check("browse and search teams", s == 200 and b["total"] == 1 and b["items"][0]["leader"]["name"] == "Lee Leader", b)
    s, b = outsider.post(f"/teams/{team}", {"tagline": "hacked"})
    check("non-member cannot edit a team", s == 403, (s, b))

    # ------------------------------------------------------------ team applications
    s, b = outsider.post(f"/teams/{team}/applications", {"introduction": "Hi"})
    check("application needs a reason", s == 400, (s, b))
    app_body = {"introduction": "I'm Oscar", "skills": ["qml"], "experience": "2 apps", "interests": "UI",
                "reason": "Your project is cool"}
    s, b = outsider.post(f"/teams/{team}/applications", app_body)
    check("apply to a team", s == 201 and b["application"]["status"] == "pending", (s, b))
    oscar_app = b["application"]["id"]
    s, b = outsider.post(f"/teams/{team}/applications", app_body)
    check("duplicate pending application refused", s == 409, (s, b))
    s, b = leader.get("/notifications")
    check("leader notified of the application", any(n["kind"] == "team_application" for n in b["items"]), b)
    s, b = member.post(f"/teams/{team}/applications", app_body)
    mia_app = b["application"]["id"]
    s, b = outsider.post(f"/team-applications/{mia_app}/decision", {"decision": "approve"})
    check("applicant cannot approve applications", s == 403, (s, b))
    s, b = outsider.get(f"/teams/{team}/applications")
    check("non-manager cannot list applications", s == 403, (s, b))
    s, b = leader.get(f"/teams/{team}/applications?status=pending")
    check("leader lists pending applications with applicant profiles", s == 200 and b["total"] == 2
          and {i["applicant"]["name"] for i in b["items"]} == {"Oscar Outsider", "Mia Member"}, b)
    s, b = leader.get("/applications/received")
    check("unified received applications", s == 200 and b["total"] == 2, b)
    # two simultaneous approvals of the same application: exactly one wins
    answers = []
    threads = [threading.Thread(target=lambda: answers.append(leader.post(f"/team-applications/{mia_app}/decision",
                                                                          {"decision": "approve", "response": "Welcome"})[0]))
               for _ in range(2)]
    [t.start() for t in threads]
    [t.join() for t in threads]
    check("concurrent approvals: one succeeds, one is refused", sorted(answers) == [200, 409], answers)
    s, b = member.get(f"/teams/{team}")
    check("approved applicant is a member", b["team"]["myRole"] == "member" and b["team"]["memberCount"] == 2, b)
    s, b = member.get("/notifications")
    check("applicant notified of approval", any(n["kind"] == "team_application_approved" for n in b["items"]), b)
    s, b = outsider.post(f"/team-applications/{oscar_app}/withdraw")
    check("applicant withdraws a pending application", s == 200 and b["application"]["status"] == "withdrawn", (s, b))
    s, b = outsider.post(f"/team-applications/{oscar_app}/withdraw")
    check("cannot withdraw twice", s == 409, (s, b))
    s, b = outsider.post(f"/teams/{team}/applications", app_body)
    check("can apply again after withdrawing", s == 201, (s, b))
    oscar_app2 = b["application"]["id"]
    s, b = leader.post(f"/team-applications/{oscar_app2}/decision", {"decision": "reject", "response": "Full stack needed"})
    check("leader rejects with a response", s == 200 and b["application"]["status"] == "rejected", (s, b))
    s, b = outsider.get("/applications/mine")
    check("my applications show statuses and responses", s == 200 and {i["status"] for i in b["items"]} == {"withdrawn", "rejected"}
          and any(i["response"] == "Full stack needed" for i in b["items"]), b)

    # ------------------------------------------------------------ team chat
    s, b = outsider.get(f"/teams/{team}/messages")
    check("non-member cannot read the chat", s == 403, (s, b))
    s, b = outsider.post(f"/teams/{team}/messages", {"body": "let me in"})
    check("non-member cannot post", s == 403, (s, b))
    s, b = member.post(f"/teams/{team}/messages", {"body": "   "})
    check("empty message refused", s == 400, (s, b))
    s, b = member.post(f"/teams/{team}/messages", {"body": "x" * 2001})
    check("over-long message refused", s == 400, (s, b))
    s, b = member.post(f"/teams/{team}/messages", {"body": "Hello team!\u0007"})
    check("member posts a message (control characters removed)", s == 201 and b["message"]["body"] == "Hello team!"
          and b["message"]["mine"] and b["message"]["author"]["name"] == "Mia Member", (s, b))
    first = b["message"]["id"]
    s, b = leader.get("/conversations")
    check("conversation list shows unread and preview", b["items"][0]["unread"] == 1
          and b["items"][0]["lastMessage"]["body"] == "Hello team!", b)
    for i in range(35):
        member.req("POST", f"/api/hub/teams/{team}/messages", {"body": f"msg {i}"}) if i < 15 else \
            leader.req("POST", f"/api/hub/teams/{team}/messages", {"body": f"msg {i}"})
    s, b = leader.get(f"/teams/{team}/messages")
    check("newest page, in chronological order, with more available",
          len(b["messages"]) == 30 and b["hasMore"] and b["messages"][-1]["body"] == "msg 34"
          and [m["id"] for m in b["messages"]] == sorted(m["id"] for m in b["messages"]), b.get("hasMore"))
    s, b2 = leader.get(f"/teams/{team}/messages?before={b['messages'][0]['id']}")
    check("older page via ?before", not b2["hasMore"] and b2["messages"][0]["id"] == first and len(b2["messages"]) == 6,
          (len(b2["messages"]), b2["hasMore"]))
    s, b = leader.get("/notifications?unread=1")
    chat_notes = [n for n in b["items"] if n["kind"] == "team_message"]
    check("chat notifications are combined into one", len(chat_notes) == 1 and "new messages" in chat_notes[0]["title"], chat_notes)
    latest = leader.get(f"/teams/{team}/messages?limit=1")[1]["messages"][0]["id"]
    leader.post(f"/teams/{team}/read", {"lastId": latest})
    s, b = leader.get("/conversations")
    check("marking read clears unread", b["items"][0]["unread"] == 0, b)
    s, b = leader.get("/notifications?unread=1")
    check("marking read clears the chat notification", not any(n["kind"] == "team_message" for n in b["items"]), b)
    # long polling: a waiting reader receives a new message as soon as it is posted
    got = {}
    def wait_for_message():
        t0 = time.time()
        got["status"], got["body"] = leader.get(f"/teams/{team}/messages?after={latest}&wait=10")
        got["took"] = time.time() - t0
    waiter = threading.Thread(target=wait_for_message)
    waiter.start()
    time.sleep(0.8)
    member.post(f"/teams/{team}/messages", {"body": "live!"})
    waiter.join(15)
    check("long polling delivers a new message immediately", got.get("status") == 200
          and [m["body"] for m in got["body"]["messages"]] == ["live!"] and got["took"] < 5, got)
    t0 = time.time()
    s, b = leader.get(f"/teams/{team}/messages?after=999999&wait=1")
    check("long polling times out with no messages", s == 200 and b["messages"] == [] and time.time() - t0 >= 0.9, b)

    # ------------------------------------------------------------ member management
    s, b = member.post(f"/teams/{team}/members/{leader.id}/remove")
    check("member cannot remove others", s == 403, (s, b))
    s, b = leader.post(f"/teams/{team}/members/{member.id}/role", {"role": "admin"})
    check("leader makes a member admin", s == 200 and any(m["id"] == member.id and m["teamRole"] == "admin"
                                                          for m in b["team"]["members"]), (s, b))
    s, b = member.get(f"/teams/{team}/applications")
    check("team admin (delegated) can review applications", s == 200, (s, b))
    s, b = member.post(f"/teams/{team}/members/{leader.id}/remove")
    check("nobody can remove the leader", s == 403, (s, b))
    s, b = leader.post(f"/teams/{team}/leave")
    check("leader cannot leave without handing over", s == 409, (s, b))
    s, b = leader.post(f"/teams/{team}", {"maxMembers": 1})
    check("team size cannot drop below the member count", s == 400, (s, b))

    # ------------------------------------------------------------ events
    s, b = outsider.get("/events")
    check("events are the app's projects", s == 200 and b["total"] == 8, b.get("total"))
    s, b = outsider.post("/events/req-001/settings", {"capacity": 1})
    check("participants cannot open events", s == 403, (s, b))
    s, b = outsider.post("/events/req-001/applications", {"motivation": "hi"})
    check("cannot apply before the event is opened", s == 409, (s, b))
    s, b = host.post("/events/req-001/settings", {"description": "Big hackathon", "eligibility": "2nd year",
                                                  "positions": ["Developer", "Designer"], "capacity": 1,
                                                  "deadline": time.time() + 3600})
    check("host opens an event and becomes organiser", s == 200 and b["event"]["canManage"] and b["event"]["open"]
          and b["event"]["organizer"]["name"] == "Hana Host", (s, b))
    s, b = admin.post("/events/req-001/settings", {"description": "Big hackathon!", "positions": ["Developer", "Designer"],
                                                   "capacity": 1, "deadline": time.time() + 3600})
    check("admin may edit any event", s == 200, (s, b))
    s, b = outsider.post("/events/req-001/applications", {"motivation": "I love hackathons", "position": "Chef"})
    check("position must be one of the event's", s == 400, (s, b))
    s, b = outsider.post("/events/req-001/applications", {"motivation": "I love hackathons", "position": "Developer"})
    check("apply to an event", s == 201 and b["application"]["status"] == "pending", (s, b))
    o_ev = b["application"]["id"]
    s, b = outsider.post("/events/req-001/applications", {"motivation": "again", "position": "Developer"})
    check("duplicate event application refused", s == 409, (s, b))
    s, b = member.post("/events/req-001/applications", {"motivation": "Me too", "position": "Designer"})
    m_ev = b["application"]["id"]
    s, b = leader.post("/events/req-001/applications", {"motivation": "Count me in", "position": "Developer"})
    l_ev = b["application"]["id"]
    s, b = outsider.get("/events/req-001/applications")
    check("applicants cannot list event applications", s == 403, (s, b))
    s, b = host.post(f"/event-applications/{o_ev}/decision", {"decision": "maybe"})
    check("unknown decision refused", s == 400, (s, b))
    # capacity 1, two simultaneous approvals of different applicants: exactly one is approved
    answers = []
    threads = [threading.Thread(target=lambda a=a: answers.append(host.post(f"/event-applications/{a}/decision",
                                                                            {"decision": "approve"})[0]))
               for a in (o_ev, m_ev)]
    [t.start() for t in threads]
    [t.join() for t in threads]
    s, b = host.get("/events/req-001/applications")
    check("capacity holds under simultaneous approvals", sorted(answers) == [200, 409]
          and b["counts"].get("approved") == 1, (answers, b.get("counts")))
    loser = m_ev if answers and host.get("/events/req-001/applications?status=pending")[1]["total"] and \
        any(i["id"] == m_ev for i in host.get("/events/req-001/applications?status=pending")[1]["items"]) else o_ev
    s, b = host.post(f"/event-applications/{loser}/decision", {"decision": "waitlist", "feedback": "Full for now"})
    check("organiser waitlists when full", s == 200 and b["application"]["status"] == "waitlisted", (s, b))
    s, b = host.post(f"/event-applications/{l_ev}/decision", {"decision": "reject", "feedback": "Sorry"})
    check("organiser rejects with feedback", s == 200, (s, b))
    s, b = host.post(f"/event-applications/{l_ev}/decision", {"decision": "approve"})
    check("a rejected application cannot be approved", s == 409, (s, b))
    late = Client("late@test.edu")
    late.profile("Lara Late")
    s, b = late.post("/events/req-001/applications", {"motivation": "Late but keen", "position": "Designer"})
    check("applying to a full event joins the waitlist", s == 201 and b["application"]["status"] == "waitlisted", (s, b))
    winner = o_ev if loser == m_ev else m_ev
    winner_client = outsider if winner == o_ev else member
    s, b = winner_client.post(f"/event-applications/{winner}/withdraw")
    check("approved participant withdraws before the deadline", s == 200, (s, b))
    s, b = host.get("/notifications")
    check("organiser told a place opened up", any(n["kind"] == "event_place_freed" for n in b["items"]), b)
    s, b = host.post(f"/event-applications/{loser}/decision", {"decision": "approve"})
    check("waitlisted applicant approved once a place is free", s == 200, (s, b))
    s, b = host.post("/events/req-001/settings", {"capacity": 1, "positions": ["Developer", "Designer"],
                                                  "deadline": time.time() - 10})
    s, b = late.post("/events/req-002/applications", {"motivation": "x"})
    check("event without settings is not open", s == 409, (s, b))
    newbie = Client("newbie@test.edu")
    newbie.profile("Nia New")
    s, b = newbie.post("/events/req-001/applications", {"motivation": "x", "position": "Developer"})
    check("deadline respected", s == 409 and "deadline" in b["error"], (s, b))
    s, b = host.post("/events/req-001/settings", {"capacity": None, "positions": [], "accepting": False})
    s, b = newbie.get("/events/req-001")
    check("closed event says why", not b["event"]["open"] and b["event"]["closedReason"] == "Applications are closed.", b)
    s, b = member.post("/events/req-001/settings", {"capacity": 5})
    check("another account cannot take over an event", s == 403, (s, b))

    # ------------------------------------------------------------ anonymous connections
    s, b = outsider.post("/anonymous", {"targetKind": "team", "targetId": str(team), "body": "Are you hiring a tester?"})
    check("send an anonymous message to a team", s == 201, (s, b))
    thread = b["thread"]["id"]
    s, b = outsider.post("/anonymous", {"targetKind": "team", "targetId": str(team), "body": "again"})
    check("one open conversation per recipient", s == 409 and b.get("threadId") == thread, (s, b))
    s, b = member.post("/anonymous", {"targetKind": "team", "targetId": str(team), "body": "me"})
    check("cannot message your own team anonymously", s == 400, (s, b))
    s, b = outsider.post("/anonymous", {"targetKind": "person", "targetId": str(member.id), "body": "hi"})
    check("people who did not opt in cannot be messaged", s == 404, (s, b))
    s, raw_inbox, inbox = leader.req("GET", "/api/hub/anonymous/inbox")
    s2, raw_thread, tview = leader.req("GET", f"/api/hub/anonymous/{thread}")
    s3, _, notes = leader.req("GET", "/api/hub/notifications")
    raw_notes = json.dumps([n for n in notes["items"] if n["kind"].startswith("anonymous")]).encode()
    leaked = [x for x in ("outsider@test.edu", "Oscar", "sender") if x.encode() in raw_inbox + raw_thread + raw_notes]
    check("recipient sees the message without the sender's identity", s == 200 and s2 == 200
          and inbox["items"][0]["preview"] == "Are you hiring a tester?" and not leaked
          and tview["thread"]["messages"][0]["author"]["name"] == "Anonymous"
          and f'"id": {outsider.id},' not in raw_thread.decode(), (leaked, tview))
    s, b = member.get(f"/anonymous/{thread}")
    check("team admin also receives team requests", s == 200 and b["thread"]["viewer"] == "recipient", (s, b))
    s, b = late.get(f"/anonymous/{thread}")
    check("strangers get 404 for a conversation", s == 404, (s, b))
    s, b = leader.post(f"/anonymous/{thread}/messages", {"body": "Yes! Tell us more."})
    check("recipient replies", s == 201, (s, b))
    s, b = outsider.get("/anonymous/sent")
    check("sender sees status and an unread reply", s == 200 and b["items"][0]["unread"] and b["items"][0]["status"] == "reviewed", b)
    s, b = outsider.get(f"/anonymous/{thread}")
    check("sender sees the reply with the recipient's name", b["thread"]["messages"][1]["author"]["name"] == "Lee Leader"
          and b["thread"]["messages"][0]["author"]["name"] == "You", b)
    s, b = outsider.post(f"/anonymous/{thread}/status", {"status": "interested"})
    check("sender cannot change the status", s == 403, (s, b))
    s, b = leader.post(f"/anonymous/{thread}/invite")
    check("recipient invites the sender to apply", s == 200, (s, b))
    s, b = outsider.get("/notifications")
    check("sender gets the invitation with a link to apply", any(n["kind"] == "invitation" and n["link"] == f"#/teams/{team}?apply=1"
                                                                for n in b["items"]), b)
    s, b = outsider.post("/anonymous", {"targetKind": "event", "targetId": "req-001", "body": "Is there a fee?"})
    check("anonymous message to an event organiser", s == 201, (s, b))
    ev_thread = b["thread"]["id"]
    s, b = host.get("/anonymous/inbox")
    check("organiser inbox", s == 200 and b["items"][0]["id"] == ev_thread and b["items"][0]["target"]["kind"] == "event", b)
    s, b = outsider.post("/anonymous", {"targetKind": "person", "targetId": str(admin.id), "body": "Found a bug"})
    check("anonymous message to a developer who accepts them", s == 201, (s, b))
    dev_thread = b["thread"]["id"]
    s, b = admin.post(f"/anonymous/{dev_thread}/report", {"reason": "Spam"})
    check("recipient reports a conversation", s == 201, (s, b))
    s, b = admin.post(f"/anonymous/{dev_thread}/report", {"reason": "Spam"})
    check("duplicate report refused", s == 409, (s, b))
    s, b = host.get("/admin/reports")
    check("non-admins cannot see reports", s == 403, (s, b))
    s, b = admin.get("/admin/reports")
    check("admins see reports with the retained sender account", s == 200 and b["items"][0]["senderEmail"] == "outsider@test.edu", b)
    s, b = admin.post(f"/admin/reports/{b['items'][0]['id']}/resolve", {"action": "block"})
    check("admin blocks the sender for that recipient", s == 200, (s, b))
    s, b = outsider.post(f"/anonymous/{dev_thread}/messages", {"body": "hello?"})
    check("blocked conversation is closed", s == 409, (s, b))
    s, b = outsider.post("/anonymous", {"targetKind": "person", "targetId": str(admin.id), "body": "again"})
    check("blocked sender cannot start a new conversation", s == 403, (s, b))
    s, b = host.post(f"/anonymous/{ev_thread}/block")
    check("recipient blocks a sender", s == 200 and b["status"] == "declined", (s, b))
    spammer = Client("spam@test.edu")
    codes = [spammer.post("/anonymous", {"targetKind": "person", "targetId": str(t), "body": "hi"})[0]
             for t in (host.id, admin.id)]
    for i in range(5):
        spammer.req("POST", "/api/hub/teams", {"name": f"x{i}"})
    extra = [spammer.post("/anonymous", {"targetKind": "team", "targetId": str(team), "body": "hi"})[0]]
    check("anonymous messages need no profile; repeats are limited", codes == [201, 201] and extra == [201], (codes, extra))

    # ------------------------------------------------------------ notifications and summary
    s, b = outsider.get("/notifications/count")
    unread = b["unread"]
    s, b = outsider.get("/notifications")
    s, b2 = outsider.post("/notifications/read", {"ids": [b["items"][0]["id"]]})
    check("mark one notification read", s == 200 and b2["unread"] == unread - 1, (unread, b2))
    s, b2 = outsider.post("/notifications/read", {"ids": "all"})
    check("bad mark-read input refused", s == 400, (s, b2))
    s, b2 = outsider.post("/notifications/read", {"all": True})
    check("mark all read", b2["unread"] == 0, b2)
    s, b = leader.get("/summary")
    check("summary for the dashboard", s == 200 and "pendingDecisions" in b and b["profile"]["name"] == "Lee Leader", b)

    # ------------------------------------------------------------ write protection
    s, _, b = outsider.req("POST", "/api/hub/teams", {"name": "CSRF"}, headers={"X-TeamForge": "0"})
    check("hub writes need the TeamForge header", s == 403, (s, b))
    s, b = leader.post(f"/teams/{team}/delete", {"confirm": "wrong"})
    check("deleting a team needs its name", s == 400, (s, b))
    s, b = member.post(f"/teams/{team}/delete", {"confirm": "Rocket Lab"})
    check("only the leader deletes a team", s == 403, (s, b))
    s, b = leader.post(f"/teams/{team}/delete", {"confirm": "Rocket Lab"})
    check("leader deletes the team", s == 200, (s, b))
    s, b = member.get(f"/teams/{team}/messages")
    check("deleted team's chat is gone", s == 404, (s, b))
finally:
    log_text = stop(srv)
check("no server errors logged", "Traceback" not in log_text, log_text[-2000:])

# the schema is created again on an existing database without losing data
srv = start()
try:
    c = Client()
    s, _, b = c.req("POST", "/api/login", {"email": "leader@test.edu", "password": "longenough1"})
    s, _, b = c.req("GET", "/api/hub/profile")
    check("hub data survives a restart", b["profile"]["name"] == "Lee Leader", b)
finally:
    stop(srv)
for leftover in Path(DB).parent.glob(Path(DB).name + "*"):
    try:
        leftover.unlink()
    except OSError:
        pass
print(f"\n{sum(ok for _, ok in results)}/{len(results)} passed")
sys.exit(0 if all(ok for _, ok in results) else 1)
