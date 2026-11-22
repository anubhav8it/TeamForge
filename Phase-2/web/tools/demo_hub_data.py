#!/usr/bin/env python3
"""Fills a LOCAL TeamForge server with demo Community data (accounts, teams, chat, applications,
an open event, anonymous requests), for demos, screenshots and tests/tst_community.

    python web/server.py                       # in one terminal (ADMIN_EMAILS=dev@demo.test)
    python web/tools/demo_hub_data.py http://localhost:8000

Only localhost servers are accepted: the accounts use a shared demo password (printed at the end).
Run it on a fresh database; it stops if the demo accounts already exist.
"""
from __future__ import annotations

import http.cookiejar
import json
import sys
import time
import urllib.error
import urllib.request
from urllib.parse import urlsplit

PASSWORD = "demo-password-1"
PEOPLE = {
    "lee": ("lee@demo.test", "Lee Sharma", "Team lead · C++ and Qt", ["c++", "qt", "git"]),
    "mia": ("mia@demo.test", "Mia Rawat", "Frontend developer", ["qml", "ui design", "javascript"]),
    "oscar": ("oscar@demo.test", "Oscar Negi", "Tester who likes breaking things", ["testing", "python"]),
    "hana": ("hana@demo.test", "Hana Bisht", "Hackathon organiser", ["management", "documentation"]),
    "dev": ("dev@demo.test", "Dev Admin", "TeamForge developer", ["python", "c++", "sql"]),
}


class Client:
    def __init__(self, base: str):
        self.base = base
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

    def call(self, method: str, path: str, body=None):
        data = json.dumps(body).encode() if body is not None else None
        headers = {"X-TeamForge": "1", "Content-Type": "application/json"} if body is not None else {}
        request = urllib.request.Request(self.base + path, data=data, method=method, headers=headers)
        try:
            response = self.opener.open(request, timeout=30)
        except urllib.error.HTTPError as error:
            response = error
        payload = json.loads(response.read() or b"{}")
        if response.status >= 400:
            raise SystemExit(f"{method} {path} -> {response.status}: {payload.get('error')}")
        return payload


def main() -> None:
    base = (sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8000").rstrip("/")
    if urlsplit(base).hostname not in ("localhost", "127.0.0.1"):
        raise SystemExit("Demo data goes only to a local server (localhost / 127.0.0.1).")
    c = {}
    for key, (email, name, headline, skills) in PEOPLE.items():
        c[key] = Client(base)
        c[key].call("POST", "/api/register", {"email": email, "password": PASSWORD})
        c[key].call("POST", "/api/hub/profile", {"name": name, "headline": headline, "skills": skills,
                                                 "bio": f"Hi, I'm {name.split()[0]}.",
                                                 "acceptsAnonymous": key in ("dev", "hana")})
    users = c["dev"].call("GET", "/api/admin/users")["users"]
    hana_id = next(u["id"] for u in users if u["email"] == PEOPLE["hana"][0])
    dev_id = next(u["id"] for u in users if u["email"] == PEOPLE["dev"][0])
    c["dev"].call("POST", f"/api/admin/users/{hana_id}/role", {"role": "host"})

    team = c["lee"].call("POST", "/api/hub/teams", {
        "name": "Rocket Lab", "tagline": "A campus app that helps students find project partners",
        "description": "We are building CampusConnect for the hackathon: Qt front end, C++ core, tested properly.",
        "objectives": "Ship a working prototype in 48 hours and win the people's choice award.",
        "openRoles": ["Frontend", "Tester"], "skills": ["c++", "qt", "qml"], "maxMembers": 4,
        "eventId": "req-001"})["team"]
    c["lee"].call("POST", "/api/hub/teams", {"name": "Data Divers", "tagline": "Machine learning on campus data",
                                             "openRoles": ["ML engineer"], "skills": ["python", "ml"], "maxMembers": 5})
    c["hana"].call("POST", "/api/hub/teams", {"name": "Pixel Pushers", "tagline": "Design-first web apps",
                                              "openRoles": ["Designer", "Frontend"], "maxMembers": 3, "recruiting": False})
    tid = team["id"]
    app = c["mia"].call("POST", f"/api/hub/teams/{tid}/applications", {
        "introduction": "I'm Mia, I build QML interfaces.", "skills": ["qml", "ui design"],
        "experience": "Two Qt apps for college clubs.", "interests": "UI, accessibility",
        "reason": "Your project needs a frontend and I love campus tools."})["application"]
    c["lee"].call("POST", f"/api/hub/team-applications/{app['id']}/decision", {"decision": "approve", "response": "Welcome aboard!"})
    c["oscar"].call("POST", f"/api/hub/teams/{tid}/applications", {
        "introduction": "Oscar here, I test everything.", "skills": ["testing", "python"],
        "experience": "QA intern for a summer.", "reason": "I want to keep your demo from crashing on stage."})
    for author, text in [("lee", "Welcome Mia! Kick-off call tonight at 8?"), ("mia", "Sounds good. I'll sketch the screens first."),
                         ("lee", "Great. I'll push the C++ core skeleton today."), ("mia", "Pushed the login screen mock-up.\nFeedback welcome!")]:
        c[author].call("POST", f"/api/hub/teams/{tid}/messages", {"body": text})
        time.sleep(0.05)

    c["hana"].call("POST", "/api/hub/events/req-001/settings", {
        "description": "A 48-hour hackathon on smart campus collaboration.", "eligibility": "B.Tech students, any year",
        "requirements": "Teams of 3-4; bring a laptop.", "positions": ["Developer", "Designer", "Tester"],
        "capacity": 3, "deadline": time.time() + 5 * 86400})
    c["oscar"].call("POST", "/api/hub/events/req-001/applications", {"position": "Tester", "motivation": "I love hackathons.",
                                                                     "availability": "Both days"})
    mine = c["mia"].call("POST", "/api/hub/events/req-001/applications", {"position": "Designer", "motivation": "Design sprints are my thing."})
    c["hana"].call("POST", f"/api/hub/event-applications/{mine['application']['id']}/decision", {"decision": "approve", "feedback": "See you there!"})

    thread = c["oscar"].call("POST", "/api/hub/anonymous", {"targetKind": "team", "targetId": str(tid),
                                                             "body": "Do you still need a tester? I can only join on weekends."})["thread"]
    c["lee"].call("POST", f"/api/hub/anonymous/{thread['id']}/messages", {"body": "Weekends work for us. Tell us what you've tested before?"})
    c["mia"].call("POST", "/api/hub/anonymous", {"targetKind": "person", "targetId": str(dev_id),
                                                  "body": "Is there an API for exporting team data?"})
    c["oscar"].call("POST", "/api/hub/anonymous", {"targetKind": "event", "targetId": "req-001", "body": "Is there a registration fee?"})
    print(f"Demo data created on {base}. Accounts (password {PASSWORD}): " + ", ".join(p[0] for p in PEOPLE.values()))
    print(f"TEAM_ID={tid} THREAD_ID={thread['id']}")


if __name__ == "__main__":
    main()
