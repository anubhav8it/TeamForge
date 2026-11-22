"""Team recruitment: finding members, direct invitations and shareable invitation links.

Part of the Community hub (web/hub.py mixes this in). Every rule is checked here on the server:
who may search, invite or generate links (the team's leader and admins), team capacity (checked
again inside the transaction that adds a member), duplicate invitations and applications, expiry,
revocation. Nobody joins a team without a person deciding: a direct invitation adds the person
when *they* accept it (the leader chose them), and an invitation link only leads to an
application that the leader approves or rejects.

Compatibility scores are not computed here: the app computes them with the C++ matching engine
(src/core/Compatibility.h) from the same profile data, so there is one scoring algorithm.

Invitation links: the token is 32 random bytes (secrets.token_urlsafe), shown once to the
leader; only its SHA-256 is stored. Links expire, can be revoked and regenerated (which revokes
the old one), and never contain a database id.
"""
import hashlib
import json
import re
import secrets

INVITE_DAYS = 14                 # a direct invitation stays open this long
LINK_DAYS = (1, 7, 14, 30)       # how long an invitation link may stay valid
DEFAULT_LINK_DAYS = 14
MAX_CANDIDATES = 200             # search results returned at once (the app ranks and pages them)
MAX_LINKS = 3                    # portfolio links on a profile
TOKEN_RE = re.compile(r"[A-Za-z0-9_\-]{20,64}")
INVITATION_STATUSES = ("pending", "accepted", "declined", "expired", "revoked")
DAY = 86400.0


def _hash(token: str) -> str:
    return hashlib.sha256(token.encode("ascii")).hexdigest()


def normalize_skill(name) -> str:
    """Same key as the C++ normalizeSkill: trimmed, inner spaces collapsed, lower case."""
    return re.sub(r"\s+", " ", str(name or "")).strip().lower()


def create_schema(db, pk: str, real: str, ensure_column) -> None:
    for sql in [
        f"""CREATE TABLE IF NOT EXISTS team_invitations (
            id {pk}, team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            invited_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            message TEXT NOT NULL DEFAULT '', status TEXT NOT NULL, response TEXT NOT NULL DEFAULT '',
            created_at {real} NOT NULL, expires_at {real} NOT NULL, decided_at {real})""",
        # One open invitation per person per team, enforced by the database itself.
        """CREATE UNIQUE INDEX IF NOT EXISTS uq_team_invitations_pending
            ON team_invitations (team_id, user_id) WHERE status = 'pending'""",
        "CREATE INDEX IF NOT EXISTS idx_team_invitations_user ON team_invitations (user_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_team_invitations_team ON team_invitations (team_id, status)",
        f"""CREATE TABLE IF NOT EXISTS team_invite_links (
            id {pk}, team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
            token_hash TEXT NOT NULL UNIQUE, created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            created_at {real} NOT NULL, expires_at {real} NOT NULL, revoked_at {real},
            uses INTEGER NOT NULL DEFAULT 0)""",
        "CREATE INDEX IF NOT EXISTS idx_team_invite_links_team ON team_invite_links (team_id)",
        # Someone opened a link and signed in: remembered on the server, so the invitation is still
        # there after registration and onboarding (and on another device).
        f"""CREATE TABLE IF NOT EXISTS link_claims (
            user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
            link_id INTEGER NOT NULL REFERENCES team_invite_links(id) ON DELETE CASCADE,
            created_at {real} NOT NULL)""",
    ]:
        db.execute(sql)
    ensure_column(db, "teams", "required_skills", "TEXT NOT NULL DEFAULT '{}'")
    ensure_column(db, "profiles", "links", "TEXT NOT NULL DEFAULT '[]'")
    ensure_column(db, "profiles", "open_to_invites", "INTEGER NOT NULL DEFAULT 1")
    ensure_column(db, "team_applications", "source", "TEXT NOT NULL DEFAULT 'direct'")


TABLES = {
    "team_invitations": ["id", "team_id", "user_id", "invited_by", "message", "status", "response", "created_at",
                         "expires_at", "decided_at"],
    "team_invite_links": ["id", "team_id", "token_hash", "created_by", "created_at", "expires_at", "revoked_at",
                          "uses"],
    "link_claims": ["user_id", "link_id", "created_at"],
}


def clean_required_skills(value, clean_text, error) -> dict:
    """{skill: minimum level 1-5}, at most 15 skills. A list (older apps) means any level (1)."""
    if value is None:
        return {}
    if isinstance(value, list):
        value = {name: 1 for name in value}
    if not isinstance(value, dict):
        raise error(400, "Required skills must be a list of skills with levels.")
    if len(value) > 15:
        raise error(400, "Required skills: at most 15.")
    out = {}
    for name, level in value.items():
        key = normalize_skill(clean_text(name, "Skill", 40, required=True, multiline=False))
        if not isinstance(level, int) or isinstance(level, bool) or not 1 <= level <= 5:
            raise error(400, f"Required level for {key} must be 1 to 5.")
        out[key] = level
    return out


def clean_links(value, error) -> list[str]:
    """Portfolio / GitHub links: up to three https:// addresses."""
    if value is None or value == "":
        return []
    if not isinstance(value, list) or len(value) > MAX_LINKS:
        raise error(400, f"Add at most {MAX_LINKS} links.")
    out = []
    for link in value:
        link = str(link or "").strip()
        if not link:
            continue
        if len(link) > 200 or not re.fullmatch(r"https://[A-Za-z0-9.\-]+(:\d+)?(/[^\s<>\"']*)?", link):
            raise error(400, "Links must be full https:// addresses (for example https://github.com/you).")
        out.append(link)
    return out


class RecruitmentMixin:
    """Methods of Hub (web/hub.py); `self.db`, `self.notify`, `self.profiles` etc. come from there."""

    def recruitment_routes(self):
        return [
            ("GET", r"/teams/(\d+)/candidates", "team_candidates"),
            ("GET", r"/teams/(\d+)/recruitment", "team_recruitment"),
            ("POST", r"/teams/(\d+)/invitations", "invite_member"),
            ("POST", r"/team-invitations/(\d+)/revoke", "revoke_invitation"),
            ("POST", r"/team-invitations/(\d+)/respond", "respond_invitation"),
            ("GET", r"/invitations/mine", "my_invitations"),
            ("GET", r"/teams/(\d+)/invite-link", "invite_link_status"),
            ("POST", r"/teams/(\d+)/invite-link", "generate_invite_link"),
            ("POST", r"/teams/(\d+)/invite-link/revoke", "revoke_invite_link"),
            ("POST", r"/invite-link/claim", "claim_invite_link"),
            ("GET", r"/invite-link/claim", "get_link_claim"),
            ("POST", r"/invite-link/claim/apply", "apply_via_link"),
            ("POST", r"/invite-link/claim/dismiss", "dismiss_link_claim"),
        ]

    # ------------------------------------------------------------------ shared helpers
    def _required_skills(self, row) -> dict:
        """The team's required skills with minimum levels. Teams made before levels existed have a
        plain list: those skills count at any level (1)."""
        stored = self._loads_json(row[11] if len(row) > 11 else None, {})
        if isinstance(stored, dict) and stored:
            return {normalize_skill(k): int(v) for k, v in stored.items()}
        return {normalize_skill(s): 1 for s in self._loads_json(row[6], [])}

    @staticmethod
    def _loads_json(text, default):
        try:
            value = json.loads(text) if text else default
        except (TypeError, ValueError):
            return default
        return value if isinstance(value, type(default)) else default

    def _students_by_id(self) -> dict:
        row = self.db.one("SELECT content FROM shared_files WHERE name = 'students.json'")
        students = self._loads_json(row[0], {}).get("students", []) if row else []
        return {s.get("id"): s for s in students if isinstance(s, dict) and s.get("id")}

    def _expire_invitations(self) -> None:
        """Pending invitations past their date become expired; the invited person is told once."""
        stamp = self._now()
        rows = self.db.all("""SELECT i.id, i.user_id, t.name, t.id FROM team_invitations i JOIN teams t ON t.id = i.team_id
                              WHERE i.status = 'pending' AND i.expires_at < ?""", (stamp,))
        if not rows:
            return
        with self.db.transaction():
            for inv_id, user_id, team_name, team_id in rows:
                if self.db.execute("UPDATE team_invitations SET status = 'expired', decided_at = ? "
                                   "WHERE id = ? AND status = 'pending'", (stamp, inv_id)) == 1:
                    self.notify(user_id, "team_invitation_expired", f"Your invitation to {team_name} expired",
                                "Apply to the team if you are still interested.", f"#/teams/{team_id}")

    def _invitation_dict(self, r, people) -> dict:
        (inv_id, team_id, user_id, invited_by, message, status, response, created_at, expires_at, decided_at,
         team_name) = r
        return {"id": inv_id, "kind": "invitation", "source": "invitation", "teamId": team_id,
                "targetName": team_name, "person": people.get(user_id), "invitedBy": people.get(invited_by),
                "message": message, "status": status, "response": response, "createdAt": created_at,
                "expiresAt": expires_at, "decidedAt": decided_at, "link": f"#/teams/{team_id}"}

    _INVITATION_SELECT = ("SELECT i.id, i.team_id, i.user_id, i.invited_by, i.message, i.status, i.response, "
                          "i.created_at, i.expires_at, i.decided_at, t.name FROM team_invitations i "
                          "JOIN teams t ON t.id = i.team_id")

    def _member_student_ids(self, team_id) -> list[str]:
        """The members' participant profile ids (for signed-in people who were invited), so the app
        can work out how someone would complement the team. Not part of the public link preview."""
        return [r[0] for r in self.db.all(
            """SELECT p.student_id FROM team_members m JOIN profiles p ON p.user_id = m.user_id
               WHERE m.team_id = ? AND p.student_id IS NOT NULL AND p.student_id <> ''""", (int(team_id),))]

    def _public_team(self, row) -> dict:
        """What an invitation (link or direct) shows about a team: no members' details, no ids."""
        members = self._member_count(row[0])
        leader = self.db.one("SELECT user_id FROM team_members WHERE team_id = ? AND role = 'leader'", (row[0],))
        leader_profile = self.profiles([leader[0]]).get(leader[0]) if leader else None
        return {
            "name": row[1], "tagline": row[2], "description": (row[3] or "")[:600], "objectives": (row[4] or "")[:600],
            "openRoles": self._loads_json(row[5], []), "requiredSkills": self._required_skills(row),
            "recruiting": bool(row[8]), "maxMembers": row[9], "memberCount": members,
            "vacancies": max(0, row[9] - members), "full": members >= row[9],
            "leader": {"name": leader_profile["name"], "color": leader_profile["color"]} if leader_profile else None,
        }

    # ------------------------------------------------------------------ find members
    def team_candidates(self, user, team_id, q, body):
        """Registered people a team leader or admin can invite: they have a profile, they have not
        turned "open to team invitations" off, and they are not in the team already. Searched by
        name, headline, about, skills (with levels from their participant profile), experience,
        education, projects and interests. Returned unranked; the app scores and sorts them."""
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        needle = ""
        if q.get("q"):
            needle = self._clean_text(q["q"], "Search", 80, multiline=False).lower()
        terms = [t for t in re.split(r"[\s,]+", needle) if t]
        students = self._students_by_id()
        rows = self.db.all(
            f"""{self._profile_select()} WHERE p.display_name <> '' AND COALESCE(p.open_to_invites, 1) = 1
                AND u.id <> ? AND u.id NOT IN (SELECT user_id FROM team_members WHERE team_id = ?)
                ORDER BY LOWER(p.display_name)""", (user["id"], row[0]))
        found = []
        for r in rows:
            person = self._profile_from_row(r, full=True)
            student = students.get(person["studentId"]) or {}
            details = student.get("details") if isinstance(student.get("details"), dict) else student
            haystack = " ".join(str(x) for x in [
                person["name"], person["headline"], person.get("bio", ""), " ".join(person.get("skills", [])),
                person.get("experience", ""), person.get("interests", ""), person.get("program", ""),
                " ".join(person.get("links", [])),
                " ".join((student.get("skillsOffered") or {}).keys()), student.get("role", ""),
                student.get("summary", ""), student.get("program", ""),
                " ".join(details.get("projects") or []), " ".join(details.get("education") or []),
                " ".join(details.get("certifications") or []),
            ]).lower()
            if all(t in haystack for t in terms):
                found.append(person)
        truncated = len(found) > MAX_CANDIDATES
        found = found[:MAX_CANDIDATES]
        ids = [p["id"] for p in found]
        if ids:
            marks = ", ".join("?" for _ in ids)
            teams = {}
            for uid, t_id, name in self.db.all(
                    f"""SELECT m.user_id, t.id, t.name FROM team_members m JOIN teams t ON t.id = m.team_id
                        WHERE m.user_id IN ({marks}) ORDER BY t.name""", tuple(ids)):
                teams.setdefault(uid, []).append({"id": t_id, "name": name})
            invited = {r[0]: r[1] for r in self.db.all(
                f"SELECT user_id, id FROM team_invitations WHERE team_id = ? AND status = 'pending' AND user_id IN ({marks})",
                (row[0], *ids))}
            applied = {r[0]: r[1] for r in self.db.all(
                f"SELECT user_id, id FROM team_applications WHERE team_id = ? AND status = 'pending' AND user_id IN ({marks})",
                (row[0], *ids))}
            for p in found:
                p["teams"] = teams.get(p["id"], [])
                p["pendingInvitation"] = invited.get(p["id"])
                p["pendingApplication"] = applied.get(p["id"])
        return {"items": found, "total": len(found), "truncated": truncated}

    # ------------------------------------------------------------------ direct invitations
    def invite_member(self, user, team_id, q, body):
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        target = body.get("userId")
        if not isinstance(target, int) or isinstance(target, bool):
            raise self._error(400, "Choose who to invite.")
        message = self._clean_text(body.get("message"), "Message", 600)
        if target == user["id"]:
            raise self._error(400, "You are already in this team.")
        profile = self.db.one("SELECT display_name, COALESCE(open_to_invites, 1) FROM profiles WHERE user_id = ?", (target,))
        if not profile or not profile[0]:
            raise self._error(404, "No such person.")
        if not profile[1]:
            raise self._error(403, "This person is not accepting team invitations.")
        self._take("invite", user, "You have sent many invitations in the last hour. Try again later.")
        stamp = self._now()
        try:
            with self.db.transaction():
                if self._team_role(row[0], target):
                    raise self._error(409, "This person is already in the team.")
                if self._member_count(row[0]) >= row[9]:
                    raise self._error(409, "The team is full. Raise the team size or remove a member first.")
                if self.db.one("SELECT 1 FROM team_invitations WHERE team_id = ? AND user_id = ? AND status = 'pending'",
                               (row[0], target)):
                    raise self._error(409, "This person already has an open invitation to the team.")
                if self.db.one("SELECT 1 FROM team_applications WHERE team_id = ? AND user_id = ? AND status = 'pending'",
                               (row[0], target)):
                    raise self._error(409, "This person has already applied: review their application instead.",
                                      code="already_applied")
                inv_id = self.db.insert(
                    """INSERT INTO team_invitations (team_id, user_id, invited_by, message, status, created_at, expires_at)
                       VALUES (?, ?, ?, ?, 'pending', ?, ?)""",
                    (row[0], target, user["id"], message, stamp, stamp + INVITE_DAYS * DAY))
                inviter = self.profile(user["id"])["name"]
                self.notify(target, "team_invitation", f"{inviter} invited you to join {row[1]}",
                            message or "Open the invitation to accept or decline.", "#/invitations")
        except Exception as error:
            if self._unique_violation(error):
                raise self._error(409, "This person already has an open invitation to the team.")
            raise
        self.email(target, "team_invitation", target=row[1], inviter=inviter, message=message)
        return 201, {"invitation": {"id": inv_id, "status": "pending", "expiresAt": stamp + INVITE_DAYS * DAY}}

    def revoke_invitation(self, user, inv_id, q, body):
        inv = self.db.one("SELECT team_id, user_id, status FROM team_invitations WHERE id = ?", (int(inv_id),))
        if inv is None:
            raise self._error(404, "No such invitation.")
        row = self._team_row(inv[0])
        self._require_manager(inv[0], user)
        if self.db.execute("UPDATE team_invitations SET status = 'revoked', decided_at = ? WHERE id = ? AND status = 'pending'",
                           (self._now(), int(inv_id))) != 1:
            raise self._error(409, f"Only open invitations can be withdrawn (this one is {inv[2]}).")
        self.notify(inv[1], "team_invitation_revoked", f"Your invitation to {row[1]} was withdrawn", "", "#/invitations")
        return {"invitation": {"id": int(inv_id), "status": "revoked"}}

    def respond_invitation(self, user, inv_id, q, body):
        decision = body.get("decision")
        if decision not in ("accept", "decline"):
            raise self._error(400, "Decision must be accept or decline.")
        response = self._clean_text(body.get("response"), "Response", 600)
        self._expire_invitations()
        inv = self.db.one("SELECT team_id, user_id FROM team_invitations WHERE id = ?", (int(inv_id),))
        if inv is None or inv[1] != user["id"]:  # someone else's invitation looks the same as none
            raise self._error(404, "No such invitation.")
        row = self._team_row(inv[0])
        stamp = self._now()
        with self.db.transaction():
            # Re-read inside the transaction (it may have been withdrawn or expired a moment ago).
            status = self.db.one("SELECT status FROM team_invitations WHERE id = ?", (int(inv_id),))[0]
            if status != "pending":
                raise self._error(409, f"This invitation is no longer open (it was {status}).")
            if decision == "accept":
                if self._team_role(row[0], user["id"]) is None:
                    # Capacity is checked here, inside the same transaction as the insert, so two
                    # people accepting at once cannot both take the last place.
                    if self._member_count(row[0]) >= row[9]:
                        raise self._error(409, "The team is full right now. The invitation stays open until it "
                                               "expires, in case a place opens up.")
                    self.db.execute("INSERT INTO team_members (team_id, user_id, role, joined_at) VALUES (?, ?, 'member', ?)",
                                    (row[0], user["id"], stamp))
                self.db.execute("UPDATE team_applications SET status = 'withdrawn', decided_at = ? "
                                "WHERE team_id = ? AND user_id = ? AND status = 'pending'", (stamp, row[0], user["id"]))
            new_status = "accepted" if decision == "accept" else "declined"
            self.db.execute("UPDATE team_invitations SET status = ?, response = ?, decided_at = ? WHERE id = ?",
                            (new_status, response, stamp, int(inv_id)))
            name = self.profile(user["id"])["name"]
            for manager in self._team_managers(row[0]):
                if new_status == "accepted":
                    self.notify(manager, "team_invitation_accepted", f"{name} joined {row[1]}",
                                response or "They accepted your invitation.", f"#/teams/{row[0]}/members")
                else:
                    self.notify(manager, "team_invitation_declined", f"{name} declined the invitation to {row[1]}",
                                response, f"#/teams/{row[0]}/applications")
        return {"invitation": {"id": int(inv_id), "status": new_status}, "teamId": row[0]}

    def my_invitations(self, user, q, body):
        self._expire_invitations()
        status = q.get("status") or ""
        if status and status not in INVITATION_STATUSES:
            raise self._error(400, "Unknown status.")
        sql = self._INVITATION_SELECT + " WHERE i.user_id = ?" + (" AND i.status = ?" if status else "")
        rows = self.db.all(sql + " ORDER BY i.created_at DESC", (user["id"], status) if status else (user["id"],))
        page, size = self._page_args(q)
        result = self._paginate(rows, page, size)
        people = self.profiles([r[3] for r in result["items"]])
        items = []
        for r in result["items"]:
            item = self._invitation_dict(r, people)
            item["team"] = self._public_team(self._team_row(r[1]))
            item["team"]["memberStudentIds"] = self._member_student_ids(r[1])
            items.append(item)
        result["items"] = items
        return result

    # ------------------------------------------------------------------ the leader's overview
    def team_recruitment(self, user, team_id, q, body):
        """Applications and invitations of one team in one list, newest first.
        ?filter= all | pending | applications (direct) | link | invitations (open) | accepted | declined"""
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        self._expire_invitations()
        which = q.get("filter") or "pending"
        if which not in ("all", "pending", "applications", "link", "invitations", "accepted", "declined"):
            raise self._error(400, "Unknown filter.")
        apps = self._team_app_dicts(self.db.all(self._TEAM_APP_SELECT + " WHERE team_id = ?", (row[0],)))
        invitations_rows = self.db.all(self._INVITATION_SELECT + " WHERE i.team_id = ?", (row[0],))
        people = self.profiles([r[2] for r in invitations_rows] + [r[3] for r in invitations_rows], full=True)
        invitations = [self._invitation_dict(r, people) for r in invitations_rows]
        for a in apps:
            a["person"] = a.get("applicant")
        keep = {
            "all": lambda i: True,
            "pending": lambda i: i["status"] == "pending",
            "applications": lambda i: i["kind"] == "team" and i.get("source") != "link",
            "link": lambda i: i["kind"] == "team" and i.get("source") == "link",
            "invitations": lambda i: i["kind"] == "invitation" and i["status"] == "pending",
            "accepted": lambda i: i["kind"] == "invitation" and i["status"] == "accepted",
            "declined": lambda i: i["kind"] == "invitation" and i["status"] in ("declined", "expired", "revoked"),
        }[which]
        items = sorted((i for i in apps + invitations if keep(i)), key=lambda i: i["createdAt"], reverse=True)
        counts = {
            "pendingApplications": sum(1 for a in apps if a["status"] == "pending"),
            "pendingInvitations": sum(1 for i in invitations if i["status"] == "pending"),
        }
        page, size = self._page_args(q)
        result = self._paginate(items, page, size)
        result["counts"] = counts
        return result

    # ------------------------------------------------------------------ invitation links
    def _active_link(self, team_id):
        return self.db.one(
            """SELECT id, created_at, expires_at, uses FROM team_invite_links
               WHERE team_id = ? AND revoked_at IS NULL AND expires_at > ? ORDER BY id DESC LIMIT 1""",
            (int(team_id), self._now()))

    def invite_link_status(self, user, team_id, q, body):
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        link = self._active_link(row[0])
        via = self.db.one("SELECT COUNT(*) FROM team_applications WHERE team_id = ? AND source = 'link'", (row[0],))[0]
        return {"active": bool(link), "createdAt": link[1] if link else None, "expiresAt": link[2] if link else None,
                "opened": link[3] if link else 0, "applicationsViaLinks": via}

    def generate_invite_link(self, user, team_id, q, body):
        """Makes a new link (the old one stops working). The token is returned only here."""
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        days = body.get("days", DEFAULT_LINK_DAYS)
        if days not in LINK_DAYS:
            raise self._error(400, "A link can stay valid for 1, 7, 14 or 30 days.")
        self._take("invite_link", user, "You have made many links in the last hour. Try again later.")
        token = secrets.token_urlsafe(32)
        stamp = self._now()
        with self.db.transaction():
            self.db.execute("UPDATE team_invite_links SET revoked_at = ? WHERE team_id = ? AND revoked_at IS NULL",
                            (stamp, row[0]))
            self.db.insert("""INSERT INTO team_invite_links (team_id, token_hash, created_by, created_at, expires_at)
                              VALUES (?, ?, ?, ?, ?)""", (row[0], _hash(token), user["id"], stamp, stamp + days * DAY))
        return 201, {"path": f"/invite/{token}", "createdAt": stamp, "expiresAt": stamp + days * DAY, "active": True}

    def revoke_invite_link(self, user, team_id, q, body):
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        revoked = self.db.execute("UPDATE team_invite_links SET revoked_at = ? WHERE team_id = ? AND revoked_at IS NULL",
                                  (self._now(), row[0]))
        return {"active": False, "revoked": revoked}

    def _link_by_token(self, token):
        """(link id, team row) for a valid token, or HubError(404): unknown, expired and revoked
        links all get the same answer."""
        token = str(token or "")
        if not TOKEN_RE.fullmatch(token):
            raise self._error(404, "This invitation link is not valid any more. Ask the team for a new one.")
        link = self.db.one(
            "SELECT id, team_id FROM team_invite_links WHERE token_hash = ? AND revoked_at IS NULL AND expires_at > ?",
            (_hash(token), self._now()))
        if link is None:
            raise self._error(404, "This invitation link is not valid any more. Ask the team for a new one.")
        return link[0], self._team_row(link[1])

    def preview_invite_link(self, token) -> dict:
        """For the public /invite page (no sign-in): the team's public details."""
        link_id, row = self._link_by_token(token)
        return {"team": self._public_team(row), "expiresAt": self.db.one(
            "SELECT expires_at FROM team_invite_links WHERE id = ?", (link_id,))[0]}

    def claim_invite_link(self, user, q, body):
        """A signed-in person opened a link: remember it for them (the app opens the invitation,
        after onboarding if they are new)."""
        link_id, row = self._link_by_token(body.get("token"))
        with self.db.transaction():
            self.db.execute("""INSERT INTO link_claims (user_id, link_id, created_at) VALUES (?, ?, ?)
                               ON CONFLICT (user_id) DO UPDATE SET link_id = excluded.link_id,
                               created_at = excluded.created_at""", (user["id"], link_id, self._now()))
            self.db.execute("UPDATE team_invite_links SET uses = uses + 1 WHERE id = ?", (link_id,))
        return {"ok": True, "team": self._public_team(row), "member": self._team_role(row[0], user["id"]) is not None}

    def _claim(self, user):
        """(link id, team row) of this person's remembered link, if it still works."""
        claim = self.db.one(
            """SELECT c.link_id, l.team_id FROM link_claims c JOIN team_invite_links l ON l.id = c.link_id
               WHERE c.user_id = ? AND l.revoked_at IS NULL AND l.expires_at > ?""", (user["id"], self._now()))
        if claim is None:
            return None
        return claim[0], self._team_row(claim[1])

    def link_claim_summary(self, user):
        try:
            claim = self._claim(user)
        except Exception:
            return None
        if claim is None:
            return None
        row = claim[1]
        if self._team_role(row[0], user["id"]) is not None:
            return None
        return {"teamName": row[1]}

    def get_link_claim(self, user, q, body):
        claim = self._claim(user)
        if claim is None:
            had = self.db.one("SELECT 1 FROM link_claims WHERE user_id = ?", (user["id"],))
            if had:
                self.db.execute("DELETE FROM link_claims WHERE user_id = ?", (user["id"],))
            return {"claim": None, "expired": bool(had)}
        _, row = claim
        pending = self.db.one("SELECT id FROM team_applications WHERE team_id = ? AND user_id = ? AND status = 'pending'",
                              (row[0], user["id"]))
        team = self._public_team(row)
        # The team's members' participant profile ids, so the app can work out how the person would
        # complement the team (skills come from the shared participant data the app already has).
        team["memberStudentIds"] = self._member_student_ids(row[0])
        return {"claim": {"team": team, "member": self._team_role(row[0], user["id"]) is not None,
                          "pendingApplication": pending[0] if pending else None}}

    def apply_via_link(self, user, q, body):
        claim = self._claim(user)
        if claim is None:
            raise self._error(404, "This invitation link is not valid any more. Ask the team for a new one.")
        _, row = claim
        result = self._create_team_application(user, row, body, source="link")
        self.db.execute("DELETE FROM link_claims WHERE user_id = ?", (user["id"],))
        return result

    def dismiss_link_claim(self, user, q, body):
        self.db.execute("DELETE FROM link_claims WHERE user_id = ?", (user["id"],))
        return {"ok": True}
