"""TeamForge Hub: the people side of TeamForge on the web.

Teams with a private chat, applications to join a team, applications to take part in an event,
anonymous connection requests, and notifications, for signed-in accounts (web/server.py).

Unlike the app's five JSON documents (stored whole in `shared_files`, whose per-record rules are
checked only by the C++ app), everything here is kept in relational tables and every request is
authorised here, on the server: chat needs team membership, decisions need the team's leader or
one of its admins, event decisions need the event's organiser, and so on.

How this fits the existing TeamForge data:

* Events are the app's projects (requirements.json: hackathons, course projects ...).
  `event_settings` adds what applications need (organiser, deadline, capacity, eligibility,
  positions), keyed by the project id, so projects are not copied. A host opens a project for
  applications and becomes its organiser.
* Hub teams are formed by people with accounts: a leader creates one and recruits members.
  teams.json stays what it is: the Host workspace's planning drafts, built by the matching engine
  from participant profile records (which are not accounts).
* Notifications are new: TeamForge had none (the app's toasts are local to one screen).
* One identity per account: the `profiles` row (name, academic details, photo, colour) is what
  every screen shows for that person. It is linked to the participant profile the account uses
  in the app (`student_id`, from the account's session.json), so the app can show the same photo
  wherever that participant appears. Photos live in their own table and profiles carry only a
  reference (`/api/hub/photos/<user id>?v=<version>`); a new photo replaces the old one in place.

Errors are {"error": "..."} with an HTTP status, like the rest of the server's API.
"""

from __future__ import annotations

import base64
import json
import re
import secrets
import threading
import time
from urllib.parse import parse_qs

import recruitment
from recruitment import RecruitmentMixin, normalize_skill

# --------------------------------------------------------------------------- limits

PAGE_SIZE = 10
MAX_PAGE_SIZE = 50
MESSAGE_PAGE = 30
MAX_TEAM_MESSAGE = 2000
MAX_ANON_MESSAGE = 1000
MAX_LONG_TEXT = 1500
MAX_AVATAR_BYTES = 80 * 1024
LONG_POLL_SECONDS = 25
ANON_DAILY_LIMIT = 10          # new anonymous conversations per account per day (kept in the database)
ANON_REPEAT_DAYS = 7           # one open conversation per recipient at a time, within this window
AVATAR_COLORS = ["#F0434A", "#5AB0FF", "#3DD598", "#FFB547", "#A78BFA", "#2DD4BF", "#FF8F94"]
ROLE_LABELS = {"participant": "Participant", "host": "Host", "admin": "Developer"}

TEAM_ROLES = ("leader", "admin", "member")
TEAM_APP_STATUSES = ("pending", "approved", "rejected", "withdrawn")
EVENT_APP_STATUSES = ("pending", "approved", "rejected", "withdrawn", "waitlisted")
EVENT_ACTIVE = ("pending", "approved", "waitlisted")
ANON_STATUSES = ("new", "reviewed", "interested", "declined")


def now() -> float:
    return time.time()


class HubError(Exception):
    """A refused request: `status` and `message` are what the browser is told."""

    def __init__(self, status: int, message: str, **extra):
        super().__init__(message)
        self.status, self.message, self.extra = status, message, extra


# --------------------------------------------------------------------------- validation

_CONTROL = re.compile(r"[\x00-\x08\x0b-\x1f\x7f]")


def clean_text(value, what: str, limit: int, required: bool = False, multiline: bool = True) -> str:
    """Plain text: line endings normalised, control characters removed (tabs and line breaks
    kept), runs of blank lines collapsed, trimmed, at most `limit` characters."""
    if value is None:
        value = ""
    if not isinstance(value, str):
        raise HubError(400, f"{what} must be text.")
    text = value.replace("\r\n", "\n").replace("\r", "\n")
    if not multiline:
        text = re.sub(r"\s*\n\s*", " ", text)
    text = _CONTROL.sub("", text)
    text = re.sub(r"\n{3,}", "\n\n", text).strip()
    if required and not text:
        raise HubError(400, f"{what} is required.")
    if len(text) > limit:
        raise HubError(400, f"{what} can be at most {limit} characters.")
    return text


def clean_list(value, what: str, max_items: int, limit: int = 40) -> list[str]:
    """A short list of labels (skills, roles): a list or a comma-separated string; duplicates
    (ignoring case) dropped."""
    if value is None or value == "":
        return []
    if isinstance(value, str):
        value = value.split(",")
    if not isinstance(value, list):
        raise HubError(400, f"{what} must be a list.")
    out, seen = [], set()
    for item in value:
        label = clean_text(item, what, limit, multiline=False) if isinstance(item, str) else None
        if label is None:
            raise HubError(400, f"{what} must be a list of text.")
        if label and label.lower() not in seen:
            seen.add(label.lower())
            out.append(label)
    if len(out) > max_items:
        raise HubError(400, f"Use at most {max_items} {what.lower()}.")
    return out


def clean_int(value, what: str, low: int, high: int, optional: bool = False):
    if value is None or value == "":
        if optional:
            return None
        raise HubError(400, f"{what} is required.")
    if isinstance(value, bool):
        raise HubError(400, f"{what} must be a whole number.")
    try:
        number = int(value)
    except (TypeError, ValueError):
        raise HubError(400, f"{what} must be a whole number.")
    if number != value and not isinstance(value, str):
        raise HubError(400, f"{what} must be a whole number.")
    if not low <= number <= high:
        raise HubError(400, f"{what} must be between {low} and {high}.")
    return number


def clean_bool(value) -> bool:
    return value is True or value in (1, "1", "true", "yes", "on")


def clean_avatar(value):
    """None (no picture), or a small PNG / JPEG / WebP as a data: URL, checked by its bytes."""
    if value in (None, ""):
        return None
    if not isinstance(value, str):
        raise HubError(400, "The picture must be an image.")
    match = re.fullmatch(r"data:image/(png|jpeg|webp);base64,([A-Za-z0-9+/=]+)", value)
    if not match:
        raise HubError(400, "Use a PNG, JPEG or WebP picture.")
    try:
        data = base64.b64decode(match.group(2), validate=True)
    except ValueError:
        raise HubError(400, "The picture could not be read.")
    if len(data) > MAX_AVATAR_BYTES:
        raise HubError(400, "The picture is too large (at most 80 KB after resizing).")
    kind = match.group(1)
    ok = ((kind == "png" and data.startswith(b"\x89PNG\r\n")) or (kind == "jpeg" and data.startswith(b"\xff\xd8\xff"))
          or (kind == "webp" and data[:4] == b"RIFF" and data[8:12] == b"WEBP"))
    if not ok:
        raise HubError(400, "The picture's contents do not match its type.")
    return value


def page_args(query: dict, size: int = PAGE_SIZE) -> tuple[int, int]:
    page = clean_int(query.get("page") or 1, "Page", 1, 100000)
    size = clean_int(query.get("pageSize") or size, "Page size", 1, MAX_PAGE_SIZE)
    return page, size


def paginate(items: list, page: int, size: int) -> dict:
    total = len(items)
    start = (page - 1) * size
    return {"items": items[start:start + size], "page": page, "pageSize": size, "total": total,
            "pages": max(1, (total + size - 1) // size)}


def _loads(text, default):
    try:
        value = json.loads(text) if text else default
    except (TypeError, ValueError):
        return default
    return value if isinstance(value, type(default)) else default


def _is_unique_violation(error: Exception) -> bool:
    return "unique" in str(error).lower() or type(error).__name__ in ("IntegrityError", "UniqueViolation")


# --------------------------------------------------------------------------- schema

def _columns(db, table: str) -> set[str]:
    if db.is_pg:
        rows = db.all("SELECT column_name FROM information_schema.columns WHERE table_name = ?", (table,))
        return {r[0] for r in rows}
    return {r[1] for r in db.all(f"PRAGMA table_info({table})")}


def ensure_column(db, table: str, column: str, ddl: str) -> None:
    """Adds a column to an existing table (a migration that keeps every row)."""
    if column not in _columns(db, table):
        db.execute(f"ALTER TABLE {table} ADD COLUMN {column} {ddl}")


def create_schema(db) -> None:
    """The hub's tables. CREATE ... IF NOT EXISTS only: existing tables and data are kept."""
    pk = "SERIAL PRIMARY KEY" if db.is_pg else "INTEGER PRIMARY KEY AUTOINCREMENT"
    real = "DOUBLE PRECISION" if db.is_pg else "REAL"
    statements = [
        f"""CREATE TABLE IF NOT EXISTS profiles (
            user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
            display_name TEXT NOT NULL DEFAULT '', headline TEXT NOT NULL DEFAULT '',
            bio TEXT NOT NULL DEFAULT '', skills TEXT NOT NULL DEFAULT '[]',
            experience TEXT NOT NULL DEFAULT '', interests TEXT NOT NULL DEFAULT '',
            avatar TEXT, color TEXT NOT NULL DEFAULT '', accepts_anonymous INTEGER NOT NULL DEFAULT 0,
            updated_at {real} NOT NULL)""",
        f"""CREATE TABLE IF NOT EXISTS teams (
            id {pk}, name TEXT NOT NULL, name_key TEXT NOT NULL UNIQUE, tagline TEXT NOT NULL DEFAULT '',
            description TEXT NOT NULL DEFAULT '', objectives TEXT NOT NULL DEFAULT '',
            open_roles TEXT NOT NULL DEFAULT '[]', skills TEXT NOT NULL DEFAULT '[]', event_id TEXT,
            recruiting INTEGER NOT NULL DEFAULT 1, max_members INTEGER NOT NULL DEFAULT 6,
            created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            created_at {real} NOT NULL, updated_at {real} NOT NULL)""",
        f"""CREATE TABLE IF NOT EXISTS team_members (
            team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            role TEXT NOT NULL, joined_at {real} NOT NULL, last_read_id INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (team_id, user_id))""",
        "CREATE INDEX IF NOT EXISTS idx_team_members_user ON team_members (user_id)",
        f"""CREATE TABLE IF NOT EXISTS team_messages (
            id {pk}, team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
            user_id INTEGER REFERENCES users(id) ON DELETE SET NULL, body TEXT NOT NULL,
            created_at {real} NOT NULL)""",
        "CREATE INDEX IF NOT EXISTS idx_team_messages_team ON team_messages (team_id, id)",
        f"""CREATE TABLE IF NOT EXISTS team_applications (
            id {pk}, team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            introduction TEXT NOT NULL, skills TEXT NOT NULL DEFAULT '[]', experience TEXT NOT NULL DEFAULT '',
            interests TEXT NOT NULL DEFAULT '', reason TEXT NOT NULL, status TEXT NOT NULL,
            response TEXT NOT NULL DEFAULT '', decided_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            created_at {real} NOT NULL, decided_at {real})""",
        # One pending application per person per team, enforced by the database itself.
        """CREATE UNIQUE INDEX IF NOT EXISTS uq_team_applications_pending
            ON team_applications (team_id, user_id) WHERE status = 'pending'""",
        "CREATE INDEX IF NOT EXISTS idx_team_applications_team ON team_applications (team_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_team_applications_user ON team_applications (user_id)",
        f"""CREATE TABLE IF NOT EXISTS event_settings (
            event_id TEXT PRIMARY KEY, organizer_id INTEGER NOT NULL REFERENCES users(id),
            description TEXT NOT NULL DEFAULT '', requirements_text TEXT NOT NULL DEFAULT '',
            eligibility TEXT NOT NULL DEFAULT '', positions TEXT NOT NULL DEFAULT '[]',
            capacity INTEGER, deadline {real}, accepting INTEGER NOT NULL DEFAULT 1,
            updated_at {real} NOT NULL)""",
        f"""CREATE TABLE IF NOT EXISTS event_applications (
            id {pk}, event_id TEXT NOT NULL REFERENCES event_settings(event_id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            position TEXT NOT NULL DEFAULT '', motivation TEXT NOT NULL, experience TEXT NOT NULL DEFAULT '',
            availability TEXT NOT NULL DEFAULT '', status TEXT NOT NULL, feedback TEXT NOT NULL DEFAULT '',
            decided_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            created_at {real} NOT NULL, decided_at {real})""",
        """CREATE UNIQUE INDEX IF NOT EXISTS uq_event_applications_active
            ON event_applications (event_id, user_id) WHERE status IN ('pending', 'approved', 'waitlisted')""",
        "CREATE INDEX IF NOT EXISTS idx_event_applications_event ON event_applications (event_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_event_applications_user ON event_applications (user_id)",
        f"""CREATE TABLE IF NOT EXISTS anon_threads (
            id {pk}, public_id TEXT NOT NULL UNIQUE,
            sender_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            target_kind TEXT NOT NULL, target_id TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'new',
            sender_unread INTEGER NOT NULL DEFAULT 0, recipient_unread INTEGER NOT NULL DEFAULT 1,
            invited_at {real}, created_at {real} NOT NULL, last_message_at {real} NOT NULL)""",
        "CREATE INDEX IF NOT EXISTS idx_anon_threads_sender ON anon_threads (sender_id)",
        "CREATE INDEX IF NOT EXISTS idx_anon_threads_target ON anon_threads (target_kind, target_id)",
        f"""CREATE TABLE IF NOT EXISTS anon_messages (
            id {pk}, thread_id INTEGER NOT NULL REFERENCES anon_threads(id) ON DELETE CASCADE,
            from_sender INTEGER NOT NULL, author_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
            body TEXT NOT NULL, created_at {real} NOT NULL)""",
        "CREATE INDEX IF NOT EXISTS idx_anon_messages_thread ON anon_messages (thread_id, id)",
        f"""CREATE TABLE IF NOT EXISTS anon_reports (
            id {pk}, thread_id INTEGER NOT NULL REFERENCES anon_threads(id) ON DELETE CASCADE,
            reporter_id INTEGER REFERENCES users(id) ON DELETE SET NULL, reason TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'open', resolution TEXT NOT NULL DEFAULT '',
            resolved_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            created_at {real} NOT NULL, resolved_at {real})""",
        f"""CREATE TABLE IF NOT EXISTS anon_blocks (
            target_kind TEXT NOT NULL, target_id TEXT NOT NULL,
            sender_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, created_at {real} NOT NULL,
            PRIMARY KEY (target_kind, target_id, sender_id))""",
        f"""CREATE TABLE IF NOT EXISTS notifications (
            id {pk}, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            kind TEXT NOT NULL, title TEXT NOT NULL, body TEXT NOT NULL DEFAULT '', link TEXT NOT NULL DEFAULT '',
            ref_key TEXT, read_at {real}, created_at {real} NOT NULL)""",
        "CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications (user_id, id)",
        "CREATE INDEX IF NOT EXISTS idx_notifications_unread ON notifications (user_id, read_at)",
        # Profile photos, one per account (small, already resized by the app), kept out of the
        # profile rows that every list reads.
        f"""CREATE TABLE IF NOT EXISTS profile_photos (
            user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
            content_type TEXT NOT NULL, data TEXT NOT NULL, version INTEGER NOT NULL,
            updated_at {real} NOT NULL)""",
    ]
    for sql in statements:
        db.execute(sql)
    # Added after the first release; existing rows keep their data.
    ensure_column(db, "profiles", "student_id", "TEXT")
    ensure_column(db, "profiles", "program", "TEXT NOT NULL DEFAULT ''")
    ensure_column(db, "profiles", "email_updates", "INTEGER NOT NULL DEFAULT 1")
    db.execute("CREATE INDEX IF NOT EXISTS idx_profiles_student ON profiles (student_id)")
    recruitment.create_schema(db, pk, real, ensure_column)


# Parents before children, for web/tools/copy_database.py.
HUB_TABLES = {
    "profiles": ["user_id", "display_name", "headline", "bio", "skills", "experience", "interests", "avatar",
                 "color", "accepts_anonymous", "updated_at", "student_id", "program", "email_updates", "links",
                 "open_to_invites"],
    "profile_photos": ["user_id", "content_type", "data", "version", "updated_at"],
    "teams": ["id", "name", "name_key", "tagline", "description", "objectives", "open_roles", "skills", "event_id",
              "recruiting", "max_members", "created_by", "created_at", "updated_at", "required_skills"],
    "team_members": ["team_id", "user_id", "role", "joined_at", "last_read_id"],
    "team_messages": ["id", "team_id", "user_id", "body", "created_at"],
    "team_applications": ["id", "team_id", "user_id", "introduction", "skills", "experience", "interests", "reason",
                          "status", "response", "decided_by", "created_at", "decided_at", "source"],
    "event_settings": ["event_id", "organizer_id", "description", "requirements_text", "eligibility", "positions",
                       "capacity", "deadline", "accepting", "updated_at"],
    "event_applications": ["id", "event_id", "user_id", "position", "motivation", "experience", "availability",
                           "status", "feedback", "decided_by", "created_at", "decided_at"],
    "anon_threads": ["id", "public_id", "sender_id", "target_kind", "target_id", "status", "sender_unread",
                     "recipient_unread", "invited_at", "created_at", "last_message_at"],
    "anon_messages": ["id", "thread_id", "from_sender", "author_id", "body", "created_at"],
    "anon_reports": ["id", "thread_id", "reporter_id", "reason", "status", "resolution", "resolved_by",
                     "created_at", "resolved_at"],
    "anon_blocks": ["target_kind", "target_id", "sender_id", "created_at"],
    "notifications": ["id", "user_id", "kind", "title", "body", "link", "ref_key", "read_at", "created_at"],
    **recruitment.TABLES,
}

_PROFILE_SELECT = ("SELECT u.id, u.role, p.display_name, p.headline, p.bio, p.skills, p.experience, p.interests, "
                   "p.avatar, p.color, p.accepts_anonymous, p.student_id, p.program, p.email_updates, "
                   "p.links, p.open_to_invites FROM users u LEFT JOIN profiles p ON p.user_id = u.id")
PHOTO_PREFIX = "/api/hub/photos/"


def _marks(values) -> str:
    return ", ".join("?" for _ in values)


# --------------------------------------------------------------------------- the hub

class Hub(RecruitmentMixin):
    def __init__(self, db, rate_limit):
        """`rate_limit(limit, window)` makes a per-key limiter with a `take(key) -> bool`."""
        self.db = db
        self.limits = {
            "team_message": rate_limit(20, 60),
            "application": rate_limit(10, 3600),
            "team_create": rate_limit(5, 3600),
            "anon_new": rate_limit(5, 3600),
            "anon_reply": rate_limit(30, 3600),
            "report": rate_limit(10, 3600),
            "invite": rate_limit(30, 3600),
            "invite_link": rate_limit(10, 3600),
        }
        # Set by the server: mailer(user_id, kind, context) sends an email (web/email_service.py).
        self.mailer = None
        self.chat = threading.Condition()
        self.latest_message: dict[int, int] = {}
        self._events_cache = (None, {})
        self.routes = [(method, re.compile(pattern), getattr(self, name)) for method, pattern, name in [
            ("GET", r"/summary", "summary"),
            ("GET", r"/profile", "get_my_profile"),
            ("POST", r"/profile", "save_my_profile"),
            ("GET", r"/people", "list_people"),
            ("GET", r"/people/(\d+)", "get_person"),
            ("GET", r"/identities", "identities"),
            ("GET", r"/photos/(\d+)", "get_photo"),
            ("POST", r"/profile/photo", "set_photo"),
            ("POST", r"/profile/photo/remove", "remove_photo"),
            ("POST", r"/profile/sync", "sync_identity"),
            ("GET", r"/teams", "list_teams"),
            ("POST", r"/teams", "create_team"),
            ("GET", r"/teams/(\d+)", "get_team"),
            ("POST", r"/teams/(\d+)", "update_team"),
            ("POST", r"/teams/(\d+)/delete", "delete_team"),
            ("POST", r"/teams/(\d+)/leave", "leave_team"),
            ("POST", r"/teams/(\d+)/members/(\d+)/role", "set_member_role"),
            ("POST", r"/teams/(\d+)/members/(\d+)/remove", "remove_member"),
            ("GET", r"/teams/(\d+)/messages", "get_messages"),
            ("POST", r"/teams/(\d+)/messages", "post_message"),
            ("POST", r"/teams/(\d+)/read", "mark_read"),
            ("GET", r"/conversations", "conversations"),
            ("POST", r"/teams/(\d+)/applications", "apply_to_team"),
            ("GET", r"/teams/(\d+)/applications", "team_applications"),
            ("POST", r"/team-applications/(\d+)/decision", "decide_team_application"),
            ("POST", r"/team-applications/(\d+)/withdraw", "withdraw_team_application"),
            ("GET", r"/events", "list_events"),
            ("GET", r"/events/([A-Za-z0-9_.\-]{1,64})", "get_event"),
            ("POST", r"/events/([A-Za-z0-9_.\-]{1,64})/settings", "save_event_settings"),
            ("POST", r"/events/([A-Za-z0-9_.\-]{1,64})/applications", "apply_to_event"),
            ("GET", r"/events/([A-Za-z0-9_.\-]{1,64})/applications", "event_applications"),
            ("POST", r"/event-applications/(\d+)/decision", "decide_event_application"),
            ("POST", r"/event-applications/(\d+)/withdraw", "withdraw_event_application"),
            ("GET", r"/applications/mine", "my_applications"),
            ("GET", r"/applications/received", "received_applications"),
            ("POST", r"/anonymous", "anon_create"),
            ("GET", r"/anonymous/inbox", "anon_inbox"),
            ("GET", r"/anonymous/sent", "anon_sent"),
            ("GET", r"/anonymous/([A-Za-z0-9_\-]{8,40})", "anon_thread"),
            ("POST", r"/anonymous/([A-Za-z0-9_\-]{8,40})/messages", "anon_reply"),
            ("POST", r"/anonymous/([A-Za-z0-9_\-]{8,40})/status", "anon_status"),
            ("POST", r"/anonymous/([A-Za-z0-9_\-]{8,40})/invite", "anon_invite"),
            ("POST", r"/anonymous/([A-Za-z0-9_\-]{8,40})/report", "anon_report"),
            ("POST", r"/anonymous/([A-Za-z0-9_\-]{8,40})/block", "anon_block"),
            ("GET", r"/notifications", "list_notifications"),
            ("GET", r"/notifications/count", "notification_count"),
            ("POST", r"/notifications/read", "read_notifications"),
            ("GET", r"/admin/reports", "admin_reports"),
            ("POST", r"/admin/reports/(\d+)/resolve", "admin_resolve_report"),
        ] + self.recruitment_routes()]

    # ------------------------------------------------------------------ dispatch
    def handle(self, user: dict, method: str, path: str, query: str = "", body: dict | None = None):
        """Returns (status, payload). `path` is the part after /api/hub."""
        params = {k: v[0] for k, v in parse_qs(query or "", keep_blank_values=True).items()}
        for route_method, pattern, func in self.routes:
            if route_method != method:
                continue
            match = pattern.fullmatch(path or "/")
            if match:
                try:
                    result = func(user, *match.groups(), q=params, body=body or {})
                except HubError as error:
                    return error.status, dict({"error": error.message}, **error.extra)
                if isinstance(result, tuple):
                    return result
                return 200, result
        return 404, {"error": "Not found"}

    # (used by web/recruitment.py)
    _now = staticmethod(lambda: now())
    _error = HubError
    _clean_text = staticmethod(clean_text)
    _unique_violation = staticmethod(lambda error: _is_unique_violation(error))
    _page_args = staticmethod(lambda q, size=PAGE_SIZE: page_args(q, size))
    _paginate = staticmethod(lambda items, page, size: paginate(items, page, size))
    _profile_select = staticmethod(lambda: _PROFILE_SELECT)

    def _take(self, kind: str, user: dict, message: str):
        if not self.limits[kind].take(user["id"]):
            raise HubError(429, message)

    # ------------------------------------------------------------------ profiles
    def _profile_from_row(self, row, full: bool = False) -> dict:
        (uid, role, name, headline, bio, skills, experience, interests, avatar, color, accepts, student_id, program,
         email_updates, links, open_to_invites) = row
        profile = {
            "id": uid, "name": name or "New member", "headline": headline or "",
            # a reference to the photo (fetched separately and cached by the app), never the image
            "avatar": avatar if avatar and avatar.startswith(PHOTO_PREFIX) else None,
            "color": color or AVATAR_COLORS[uid % len(AVATAR_COLORS)], "role": ROLE_LABELS.get(role, "Participant"),
            "studentId": student_id or "",
        }
        if full:
            profile.update({"bio": bio or "", "skills": _loads(skills, []), "experience": experience or "",
                            "interests": interests or "", "acceptsAnonymous": bool(accepts), "program": program or "",
                            "emailUpdates": email_updates is None or bool(email_updates),
                            "links": _loads(links, []), "openToInvites": open_to_invites is None or bool(open_to_invites)})
        return profile

    def profiles(self, ids, full: bool = False) -> dict[int, dict]:
        ids = sorted({int(i) for i in ids if i is not None})
        if not ids:
            return {}
        rows = self.db.all(f"{_PROFILE_SELECT} WHERE u.id IN ({_marks(ids)})", tuple(ids))
        return {row[0]: self._profile_from_row(row, full) for row in rows}

    def profile(self, user_id, full: bool = False) -> dict:
        found = self.profiles([user_id], full)
        if user_id not in found:
            raise HubError(404, "No such person.")
        return found[user_id]

    def _require_profile(self, user):
        row = self.db.one("SELECT display_name FROM profiles WHERE user_id = ?", (user["id"],))
        if not row or not row[0]:
            raise HubError(409, "Set up your profile first (your name, at least), so others know who you are.",
                           code="profile_needed")

    def _linked_student(self, user_id) -> dict | None:
        """The participant profile this account uses in the app, used to suggest profile details."""
        own = self.db.one("SELECT content FROM user_files WHERE user_id = ? AND name = 'session.json'", (user_id,))
        student_id = _loads(own[0], {}).get("myProfileId") if own else None
        return self._student(student_id) if student_id else None

    def get_my_profile(self, user, q, body):
        found = self.profiles([user["id"]], full=True).get(user["id"])
        row = self.db.one("SELECT display_name FROM profiles WHERE user_id = ?", (user["id"],))
        result = {"profile": found, "email": user["email"], "needsSetup": not (row and row[0]), "suggestion": None}
        if result["needsSetup"]:
            student = self._linked_student(user["id"])
            if student:
                result["suggestion"] = {
                    "name": str(student.get("name", ""))[:60], "headline": str(student.get("role", ""))[:120],
                    "bio": str(student.get("summary", ""))[:1500],
                    "skills": [str(s) for s in (student.get("skillsOffered") or {})][:20],
                    "interests": ", ".join(str(s) for s in (student.get("skillsWanted") or []))[:1000],
                }
        return result

    def save_my_profile(self, user, q, body):
        name = clean_text(body.get("name"), "Name", 60, required=True, multiline=False)
        avatar = body.get("avatar")
        new_photo = clean_avatar(avatar) if isinstance(avatar, str) and avatar.startswith("data:") else None
        values = (
            name,
            clean_text(body.get("headline"), "Headline", 120, multiline=False),
            clean_text(body.get("bio"), "About you", MAX_LONG_TEXT),
            json.dumps(clean_list(body.get("skills"), "Skills", 20)),
            clean_text(body.get("experience"), "Experience", MAX_LONG_TEXT),
            clean_text(body.get("interests"), "Interests", 1000),
            body.get("color") if body.get("color") in AVATAR_COLORS else "",
            1 if clean_bool(body.get("acceptsAnonymous")) else 0,
            clean_text(body.get("program"), "Academic details", 120, multiline=False),
            0 if body.get("emailUpdates") is False else 1,
            now(),
        )
        with self.db.transaction():
            self.db.execute(
                """INSERT INTO profiles (display_name, headline, bio, skills, experience, interests, color,
                       accepts_anonymous, program, email_updates, updated_at, user_id)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                   ON CONFLICT (user_id) DO UPDATE SET display_name = excluded.display_name,
                       headline = excluded.headline, bio = excluded.bio, skills = excluded.skills,
                       experience = excluded.experience, interests = excluded.interests,
                       color = excluded.color, accepts_anonymous = excluded.accepts_anonymous,
                       program = excluded.program, email_updates = excluded.email_updates,
                       updated_at = excluded.updated_at""",
                values + (user["id"],))
            if "links" in body:
                self.db.execute("UPDATE profiles SET links = ? WHERE user_id = ?",
                                (json.dumps(recruitment.clean_links(body.get("links"), HubError)), user["id"]))
            if "openToInvites" in body:
                self.db.execute("UPDATE profiles SET open_to_invites = ? WHERE user_id = ?",
                                (0 if body.get("openToInvites") is False else 1, user["id"]))
            # The photo: a new picture replaces the old one; empty removes it; the current
            # reference (or nothing said) keeps it.
            if new_photo:
                self._store_photo(user["id"], new_photo)
            elif "avatar" in body and not avatar:
                self._drop_photo(user["id"])
        return {"profile": self.profile(user["id"], full=True)}

    # ------------------------------------------------------------------ photos and identity
    def _ensure_profile_row(self, user_id):
        self.db.execute("INSERT INTO profiles (user_id, updated_at) VALUES (?, ?) ON CONFLICT (user_id) DO NOTHING",
                        (user_id, now()))

    def _store_photo(self, user_id, data_url: str) -> str:
        kind, data = data_url[len("data:"):].split(";base64,", 1)
        with self.db.transaction():
            self._ensure_profile_row(user_id)
            row = self.db.one("SELECT version FROM profile_photos WHERE user_id = ?", (user_id,))
            version = (row[0] if row else 0) + 1
            self.db.execute(
                """INSERT INTO profile_photos (user_id, content_type, data, version, updated_at) VALUES (?, ?, ?, ?, ?)
                   ON CONFLICT (user_id) DO UPDATE SET content_type = excluded.content_type, data = excluded.data,
                       version = excluded.version, updated_at = excluded.updated_at""",
                (user_id, kind, data, version, now()))
            ref = f"{PHOTO_PREFIX}{user_id}?v={version}"
            self.db.execute("UPDATE profiles SET avatar = ?, updated_at = ? WHERE user_id = ?", (ref, now(), user_id))
        return ref

    def _drop_photo(self, user_id):
        with self.db.transaction():
            self.db.execute("DELETE FROM profile_photos WHERE user_id = ?", (user_id,))
            self.db.execute("UPDATE profiles SET avatar = NULL, updated_at = ? WHERE user_id = ?", (now(), user_id))

    def set_photo(self, user, q, body):
        image = body.get("image")
        if not image:
            raise HubError(400, "Choose a picture.")
        ref = self._store_photo(user["id"], clean_avatar(image))
        return {"avatar": ref, "profile": self.profile(user["id"], full=True)}

    def remove_photo(self, user, q, body):
        self._drop_photo(user["id"])
        return {"avatar": None, "profile": self.profile(user["id"], full=True)}

    def get_photo(self, user, user_id, q, body):
        row = self.db.one("SELECT content_type, data, version FROM profile_photos WHERE user_id = ?", (int(user_id),))
        if row is None:
            raise HubError(404, "No photo.")
        return {"image": f"data:{row[0]};base64,{row[1]}", "version": row[2]}

    def link_student(self, user_id, student_id) -> None:
        """Links the account to the participant profile it uses in the app (from session.json).
        A profile belongs to one account: the latest to choose it. An account without a name yet
        takes the participant profile's name, academic details and summary."""
        student_id = str(student_id or "").strip()[:64]
        with self.db.transaction():
            self._ensure_profile_row(user_id)
            current = self.db.one("SELECT student_id FROM profiles WHERE user_id = ?", (user_id,))
            if current and (current[0] or "") == student_id:
                return
            if student_id:
                self.db.execute("UPDATE profiles SET student_id = NULL, updated_at = ? WHERE student_id = ? AND user_id <> ?",
                                (now(), student_id, user_id))
            self.db.execute("UPDATE profiles SET student_id = ?, updated_at = ? WHERE user_id = ?",
                            (student_id or None, now(), user_id))
            student = self._student(student_id) if student_id else None
            named = self.db.one("SELECT display_name FROM profiles WHERE user_id = ?", (user_id,))
            if student and named and not named[0]:
                self.db.execute(
                    "UPDATE profiles SET display_name = ?, program = ?, headline = ?, bio = ?, skills = ? WHERE user_id = ?",
                    (str(student.get("name", ""))[:60], str(student.get("program", ""))[:120],
                     str(student.get("role", ""))[:120], str(student.get("summary", ""))[:MAX_LONG_TEXT],
                     json.dumps([str(s) for s in (student.get("skillsOffered") or {})][:20]), user_id))

    def _student(self, student_id) -> dict | None:
        row = self.db.one("SELECT content FROM shared_files WHERE name = 'students.json'")
        for student in _loads(row[0], {}).get("students", []) if row else []:
            if isinstance(student, dict) and student.get("id") == student_id:
                return student
        return None

    def migrate(self) -> None:
        """Brings existing data up to date, keeping it: photos stored inside profile rows move to
        their own table, and accounts are linked to the participant profiles they chose."""
        for user_id, avatar in self.db.all("SELECT user_id, avatar FROM profiles WHERE avatar LIKE 'data:%'"):
            try:
                self._store_photo(user_id, clean_avatar(avatar))
            except HubError:
                self.db.execute("UPDATE profiles SET avatar = NULL WHERE user_id = ?", (user_id,))
        for user_id, content in self.db.all("SELECT user_id, content FROM user_files WHERE name = 'session.json'"):
            student_id = _loads(content, {}).get("myProfileId")
            linked = self.db.one("SELECT student_id FROM profiles WHERE user_id = ?", (user_id,))
            if student_id and not (linked and linked[0]):
                self.link_student(user_id, student_id)

    def sync_identity(self, user, q, body):
        """The app keeps the account's name and academic details in step with the participant
        profile the person edits in TeamForge (one name everywhere)."""
        name = clean_text(body.get("name"), "Name", 60, required=True, multiline=False)
        program = clean_text(body.get("program"), "Academic details", 120, multiline=False)
        with self.db.transaction():
            self._ensure_profile_row(user["id"])
            self.db.execute("UPDATE profiles SET display_name = ?, program = ?, updated_at = ? WHERE user_id = ?",
                            (name, program, now(), user["id"]))
            if "headline" in body:
                self.db.execute("UPDATE profiles SET headline = ? WHERE user_id = ?",
                                (clean_text(body.get("headline"), "Headline", 120, multiline=False), user["id"]))
        if body.get("studentId"):
            self.link_student(user["id"], body.get("studentId"))
        return {"profile": self.profile(user["id"], full=True)}

    def identity_version(self) -> str:
        row = self.db.one("SELECT COUNT(*), MAX(updated_at) FROM profiles")
        return f"{row[0]}:{row[1] or 0}"

    def identities(self, user, q, body):
        """Participant profile id -> the account identity using it (name, photo reference,
        colour), so the app shows the same person the same way everywhere. No emails."""
        rows = self.db.all(f"{_PROFILE_SELECT} WHERE p.student_id IS NOT NULL AND p.student_id <> ''")
        return {"items": [self._profile_from_row(r) for r in rows], "version": self.identity_version()}

    def list_people(self, user, q, body):
        """People directory. ?anonymous=1 lists those who accept anonymous messages (developers,
        hosts and others who opted in). Email addresses are never part of a profile."""
        page, size = page_args(q, 12)
        where, params = ["p.display_name <> ''"], []
        if clean_bool(q.get("anonymous")):
            where.append("p.accepts_anonymous = 1")
        if q.get("q"):
            needle = "%" + clean_text(q["q"], "Search", 80, multiline=False).lower() + "%"
            where.append("(LOWER(p.display_name) LIKE ? OR LOWER(p.headline) LIKE ? OR LOWER(p.skills) LIKE ?)")
            params += [needle, needle, needle]
        rows = self.db.all(f"{_PROFILE_SELECT} WHERE {' AND '.join(where)} ORDER BY LOWER(p.display_name)",
                           tuple(params))
        people = [self._profile_from_row(r, full=True) for r in rows]
        for person in people:
            person["isMe"] = person["id"] == user["id"]
        return paginate(people, page, size)

    def get_person(self, user, person_id, q, body):
        person = self.profile(int(person_id), full=True)
        teams = self.db.all(
            """SELECT t.id, t.name, m.role FROM team_members m JOIN teams t ON t.id = m.team_id
               WHERE m.user_id = ? ORDER BY t.name""", (int(person_id),))
        person["teams"] = [{"id": t[0], "name": t[1], "role": t[2]} for t in teams]
        person["isMe"] = person["id"] == user["id"]
        return {"person": person}

    # ------------------------------------------------------------------ notifications
    def notify(self, user_id, kind, title, body="", link="", ref_key=None):
        """Adds a notification. With `ref_key`, an unread notification with the same key is updated
        instead (a busy team chat gives one "new messages" notification, not one per message)."""
        if ref_key:
            row = self.db.one("SELECT id FROM notifications WHERE user_id = ? AND ref_key = ? AND read_at IS NULL",
                              (user_id, ref_key))
            if row:
                self.db.execute("UPDATE notifications SET title = ?, body = ?, link = ?, created_at = ? WHERE id = ?",
                                (title, body[:300], link, now(), row[0]))
                return
        self.db.execute(
            """INSERT INTO notifications (user_id, kind, title, body, link, ref_key, created_at)
               VALUES (?, ?, ?, ?, ?, ?, ?)""", (user_id, kind, title[:160], body[:300], link, ref_key, now()))

    def email(self, user_id, template: str, **context) -> None:
        """Emails a person about something important (decisions, invitations, removal), unless
        they turned email updates off. Never blocks the request; never in an API response."""
        if self.mailer is None:
            return
        row = self.db.one("SELECT email_updates FROM profiles WHERE user_id = ?", (user_id,))
        if row is not None and not row[0]:
            return
        try:
            self.mailer(user_id, template, context)
        except Exception:  # an email problem never breaks the action itself
            pass

    def _unread_notifications(self, user_id) -> int:
        return self.db.one("SELECT COUNT(*) FROM notifications WHERE user_id = ? AND read_at IS NULL", (user_id,))[0]

    def list_notifications(self, user, q, body):
        page, size = page_args(q, 15)
        where = "user_id = ?" + (" AND read_at IS NULL" if clean_bool(q.get("unread")) else "")
        total = self.db.one(f"SELECT COUNT(*) FROM notifications WHERE {where}", (user["id"],))[0]
        rows = self.db.all(
            f"""SELECT id, kind, title, body, link, read_at, created_at FROM notifications WHERE {where}
                ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?""", (user["id"], size, (page - 1) * size))
        items = [{"id": r[0], "kind": r[1], "title": r[2], "body": r[3], "link": r[4], "read": r[5] is not None,
                  "createdAt": r[6]} for r in rows]
        return {"items": items, "page": page, "pageSize": size, "total": total,
                "pages": max(1, (total + size - 1) // size), "unread": self._unread_notifications(user["id"])}

    def notification_count(self, user, q, body):
        return {"unread": self._unread_notifications(user["id"])}

    def read_notifications(self, user, q, body):
        if clean_bool(body.get("all")):
            self.db.execute("UPDATE notifications SET read_at = ? WHERE user_id = ? AND read_at IS NULL",
                            (now(), user["id"]))
        else:
            ids = body.get("ids")
            if not isinstance(ids, list) or not ids or len(ids) > 200 or not all(
                    isinstance(i, int) and not isinstance(i, bool) for i in ids):
                raise HubError(400, "Send the notification ids to mark as read, or all: true.")
            self.db.execute(
                f"UPDATE notifications SET read_at = ? WHERE user_id = ? AND read_at IS NULL AND id IN ({_marks(ids)})",
                (now(), user["id"], *ids))
        return {"unread": self._unread_notifications(user["id"])}

    # ------------------------------------------------------------------ teams: helpers
    def _team_row(self, team_id):
        row = self.db.one(
            """SELECT id, name, tagline, description, objectives, open_roles, skills, event_id, recruiting,
                      max_members, created_at, required_skills FROM teams WHERE id = ?""", (int(team_id),))
        if row is None:
            raise HubError(404, "This team does not exist (it may have been deleted).")
        return row

    def _team_role(self, team_id, user_id):
        row = self.db.one("SELECT role FROM team_members WHERE team_id = ? AND user_id = ?", (int(team_id), user_id))
        return row[0] if row else None

    def _member_count(self, team_id) -> int:
        return self.db.one("SELECT COUNT(*) FROM team_members WHERE team_id = ?", (int(team_id),))[0]

    def _team_managers(self, team_id) -> list[int]:
        rows = self.db.all("SELECT user_id FROM team_members WHERE team_id = ? AND role IN ('leader', 'admin')",
                           (int(team_id),))
        return [r[0] for r in rows]

    def _require_member(self, team_id, user):
        self._team_row(team_id)
        role = self._team_role(team_id, user["id"])
        if role is None:
            raise HubError(403, "Only this team's members can do that.")
        return role

    def _require_manager(self, team_id, user):
        role = self._require_member(team_id, user)
        if role not in ("leader", "admin"):
            raise HubError(403, "Only the team leader or a team admin can do that.")
        return role

    def _team_fields(self, body, partial=False) -> dict:
        fields = {}
        if not partial or "name" in body:
            name = clean_text(body.get("name"), "Team name", 60, required=True, multiline=False)
            if len(name) < 3:
                raise HubError(400, "Team name must have at least 3 characters.")
            fields["name"] = name
            fields["name_key"] = re.sub(r"\s+", " ", name.lower())
        for key, column, what, limit in (("tagline", "tagline", "Tagline", 120),
                                         ("description", "description", "Description", 2000),
                                         ("objectives", "objectives", "Objectives", 2000)):
            if not partial or key in body:
                fields[column] = clean_text(body.get(key), what, limit, multiline=key != "tagline")
        if not partial or "openRoles" in body:
            fields["open_roles"] = json.dumps(clean_list(body.get("openRoles"), "Open roles", 10))
        if not partial or "requiredSkills" in body or "skills" in body:
            if "requiredSkills" in body:
                required = recruitment.clean_required_skills(body.get("requiredSkills"), clean_text, HubError)
            else:  # older apps send names only: any level
                required = {normalize_skill(s): 1 for s in clean_list(body.get("skills"), "Skills", 15)}
            fields["skills"] = json.dumps(list(required))
            fields["required_skills"] = json.dumps(required)
        if not partial or "eventId" in body:
            event_id = body.get("eventId") or None
            if event_id is not None and str(event_id) not in self._events():
                raise HubError(400, "That event does not exist.")
            fields["event_id"] = event_id
        if not partial or "recruiting" in body:
            fields["recruiting"] = 1 if clean_bool(body.get("recruiting", True)) else 0
        if not partial or "maxMembers" in body:
            fields["max_members"] = clean_int(body.get("maxMembers", 6), "Team size", 2, 20)
        return fields

    def _team_summary(self, row, user_id, counts=None, leaders=None, roles=None) -> dict:
        team_id = row[0]
        events = self._events()
        event = events.get(row[7]) if row[7] else None
        members = counts.get(team_id, 0) if counts is not None else self._member_count(team_id)
        return {
            "id": team_id, "name": row[1], "tagline": row[2], "openRoles": _loads(row[5], []),
            "skills": _loads(row[6], []), "requiredSkills": self._required_skills(row),
            "event": {"id": row[7], "name": event["name"]} if event else None,
            "recruiting": bool(row[8]), "maxMembers": row[9], "memberCount": members, "full": members >= row[9],
            "leader": (leaders or {}).get(team_id), "myRole": (roles or {}).get(team_id),
        }

    # ------------------------------------------------------------------ teams
    def list_teams(self, user, q, body):
        page, size = page_args(q, 9)
        where, params = [], []
        if q.get("q"):
            needle = "%" + clean_text(q["q"], "Search", 80, multiline=False).lower() + "%"
            where.append("(LOWER(t.name) LIKE ? OR LOWER(t.tagline) LIKE ? OR LOWER(t.skills) LIKE ? "
                         "OR LOWER(t.open_roles) LIKE ?)")
            params += [needle] * 4
        if clean_bool(q.get("recruiting")):
            where.append("t.recruiting = 1")
        if clean_bool(q.get("mine")):
            where.append("t.id IN (SELECT team_id FROM team_members WHERE user_id = ?)")
            params.append(user["id"])
        sql = ("SELECT t.id, t.name, t.tagline, t.description, t.objectives, t.open_roles, t.skills, t.event_id, "
               "t.recruiting, t.max_members, t.created_at, t.required_skills FROM teams t")
        if where:
            sql += " WHERE " + " AND ".join(where)
        rows = self.db.all(sql + " ORDER BY t.recruiting DESC, t.created_at DESC", tuple(params))
        result = paginate(rows, page, size)
        ids = [r[0] for r in result["items"]]
        counts, leaders, roles = {}, {}, {}
        if ids:
            for team_id, user_id, role in self.db.all(
                    f"SELECT team_id, user_id, role FROM team_members WHERE team_id IN ({_marks(ids)})", tuple(ids)):
                counts[team_id] = counts.get(team_id, 0) + 1
                if role == "leader":
                    leaders[team_id] = user_id
                if user_id == user["id"]:
                    roles[team_id] = role
            people = self.profiles(leaders.values())
            leaders = {t: people.get(u) for t, u in leaders.items()}
        result["items"] = [self._team_summary(r, user["id"], counts, leaders, roles) for r in result["items"]]
        return result

    def create_team(self, user, q, body):
        self._require_profile(user)
        fields = self._team_fields(body)
        self._take("team_create", user, "You have created several teams in the last hour. Try again later.")
        stamp = now()
        try:
            with self.db.transaction():
                if self.db.one("SELECT 1 FROM teams WHERE name_key = ?", (fields["name_key"],)):
                    raise HubError(409, "A team with this name already exists. Choose another name.")
                columns = list(fields) + ["created_by", "created_at", "updated_at"]
                team_id = self.db.insert(f"INSERT INTO teams ({', '.join(columns)}) VALUES ({_marks(columns)})",
                                         tuple(fields.values()) + (user["id"], stamp, stamp))
                self.db.execute("INSERT INTO team_members (team_id, user_id, role, joined_at) VALUES (?, ?, 'leader', ?)",
                                (team_id, user["id"], stamp))
        except HubError:
            raise
        except Exception as error:
            if _is_unique_violation(error):
                raise HubError(409, "A team with this name already exists. Choose another name.")
            raise
        return 201, self.get_team(user, str(team_id), q={}, body={})

    def get_team(self, user, team_id, q, body):
        row = self._team_row(team_id)
        team_id = row[0]
        members = self.db.all("SELECT user_id, role, joined_at FROM team_members WHERE team_id = ? ORDER BY joined_at",
                              (team_id,))
        people = self.profiles([m[0] for m in members])
        my_role = next((m[1] for m in members if m[0] == user["id"]), None)
        leader = next((people.get(m[0]) for m in members if m[1] == "leader"), None)
        team = self._team_summary(row, user["id"], {team_id: len(members)}, {team_id: leader}, {team_id: my_role})
        team.update({
            "description": row[3], "objectives": row[4], "createdAt": row[10],
            "members": [dict(people.get(m[0], {"id": m[0], "name": "Member"}), teamRole=m[1], joinedAt=m[2])
                        for m in members],
        })
        mine = self.db.one(
            """SELECT id, status, response, created_at, decided_at FROM team_applications
               WHERE team_id = ? AND user_id = ? ORDER BY id DESC LIMIT 1""", (team_id, user["id"]))
        team["myApplication"] = ({"id": mine[0], "status": mine[1], "response": mine[2], "createdAt": mine[3],
                                  "decidedAt": mine[4]} if mine else None)
        team["canApply"] = (my_role is None and team["recruiting"] and not team["full"]
                            and not (mine and mine[1] == "pending"))
        team["canManage"] = my_role in ("leader", "admin")
        team["vacancies"] = max(0, team["maxMembers"] - team["memberCount"])
        invitation = self.db.one(
            "SELECT id, expires_at, message FROM team_invitations WHERE team_id = ? AND user_id = ? AND status = 'pending'",
            (team_id, user["id"]))
        team["myInvitation"] = ({"id": invitation[0], "expiresAt": invitation[1], "message": invitation[2]}
                                if invitation else None)
        if team["canManage"]:
            team["pendingInvitations"] = self.db.one(
                "SELECT COUNT(*) FROM team_invitations WHERE team_id = ? AND status = 'pending'", (team_id,))[0]
            team["inviteLinkActive"] = self._active_link(team_id) is not None
            team["pendingApplications"] = self.db.one(
                "SELECT COUNT(*) FROM team_applications WHERE team_id = ? AND status = 'pending'", (team_id,))[0]
            team["anonymousRequests"] = self.db.one(
                "SELECT COUNT(*) FROM anon_threads WHERE target_kind = 'team' AND target_id = ? AND status = 'new'",
                (str(team_id),))[0]
        return {"team": team}

    def update_team(self, user, team_id, q, body):
        self._require_manager(team_id, user)
        fields = self._team_fields(body, partial=True)
        if "max_members" in fields and fields["max_members"] < self._member_count(team_id):
            raise HubError(400, "The team already has more members than that.")
        if fields:
            try:
                with self.db.transaction():
                    if "name_key" in fields and self.db.one("SELECT 1 FROM teams WHERE name_key = ? AND id <> ?",
                                                            (fields["name_key"], int(team_id))):
                        raise HubError(409, "A team with this name already exists. Choose another name.")
                    sets = ", ".join(f"{c} = ?" for c in fields)
                    self.db.execute(f"UPDATE teams SET {sets}, updated_at = ? WHERE id = ?",
                                    tuple(fields.values()) + (now(), int(team_id)))
            except HubError:
                raise
            except Exception as error:
                if _is_unique_violation(error):
                    raise HubError(409, "A team with this name already exists. Choose another name.")
                raise
        return self.get_team(user, team_id, q={}, body={})

    def delete_team(self, user, team_id, q, body):
        row = self._team_row(team_id)
        if self._team_role(team_id, user["id"]) != "leader":
            raise HubError(403, "Only the team leader can delete the team.")
        if clean_text(body.get("confirm"), "Confirmation", 60, multiline=False) != row[1]:
            raise HubError(400, "Type the team's name to confirm.")
        members = [m for m in self.db.all("SELECT user_id FROM team_members WHERE team_id = ?", (row[0],))
                   if m[0] != user["id"]]
        with self.db.transaction():
            self.db.execute("DELETE FROM anon_threads WHERE target_kind = 'team' AND target_id = ?", (str(row[0]),))
            self.db.execute("DELETE FROM teams WHERE id = ?", (row[0],))
            for (member,) in members:
                self.notify(member, "team_deleted", f"{row[1]} was closed", "The team leader deleted the team.", "#/teams")
        return {"ok": True}

    def leave_team(self, user, team_id, q, body):
        row = self._team_row(team_id)
        role = self._require_member(team_id, user)
        if role == "leader":
            raise HubError(409, "Make another member the leader before leaving, or delete the team.")
        self.db.execute("DELETE FROM team_members WHERE team_id = ? AND user_id = ?", (row[0], user["id"]))
        for manager in self._team_managers(row[0]):
            self.notify(manager, "member_left", f"A member left {row[1]}",
                        f"{self.profile(user['id'])['name']} left the team.", f"#/teams/{row[0]}")
        return {"ok": True}

    def set_member_role(self, user, team_id, member_id, q, body):
        row = self._team_row(team_id)
        member_id = int(member_id)
        if self._team_role(team_id, user["id"]) != "leader":
            raise HubError(403, "Only the team leader can change roles.")
        role = body.get("role")
        if role not in TEAM_ROLES:
            raise HubError(400, "Role must be leader, admin or member.")
        if member_id == user["id"]:
            raise HubError(400, "Make another member the leader to change your own role.")
        if self._team_role(team_id, member_id) is None:
            raise HubError(404, "That person is not a member of this team.")
        with self.db.transaction():
            self.db.execute("UPDATE team_members SET role = ? WHERE team_id = ? AND user_id = ?",
                            (role, row[0], member_id))
            if role == "leader":  # leadership moves; the old leader stays as an admin
                self.db.execute("UPDATE team_members SET role = 'admin' WHERE team_id = ? AND user_id = ?",
                                (row[0], user["id"]))
        label = {"leader": "the leader", "admin": "an admin", "member": "a member"}[role]
        self.notify(member_id, "team_role", f"You are now {label} of {row[1]}", "", f"#/teams/{row[0]}")
        return self.get_team(user, team_id, q={}, body={})

    def remove_member(self, user, team_id, member_id, q, body):
        row = self._team_row(team_id)
        member_id = int(member_id)
        my_role = self._require_manager(team_id, user)
        target = self._team_role(team_id, member_id)
        if target is None:
            raise HubError(404, "That person is not a member of this team.")
        if member_id == user["id"]:
            raise HubError(400, "Use Leave team to leave.")
        if target == "leader" or (target == "admin" and my_role != "leader"):
            raise HubError(403, "Only the team leader can remove an admin, and nobody can remove the leader.")
        self.db.execute("DELETE FROM team_members WHERE team_id = ? AND user_id = ?", (row[0], member_id))
        self.notify(member_id, "team_removed", f"You were removed from {row[1]}", "", "#/teams")
        self.email(member_id, "team_notification", target=row[1], text=f"You were removed from the team {row[1]}.")
        return self.get_team(user, team_id, q={}, body={})

    # ------------------------------------------------------------------ team chat
    def _message_dicts(self, rows, user_id) -> list[dict]:
        people = self.profiles([r[1] for r in rows])
        return [{"id": r[0], "author": people.get(r[1], {"id": r[1], "name": "Former member", "color": "#A3ACB8",
                                                         "avatar": None, "headline": "", "role": ""}),
                 "body": r[2], "createdAt": r[3], "mine": r[1] == user_id} for r in rows]

    def get_messages(self, user, team_id, q, body):
        """Newest page by default; ?before=<id> for older pages; ?after=<id>&wait=<s> waits (long
        polling) until a newer message arrives or the time is up, which is how the chat updates live."""
        self._require_member(team_id, user)
        team_id = int(team_id)
        limit = clean_int(q.get("limit") or MESSAGE_PAGE, "Limit", 1, 100)
        last_read = self.db.one("SELECT last_read_id FROM team_members WHERE team_id = ? AND user_id = ?",
                                (team_id, user["id"]))[0]
        select = "SELECT id, user_id, body, created_at FROM team_messages WHERE team_id = ?"
        if q.get("after") not in (None, ""):
            after = clean_int(q["after"], "after", 0, 2 ** 62)
            wait = clean_int(q.get("wait") or 0, "wait", 0, LONG_POLL_SECONDS)
            rows = self.db.all(f"{select} AND id > ? ORDER BY id LIMIT ?", (team_id, after, limit))
            if not rows and wait:
                with self.chat:
                    self.chat.wait_for(lambda: self.latest_message.get(team_id, 0) > after, timeout=wait)
                if self._team_role(team_id, user["id"]) is None:  # removed while waiting
                    raise HubError(403, "Only this team's members can do that.")
                rows = self.db.all(f"{select} AND id > ? ORDER BY id LIMIT ?", (team_id, after, limit))
            return {"messages": self._message_dicts(rows, user["id"]), "hasMore": False, "lastReadId": last_read}
        params = [team_id]
        sql = select
        if q.get("before") not in (None, ""):
            sql += " AND id < ?"
            params.append(clean_int(q["before"], "before", 0, 2 ** 62))
        rows = self.db.all(sql + " ORDER BY id DESC LIMIT ?", tuple(params) + (limit + 1,))
        has_more = len(rows) > limit
        rows = list(reversed(rows[:limit]))
        return {"messages": self._message_dicts(rows, user["id"]), "hasMore": has_more, "lastReadId": last_read}

    def post_message(self, user, team_id, q, body):
        row = self._team_row(team_id)
        self._require_member(team_id, user)
        text = clean_text(body.get("body"), "Message", MAX_TEAM_MESSAGE, required=True)
        self._take("team_message", user, "You are sending messages very quickly. Wait a moment.")
        team_id = row[0]
        stamp = now()
        author = self.profile(user["id"])["name"]
        with self.db.transaction():
            message_id = self.db.insert("INSERT INTO team_messages (team_id, user_id, body, created_at) VALUES (?, ?, ?, ?)",
                                        (team_id, user["id"], text, stamp))
            self.db.execute("UPDATE team_members SET last_read_id = ? WHERE team_id = ? AND user_id = ?",
                            (message_id, team_id, user["id"]))
            for member_id, last_read in self.db.all(
                    "SELECT user_id, last_read_id FROM team_members WHERE team_id = ? AND user_id <> ?",
                    (team_id, user["id"])):
                unread = self.db.one(
                    "SELECT COUNT(*) FROM team_messages WHERE team_id = ? AND id > ? AND user_id <> ?",
                    (team_id, last_read, member_id))[0]
                title = f"{unread} new message{'s' if unread != 1 else ''} in {row[1]}"
                self.notify(member_id, "team_message", title, f"{author}: {text[:140]}",
                            f"#/messages/{team_id}", ref_key=f"team-msg:{team_id}")
        with self.chat:
            self.latest_message[team_id] = max(self.latest_message.get(team_id, 0), message_id)
            self.chat.notify_all()
        return 201, {"message": self._message_dicts([(message_id, user["id"], text, stamp)], user["id"])[0]}

    def mark_read(self, user, team_id, q, body):
        self._require_member(team_id, user)
        team_id = int(team_id)
        last_id = clean_int(body.get("lastId"), "lastId", 0, 2 ** 62)
        latest = self.db.one("SELECT COALESCE(MAX(id), 0) FROM team_messages WHERE team_id = ?", (team_id,))[0]
        self.db.execute(
            "UPDATE team_members SET last_read_id = ? WHERE team_id = ? AND user_id = ? AND last_read_id < ?",
            (min(last_id, latest), team_id, user["id"], min(last_id, latest)))
        if last_id >= latest:
            self.db.execute("UPDATE notifications SET read_at = ? WHERE user_id = ? AND ref_key = ? AND read_at IS NULL",
                            (now(), user["id"], f"team-msg:{team_id}"))
        return {"ok": True}

    def _unread_messages(self, user_id) -> dict[int, int]:
        rows = self.db.all(
            """SELECT m.team_id, COUNT(x.id) FROM team_members m
               LEFT JOIN team_messages x ON x.team_id = m.team_id AND x.id > m.last_read_id AND x.user_id <> m.user_id
               WHERE m.user_id = ? GROUP BY m.team_id""", (user_id,))
        return {r[0]: r[1] for r in rows}

    def conversations(self, user, q, body):
        """The chats of every team the person belongs to, newest activity first."""
        teams = self.db.all(
            """SELECT t.id, t.name, m.role, t.created_at FROM team_members m JOIN teams t ON t.id = m.team_id
               WHERE m.user_id = ?""", (user["id"],))
        unread = self._unread_messages(user["id"])
        items = []
        for team_id, name, role, created in teams:
            last = self.db.one("SELECT id, user_id, body, created_at FROM team_messages WHERE team_id = ? "
                               "ORDER BY id DESC LIMIT 1", (team_id,))
            preview = None
            if last:
                author = self.profiles([last[1]]).get(last[1], {"name": "Former member"})
                preview = {"body": last[2][:140], "author": author["name"], "createdAt": last[3],
                           "mine": last[1] == user["id"]}
            items.append({"teamId": team_id, "name": name, "myRole": role, "unread": unread.get(team_id, 0),
                          "lastMessage": preview, "activity": last[3] if last else created,
                          "memberCount": self._member_count(team_id)})
        items.sort(key=lambda c: c["activity"], reverse=True)
        return {"items": items, "unread": sum(c["unread"] for c in items)}

    # ------------------------------------------------------------------ team applications
    def apply_to_team(self, user, team_id, q, body):
        return self._create_team_application(user, self._team_row(team_id), body, source="direct")

    def _create_team_application(self, user, row, body, source):
        team_id = row[0]
        self._require_profile(user)
        fields = (
            clean_text(body.get("introduction"), "Introduction", MAX_LONG_TEXT, required=True),
            json.dumps(clean_list(body.get("skills"), "Skills", 20)),
            clean_text(body.get("experience"), "Experience", MAX_LONG_TEXT),
            clean_text(body.get("interests"), "Interests", 1000),
            clean_text(body.get("reason"), "Why you want to join", MAX_LONG_TEXT, required=True),
        )
        self._take("application", user, "You have sent many applications in the last hour. Try again later.")
        try:
            with self.db.transaction():
                if self._team_role(team_id, user["id"]):
                    raise HubError(409, "You are already a member of this team.")
                if not row[8]:
                    raise HubError(409, "This team is not recruiting right now.")
                if self._member_count(team_id) >= row[9]:
                    raise HubError(409, "This team is full.")
                if self.db.one("SELECT 1 FROM team_applications WHERE team_id = ? AND user_id = ? AND status = 'pending'",
                               (team_id, user["id"])):
                    raise HubError(409, "You already have a pending application to this team.")
                app_id = self.db.insert(
                    """INSERT INTO team_applications (team_id, user_id, introduction, skills, experience, interests,
                           reason, status, created_at, source) VALUES (?, ?, ?, ?, ?, ?, ?, 'pending', ?, ?)""",
                    (team_id, user["id"], *fields, now(), source))
                name = self.profile(user["id"])["name"]
                how = "through your invitation link" if source == "link" else ""
                for manager in self._team_managers(team_id):
                    self.notify(manager, "team_application", f"New application to {row[1]}",
                                f"{name} wants to join{' ' + how if how else ''}.", f"#/teams/{team_id}/applications")
        except HubError:
            raise
        except Exception as error:
            if _is_unique_violation(error):
                raise HubError(409, "You already have a pending application to this team.")
            raise
        return 201, {"application": {"id": app_id, "status": "pending", "source": source}, "teamId": team_id}

    def _team_app_dicts(self, rows, with_profiles=True) -> list[dict]:
        people = self.profiles([r[2] for r in rows], full=True) if with_profiles else {}
        teams = {}
        out = []
        for r in rows:
            if r[1] not in teams:
                t = self.db.one("SELECT name FROM teams WHERE id = ?", (r[1],))
                teams[r[1]] = t[0] if t else "Deleted team"
            out.append({"id": r[0], "kind": "team", "teamId": r[1], "targetName": teams[r[1]],
                        "applicant": people.get(r[2]) if with_profiles else None,
                        "introduction": r[3], "skills": _loads(r[4], []), "experience": r[5], "interests": r[6],
                        "reason": r[7], "status": r[8], "response": r[9], "createdAt": r[10], "decidedAt": r[11],
                        "source": r[12] or "direct", "link": f"#/teams/{r[1]}"})
        return out

    _TEAM_APP_SELECT = ("SELECT id, team_id, user_id, introduction, skills, experience, interests, reason, status, "
                        "response, created_at, decided_at, source FROM team_applications")

    def team_applications(self, user, team_id, q, body):
        self._require_manager(team_id, user)
        page, size = page_args(q)
        status = q.get("status") or ""
        if status and status not in TEAM_APP_STATUSES:
            raise HubError(400, "Unknown status.")
        sql = self._TEAM_APP_SELECT + " WHERE team_id = ?" + (" AND status = ?" if status else "")
        rows = self.db.all(sql + " ORDER BY created_at DESC", (int(team_id), status) if status else (int(team_id),))
        result = paginate(rows, page, size)
        result["items"] = self._team_app_dicts(result["items"])
        return result

    def decide_team_application(self, user, app_id, q, body):
        decision = body.get("decision")
        if decision not in ("approve", "reject"):
            raise HubError(400, "Decision must be approve or reject.")
        response = clean_text(body.get("response"), "Response", 1000)
        app = self.db.one("SELECT team_id, user_id, status FROM team_applications WHERE id = ?", (int(app_id),))
        if app is None:
            raise HubError(404, "No such application.")
        team_id, applicant, _ = app
        row = self._team_row(team_id)
        self._require_manager(team_id, user)
        with self.db.transaction():
            # Re-read inside the transaction: a second leader may have decided a moment ago.
            current = self.db.one("SELECT status FROM team_applications WHERE id = ?", (int(app_id),))[0]
            if current != "pending":
                raise HubError(409, f"This application was already {current}.")
            status = "approved" if decision == "approve" else "rejected"
            if status == "approved":
                if self._team_role(team_id, applicant) is None:
                    if self._member_count(team_id) >= row[9]:
                        raise HubError(409, "The team is full. Raise the team size or remove a member first.")
                    self.db.execute(
                        "INSERT INTO team_members (team_id, user_id, role, joined_at) VALUES (?, ?, 'member', ?)",
                        (team_id, applicant, now()))
            updated = self.db.execute(
                """UPDATE team_applications SET status = ?, response = ?, decided_by = ?, decided_at = ?
                   WHERE id = ? AND status = 'pending'""", (status, response, user["id"], now(), int(app_id)))
            if updated != 1:
                raise HubError(409, "This application was already decided.")
            if status == "approved":
                self.notify(applicant, "team_application_approved", f"Welcome to {row[1]}!",
                            response or "Your application was approved. Say hello in the team chat.",
                            f"#/messages/{team_id}")
            else:
                self.notify(applicant, "team_application_rejected", f"Your application to {row[1]} was not accepted",
                            response, f"#/applications")
        self.email(applicant, "application_decision", target=row[1], kind="team", approved=status == "approved",
                   message=response)
        return {"application": {"id": int(app_id), "status": status, "response": response}}

    def withdraw_team_application(self, user, app_id, q, body):
        updated = self.db.execute(
            "UPDATE team_applications SET status = 'withdrawn', decided_at = ? WHERE id = ? AND user_id = ? AND status = 'pending'",
            (now(), int(app_id), user["id"]))
        if updated != 1:
            row = self.db.one("SELECT status FROM team_applications WHERE id = ? AND user_id = ?", (int(app_id), user["id"]))
            if row is None:
                raise HubError(404, "No such application.")
            raise HubError(409, f"Only pending applications can be withdrawn (this one is {row[0]}).")
        return {"application": {"id": int(app_id), "status": "withdrawn"}}

    # ------------------------------------------------------------------ events
    def _events(self) -> dict[str, dict]:
        """The app's projects (requirements.json), by id; parsed once per stored version."""
        row = self.db.one("SELECT content, version FROM shared_files WHERE name = 'requirements.json'")
        if row is None:
            return {}
        if self._events_cache[0] == row[1]:
            return self._events_cache[1]
        events = {}
        for item in _loads(row[0], {}).get("requirements", []):
            if isinstance(item, dict) and isinstance(item.get("id"), str):
                events[item["id"]] = item
        self._events_cache = (row[1], events)
        return events

    def _event(self, event_id) -> dict:
        event = self._events().get(event_id)
        if event is None:
            raise HubError(404, "This event does not exist (it may have been removed in the Host workspace).")
        return event

    def _settings(self, event_id):
        return self.db.one(
            """SELECT event_id, organizer_id, description, requirements_text, eligibility, positions, capacity,
                      deadline, accepting FROM event_settings WHERE event_id = ?""", (event_id,))

    def _event_counts(self, event_id) -> dict[str, int]:
        rows = self.db.all("SELECT status, COUNT(*) FROM event_applications WHERE event_id = ? GROUP BY status",
                           (event_id,))
        return {status: count for status, count in rows}

    def _can_manage_event(self, settings, user) -> bool:
        return settings is not None and (settings[1] == user["id"] or user["role"] == "admin")

    def _event_dict(self, event, settings, user, counts=None, organizers=None) -> dict:
        counts = counts if counts is not None else (self._event_counts(event["id"]) if settings else {})
        approved = counts.get("approved", 0)
        capacity = settings[6] if settings else None
        deadline = settings[7] if settings else None
        accepting = bool(settings and settings[8])
        closed_reason = ""
        if not settings:
            closed_reason = "Not open for applications yet."
        elif not accepting:
            closed_reason = "Applications are closed."
        elif deadline is not None and now() > deadline:
            closed_reason = "The registration deadline has passed."
        data = {
            "id": event["id"], "name": str(event.get("name", event["id"])), "type": str(event.get("type", "")),
            "summary": str(event.get("summary", "")), "requiredSkills": event.get("requiredSkills") or {},
            "minTeamSize": event.get("minTeamSize"), "maxTeamSize": event.get("maxTeamSize"),
            "configured": settings is not None, "description": settings[2] if settings else "",
            "requirements": settings[3] if settings else "", "eligibility": settings[4] if settings else "",
            "positions": _loads(settings[5], []) if settings else [], "capacity": capacity, "deadline": deadline,
            "accepting": accepting, "open": settings is not None and not closed_reason, "closedReason": closed_reason,
            "approvedCount": approved, "waitlistedCount": counts.get("waitlisted", 0),
            "spotsLeft": None if capacity is None else max(0, capacity - approved),
            "full": capacity is not None and approved >= capacity,
            "organizer": (organizers or {}).get(settings[1]) if settings else None,
            "canManage": self._can_manage_event(settings, user),
            "canConfigure": user["role"] in ("host", "admin") and (settings is None or self._can_manage_event(settings, user)),
        }
        if data["canManage"]:
            data["pendingCount"] = counts.get("pending", 0)
        return data

    def list_events(self, user, q, body):
        page, size = page_args(q, 9)
        events = list(self._events().values())
        settings = {r[0]: r for r in self.db.all(
            """SELECT event_id, organizer_id, description, requirements_text, eligibility, positions, capacity,
                      deadline, accepting FROM event_settings""")}
        counts = {}
        for event_id, status, count in self.db.all(
                "SELECT event_id, status, COUNT(*) FROM event_applications GROUP BY event_id, status"):
            counts.setdefault(event_id, {})[status] = count
        organizers = self.profiles([s[1] for s in settings.values()])
        items = [self._event_dict(e, settings.get(e["id"]), user, counts.get(e["id"], {}), organizers) for e in events]
        if q.get("q"):
            needle = clean_text(q["q"], "Search", 80, multiline=False).lower()
            items = [e for e in items if needle in (e["name"] + " " + e["type"] + " " + e["summary"] + " "
                                                    + " ".join(e["requiredSkills"])).lower()]
        if clean_bool(q.get("open")):
            items = [e for e in items if e["open"]]
        if clean_bool(q.get("mine")):
            items = [e for e in items if e["canManage"]]
        items.sort(key=lambda e: (not e["open"], not e["configured"], e["name"].lower()))
        mine = {r[0]: r[1] for r in self.db.all(
            "SELECT event_id, status FROM event_applications WHERE user_id = ? ORDER BY id", (user["id"],))}
        for item in items:
            item["myStatus"] = mine.get(item["id"])
        return paginate(items, page, size)

    def get_event(self, user, event_id, q, body):
        event = self._event(event_id)
        settings = self._settings(event_id)
        organizers = self.profiles([settings[1]]) if settings else {}
        data = self._event_dict(event, settings, user, organizers=organizers)
        mine = self.db.one(
            """SELECT id, status, position, feedback, created_at, decided_at FROM event_applications
               WHERE event_id = ? AND user_id = ? ORDER BY id DESC LIMIT 1""", (event_id, user["id"]))
        data["myApplication"] = ({"id": mine[0], "status": mine[1], "position": mine[2], "feedback": mine[3],
                                  "createdAt": mine[4], "decidedAt": mine[5]} if mine else None)
        active = mine is not None and mine[1] in EVENT_ACTIVE
        data["canApply"] = data["open"] and not active and not (settings and settings[1] == user["id"])
        if data["canManage"]:
            data["anonymousRequests"] = self.db.one(
                "SELECT COUNT(*) FROM anon_threads WHERE target_kind = 'event' AND target_id = ? AND status = 'new'",
                (event_id,))[0]
        return {"event": data}

    def save_event_settings(self, user, event_id, q, body):
        self._event(event_id)
        if user["role"] not in ("host", "admin"):
            raise HubError(403, "Only hosts can open events for applications.")
        settings = self._settings(event_id)
        if settings is not None and not self._can_manage_event(settings, user):
            raise HubError(403, "Only this event's organiser (or an admin) can change it.")
        deadline = body.get("deadline")
        if deadline in (None, ""):
            deadline = None
        elif isinstance(deadline, bool) or not isinstance(deadline, (int, float)) or not 0 < deadline < 4102444800:
            raise HubError(400, "Deadline must be a date.")
        values = {
            "description": clean_text(body.get("description"), "Description", 3000),
            "requirements_text": clean_text(body.get("requirements"), "Requirements", 2000),
            "eligibility": clean_text(body.get("eligibility"), "Eligibility", 1000),
            "positions": json.dumps(clean_list(body.get("positions"), "Positions", 10)),
            "capacity": clean_int(body.get("capacity"), "Capacity", 1, 1000, optional=True),
            "deadline": float(deadline) if deadline is not None else None,
            "accepting": 1 if clean_bool(body.get("accepting", True)) else 0,
        }
        with self.db.transaction():
            approved = self.db.one("SELECT COUNT(*) FROM event_applications WHERE event_id = ? AND status = 'approved'",
                                   (event_id,))[0]
            if values["capacity"] is not None and values["capacity"] < approved:
                raise HubError(400, f"{approved} people are already approved; capacity cannot be lower.")
            if settings is None:
                columns = ["event_id", "organizer_id"] + list(values) + ["updated_at"]
                self.db.execute(f"INSERT INTO event_settings ({', '.join(columns)}) VALUES ({_marks(columns)})",
                                (event_id, user["id"], *values.values(), now()))
            else:
                sets = ", ".join(f"{c} = ?" for c in values)
                self.db.execute(f"UPDATE event_settings SET {sets}, updated_at = ? WHERE event_id = ?",
                                (*values.values(), now(), event_id))
        return self.get_event(user, event_id, q={}, body={})

    def apply_to_event(self, user, event_id, q, body):
        event = self._event(event_id)
        self._require_profile(user)
        settings = self._settings(event_id)
        if settings is None:
            raise HubError(409, "This event is not open for applications yet.")
        positions = _loads(settings[5], [])
        position = clean_text(body.get("position"), "Position", 40, multiline=False)
        if positions and position not in positions:
            raise HubError(400, "Choose one of the event's positions.")
        motivation = clean_text(body.get("motivation"), "Why you want to take part", MAX_LONG_TEXT, required=True)
        experience = clean_text(body.get("experience"), "Experience", MAX_LONG_TEXT)
        availability = clean_text(body.get("availability"), "Availability", 300)
        self._take("application", user, "You have sent many applications in the last hour. Try again later.")
        try:
            with self.db.transaction():
                settings = self._settings(event_id)  # fresh, inside the transaction
                if settings[1] == user["id"]:
                    raise HubError(409, "You organise this event.")
                if not settings[8]:
                    raise HubError(409, "Applications for this event are closed.")
                if settings[7] is not None and now() > settings[7]:
                    raise HubError(409, "The registration deadline has passed.")
                if self.db.one("SELECT 1 FROM event_applications WHERE event_id = ? AND user_id = ? AND status IN "
                               "('pending', 'approved', 'waitlisted')", (event_id, user["id"])):
                    raise HubError(409, "You have already applied to this event.")
                approved = self.db.one(
                    "SELECT COUNT(*) FROM event_applications WHERE event_id = ? AND status = 'approved'", (event_id,))[0]
                status = "waitlisted" if settings[6] is not None and approved >= settings[6] else "pending"
                app_id = self.db.insert(
                    """INSERT INTO event_applications (event_id, user_id, position, motivation, experience,
                           availability, status, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                    (event_id, user["id"], position, motivation, experience, availability, status, now()))
                name = self.profile(user["id"])["name"]
                self.notify(settings[1], "event_application", f"New application for {event.get('name', event_id)}",
                            f"{name} applied" + (" (waitlisted: the event is full)." if status == "waitlisted" else "."),
                            f"#/events/{event_id}/applications")
        except HubError:
            raise
        except Exception as error:
            if _is_unique_violation(error):
                raise HubError(409, "You have already applied to this event.")
            raise
        return 201, {"application": {"id": app_id, "status": status}}

    _EVENT_APP_SELECT = ("SELECT id, event_id, user_id, position, motivation, experience, availability, status, "
                         "feedback, created_at, decided_at FROM event_applications")

    def _event_app_dicts(self, rows, with_profiles=True) -> list[dict]:
        people = self.profiles([r[2] for r in rows], full=True) if with_profiles else {}
        events = self._events()
        return [{"id": r[0], "kind": "event", "eventId": r[1],
                 "targetName": str(events.get(r[1], {}).get("name", "Removed event")),
                 "applicant": people.get(r[2]) if with_profiles else None, "position": r[3], "motivation": r[4],
                 "experience": r[5], "availability": r[6], "status": r[7], "response": r[8], "createdAt": r[9],
                 "decidedAt": r[10], "link": f"#/events/{r[1]}"} for r in rows]

    def event_applications(self, user, event_id, q, body):
        self._event(event_id)
        settings = self._settings(event_id)
        if not self._can_manage_event(settings, user):
            raise HubError(403, "Only this event's organiser can see its applications.")
        page, size = page_args(q)
        status = q.get("status") or ""
        if status and status not in EVENT_APP_STATUSES:
            raise HubError(400, "Unknown status.")
        sql = self._EVENT_APP_SELECT + " WHERE event_id = ?" + (" AND status = ?" if status else "")
        rows = self.db.all(sql + " ORDER BY created_at", (event_id, status) if status else (event_id,))
        result = paginate(rows, page, size)
        result["items"] = self._event_app_dicts(result["items"])
        result["counts"] = self._event_counts(event_id)
        result["capacity"] = settings[6]
        return result

    def decide_event_application(self, user, app_id, q, body):
        decision = body.get("decision")
        moves = {"approve": ("approved", ("pending", "waitlisted")), "reject": ("rejected", ("pending", "waitlisted")),
                 "waitlist": ("waitlisted", ("pending",))}
        if decision not in moves:
            raise HubError(400, "Decision must be approve, reject or waitlist.")
        feedback = clean_text(body.get("feedback"), "Feedback", 1000)
        app = self.db.one("SELECT event_id, user_id FROM event_applications WHERE id = ?", (int(app_id),))
        if app is None:
            raise HubError(404, "No such application.")
        event_id, applicant = app
        event = self._event(event_id)
        if not self._can_manage_event(self._settings(event_id), user):
            raise HubError(403, "Only this event's organiser can decide applications.")
        status, allowed_from = moves[decision]
        with self.db.transaction():
            current = self.db.one("SELECT status FROM event_applications WHERE id = ?", (int(app_id),))[0]
            if current not in allowed_from:
                raise HubError(409, f"This application is {current}; it cannot be moved to {status}.")
            if status == "approved":
                capacity = self._settings(event_id)[6]
                approved = self.db.one(
                    "SELECT COUNT(*) FROM event_applications WHERE event_id = ? AND status = 'approved'", (event_id,))[0]
                if capacity is not None and approved >= capacity:
                    raise HubError(409, f"The event is full ({capacity} places). Waitlist this applicant or raise "
                                        "the capacity.", code="full")
            updated = self.db.execute(
                """UPDATE event_applications SET status = ?, feedback = ?, decided_by = ?, decided_at = ?
                   WHERE id = ? AND status = ?""", (status, feedback, user["id"], now(), int(app_id), current))
            if updated != 1:
                raise HubError(409, "This application was changed by someone else a moment ago.")
            name = str(event.get("name", event_id))
            titles = {"approved": f"You're in: {name}", "rejected": f"Your application for {name} was not accepted",
                      "waitlisted": f"You're on the waitlist for {name}"}
            self.notify(applicant, f"event_application_{status}", titles[status], feedback, f"#/events/{event_id}")
        if status in ("approved", "rejected"):
            self.email(applicant, "application_decision", target=name, kind="event", approved=status == "approved",
                       message=feedback)
        return {"application": {"id": int(app_id), "status": status, "feedback": feedback}}

    def withdraw_event_application(self, user, app_id, q, body):
        row = self.db.one("SELECT event_id, status FROM event_applications WHERE id = ? AND user_id = ?",
                          (int(app_id), user["id"]))
        if row is None:
            raise HubError(404, "No such application.")
        event_id, status = row
        if status not in EVENT_ACTIVE:
            raise HubError(409, f"This application is already {status}.")
        settings = self._settings(event_id)
        if status == "approved" and settings and settings[7] is not None and now() > settings[7]:
            raise HubError(409, "The deadline has passed; contact the organiser to drop out.")
        updated = self.db.execute(
            "UPDATE event_applications SET status = 'withdrawn', decided_at = ? WHERE id = ? AND status = ?",
            (now(), int(app_id), status))
        if updated != 1:
            raise HubError(409, "This application changed a moment ago. Reload and try again.")
        if status == "approved" and settings:
            waiting = self.db.one("SELECT COUNT(*) FROM event_applications WHERE event_id = ? AND status = 'waitlisted'",
                                  (event_id,))[0]
            name = str(self._events().get(event_id, {}).get("name", event_id))
            self.notify(settings[1], "event_place_freed", f"A place opened up in {name}",
                        f"An approved participant withdrew. {waiting} on the waitlist.",
                        f"#/events/{event_id}/applications")
        return {"application": {"id": int(app_id), "status": "withdrawn"}}

    # ------------------------------------------------------------------ dashboards
    def my_applications(self, user, q, body):
        page, size = page_args(q)
        kind = q.get("kind") or ""
        status = q.get("status") or ""
        items = []
        if kind in ("", "team"):
            items += self._team_app_dicts(self.db.all(self._TEAM_APP_SELECT + " WHERE user_id = ?", (user["id"],)),
                                          with_profiles=False)
        if kind in ("", "event"):
            items += self._event_app_dicts(self.db.all(self._EVENT_APP_SELECT + " WHERE user_id = ?", (user["id"],)),
                                           with_profiles=False)
        if status:
            items = [i for i in items if i["status"] == status]
        if q.get("q"):
            needle = clean_text(q["q"], "Search", 80, multiline=False).lower()
            items = [i for i in items if needle in i["targetName"].lower()]
        items.sort(key=lambda i: i["createdAt"], reverse=True)
        return paginate(items, page, size)

    def received_applications(self, user, q, body):
        """Applications waiting for this person's decision: to teams they lead or help run, and to
        events they organise (all events for admins)."""
        page, size = page_args(q)
        kind = q.get("kind") or ""
        status = q.get("status") if "status" in q else "pending"
        items = []
        if kind in ("", "team"):
            teams = [r[0] for r in self.db.all(
                "SELECT team_id FROM team_members WHERE user_id = ? AND role IN ('leader', 'admin')", (user["id"],))]
            if teams:
                items += self._team_app_dicts(self.db.all(
                    self._TEAM_APP_SELECT + f" WHERE team_id IN ({_marks(teams)})", tuple(teams)))
        if kind in ("", "event"):
            if user["role"] == "admin":
                rows = self.db.all(self._EVENT_APP_SELECT)
            else:
                rows = self.db.all(self._EVENT_APP_SELECT + " WHERE event_id IN "
                                   "(SELECT event_id FROM event_settings WHERE organizer_id = ?)", (user["id"],))
            items += self._event_app_dicts(rows)
        if status:
            items = [i for i in items if i["status"] == status]
        if q.get("q"):
            needle = clean_text(q["q"], "Search", 80, multiline=False).lower()
            items = [i for i in items if needle in (i["targetName"] + " " + (i["applicant"] or {}).get("name", "")).lower()]
        items.sort(key=lambda i: i["createdAt"], reverse=True)
        return paginate(items, page, size)

    def summary(self, user, q, body):
        own = self.get_my_profile(user, q, body)
        managed_teams = [r[0] for r in self.db.all(
            "SELECT team_id FROM team_members WHERE user_id = ? AND role IN ('leader', 'admin')", (user["id"],))]
        pending_team = self.db.one(
            f"SELECT COUNT(*) FROM team_applications WHERE status = 'pending' AND team_id IN ({_marks(managed_teams)})",
            tuple(managed_teams))[0] if managed_teams else 0
        if user["role"] == "admin":
            pending_event = self.db.one("SELECT COUNT(*) FROM event_applications WHERE status = 'pending'")[0]
        else:
            pending_event = self.db.one(
                """SELECT COUNT(*) FROM event_applications a JOIN event_settings s ON s.event_id = a.event_id
                   WHERE a.status = 'pending' AND s.organizer_id = ?""", (user["id"],))[0]
        my_apps = self.db.all(
            """SELECT status, COUNT(*) FROM (SELECT status FROM team_applications WHERE user_id = ?
               UNION ALL SELECT status FROM event_applications WHERE user_id = ?) x GROUP BY status""",
            (user["id"], user["id"]))
        inbox = self._anon_inbox_rows(user)
        return {
            "profile": own["profile"], "needsSetup": own["needsSetup"], "canOrganize": user["role"] in ("host", "admin"),
            "identityVersion": self.identity_version(),
            "isAdmin": user["role"] == "admin",
            "unreadNotifications": self._unread_notifications(user["id"]),
            "unreadMessages": sum(self._unread_messages(user["id"]).values()),
            "pendingDecisions": {"team": pending_team, "event": pending_event},
            "myApplications": {status: count for status, count in my_apps},
            "anonymousInbox": sum(1 for r in inbox if r[5]),
            "anonymousSentUnread": self.db.one(
                "SELECT COUNT(*) FROM anon_threads WHERE sender_id = ? AND sender_unread = 1", (user["id"],))[0],
            "teams": self.conversations(user, q, body)["items"][:6],
            "pendingInvitations": self.db.one(
                "SELECT COUNT(*) FROM team_invitations WHERE user_id = ? AND status = 'pending' AND expires_at > ?",
                (user["id"], now()))[0],
            "linkClaim": self.link_claim_summary(user),
        }

    # ------------------------------------------------------------------ anonymous connections
    #
    # The sender's account is stored (anon_threads.sender_id) so the sender can follow their own
    # requests, so limits and blocks work, and so admins can act on a report. It is never part of
    # what a recipient receives: the thread's public id is random, messages carry only a
    # "from_sender" flag, and recipient-facing payloads are built from explicit fields only.

    def _target(self, kind, target_id) -> dict:
        """{kind, id, name, recipients} for a team, an event or a person, or HubError(404)."""
        if kind == "team":
            if not str(target_id).isdigit():
                raise HubError(404, "No such team.")
            row = self.db.one("SELECT id, name FROM teams WHERE id = ?", (int(target_id),))
            if row is None:
                raise HubError(404, "No such team.")
            return {"kind": "team", "id": str(row[0]), "name": row[1], "recipients": self._team_managers(row[0])}
        if kind == "event":
            event = self._events().get(str(target_id))
            settings = self._settings(str(target_id)) if event else None
            if settings is None:
                raise HubError(404, "This event has no organiser to contact yet.")
            return {"kind": "event", "id": str(target_id), "name": str(event.get("name", target_id)),
                    "recipients": [settings[1]]}
        if kind == "person":
            if not str(target_id).isdigit():
                raise HubError(404, "No such person.")
            row = self.db.one("SELECT display_name, accepts_anonymous FROM profiles WHERE user_id = ?", (int(target_id),))
            if row is None or not row[1] or not row[0]:
                raise HubError(404, "This person does not accept anonymous messages.")
            return {"kind": "person", "id": str(target_id), "name": row[0], "recipients": [int(target_id)]}
        raise HubError(400, "Send to a team, an event or a person.")

    def _target_or_gone(self, kind, target_id) -> dict:
        try:
            return self._target(kind, target_id)
        except HubError:
            return {"kind": kind, "id": str(target_id), "name": "No longer available", "recipients": []}

    def anon_create(self, user, q, body):
        target = self._target(body.get("targetKind"), str(body.get("targetId", "")))
        text = clean_text(body.get("body"), "Message", MAX_ANON_MESSAGE, required=True)
        if user["id"] in target["recipients"] or (target["kind"] == "team" and self._team_role(int(target["id"]), user["id"])):
            raise HubError(400, "You cannot send an anonymous message to yourself or your own team.")
        if self.db.one("SELECT 1 FROM anon_blocks WHERE target_kind = ? AND target_id = ? AND sender_id = ?",
                       (target["kind"], target["id"], user["id"])):
            raise HubError(403, "This recipient is not accepting messages from you.")
        existing = self.db.one(
            """SELECT public_id FROM anon_threads WHERE sender_id = ? AND target_kind = ? AND target_id = ?
               AND status <> 'declined' AND created_at > ? ORDER BY id DESC LIMIT 1""",
            (user["id"], target["kind"], target["id"], now() - ANON_REPEAT_DAYS * 86400))
        if existing:
            raise HubError(409, "You already have an open conversation with this recipient. Continue it instead.",
                           threadId=existing[0])
        sent_today = self.db.one("SELECT COUNT(*) FROM anon_threads WHERE sender_id = ? AND created_at > ?",
                                 (user["id"], now() - 86400))[0]
        if sent_today >= ANON_DAILY_LIMIT:
            raise HubError(429, f"You can start up to {ANON_DAILY_LIMIT} anonymous conversations a day.")
        self._take("anon_new", user, "You have started several anonymous conversations in the last hour. Try later.")
        public_id = secrets.token_urlsafe(12)
        stamp = now()
        with self.db.transaction():
            thread_id = self.db.insert(
                """INSERT INTO anon_threads (public_id, sender_id, target_kind, target_id, status, sender_unread,
                       recipient_unread, created_at, last_message_at) VALUES (?, ?, ?, ?, 'new', 0, 1, ?, ?)""",
                (public_id, user["id"], target["kind"], target["id"], stamp, stamp))
            self.db.execute("INSERT INTO anon_messages (thread_id, from_sender, author_id, body, created_at) VALUES (?, 1, ?, ?, ?)",
                            (thread_id, user["id"], text, stamp))
            about = {"team": f"your team {target['name']}", "event": f"your event {target['name']}", "person": "you"}
            for recipient in target["recipients"]:
                self.notify(recipient, "anonymous_message", "New anonymous message",
                            f"Someone reached out about {about[target['kind']]}.", f"#/anonymous/{public_id}")
        return 201, {"thread": {"id": public_id, "status": "new"}}

    def _anon_inbox_rows(self, user):
        conditions, params = ["(target_kind = 'person' AND target_id = ?)"], [str(user["id"])]
        teams = [str(r[0]) for r in self.db.all(
            "SELECT team_id FROM team_members WHERE user_id = ? AND role IN ('leader', 'admin')", (user["id"],))]
        if teams:
            conditions.append(f"(target_kind = 'team' AND target_id IN ({_marks(teams)}))")
            params += teams
        events = [r[0] for r in self.db.all("SELECT event_id FROM event_settings WHERE organizer_id = ?", (user["id"],))]
        if events:
            conditions.append(f"(target_kind = 'event' AND target_id IN ({_marks(events)}))")
            params += events
        return self.db.all(
            f"""SELECT id, public_id, target_kind, target_id, status, recipient_unread, created_at, last_message_at,
                       invited_at FROM anon_threads WHERE sender_id <> ? AND ({' OR '.join(conditions)})
                ORDER BY last_message_at DESC""", (user["id"], *params))

    def _thread_summary(self, row, unread) -> dict:
        thread_id, public_id, kind, target_id, status = row[:5]
        target = self._target_or_gone(kind, target_id)
        first = self.db.one("SELECT body FROM anon_messages WHERE thread_id = ? ORDER BY id LIMIT 1", (thread_id,))
        count = self.db.one("SELECT COUNT(*) FROM anon_messages WHERE thread_id = ?", (thread_id,))[0]
        return {"id": public_id, "target": {"kind": kind, "id": target["id"], "name": target["name"]},
                "status": status, "unread": bool(unread), "preview": (first[0] if first else "")[:160],
                "messageCount": count, "createdAt": row[6], "lastMessageAt": row[7], "invited": row[8] is not None}

    def anon_inbox(self, user, q, body):
        page, size = page_args(q)
        status = q.get("status") or ""
        if status and status not in ANON_STATUSES:
            raise HubError(400, "Unknown status.")
        rows = [r for r in self._anon_inbox_rows(user) if not status or r[4] == status]
        result = paginate(rows, page, size)
        result["items"] = [self._thread_summary(r, r[5]) for r in result["items"]]
        return result

    def anon_sent(self, user, q, body):
        page, size = page_args(q)
        rows = self.db.all(
            """SELECT id, public_id, target_kind, target_id, status, sender_unread, created_at, last_message_at,
                      invited_at FROM anon_threads WHERE sender_id = ? ORDER BY last_message_at DESC""", (user["id"],))
        result = paginate(rows, page, size)
        result["items"] = [self._thread_summary(r, r[5]) for r in result["items"]]
        return result

    def _thread_for(self, user, public_id):
        """(row, viewer, target) where viewer is "sender" or "recipient"; 404 for anyone else, so
        a stranger cannot even tell whether the conversation exists."""
        row = self.db.one(
            """SELECT id, public_id, sender_id, target_kind, target_id, status, invited_at, created_at, last_message_at
               FROM anon_threads WHERE public_id = ?""", (public_id,))
        if row is None:
            raise HubError(404, "No such conversation.")
        target = self._target_or_gone(row[3], row[4])
        if row[2] == user["id"]:
            return row, "sender", target
        if user["id"] in target["recipients"]:
            return row, "recipient", target
        raise HubError(404, "No such conversation.")

    def anon_thread(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        messages = self.db.all(
            "SELECT id, from_sender, author_id, body, created_at FROM anon_messages WHERE thread_id = ? ORDER BY id",
            (row[0],))
        people = self.profiles([m[2] for m in messages if not m[1]])
        out = []
        for message_id, from_sender, author_id, text, created in messages:
            if from_sender:
                author = {"name": "You" if viewer == "sender" else "Anonymous", "anonymous": True}
            else:
                person = people.get(author_id, {"name": "Recipient"})
                author = {"name": "You" if author_id == user["id"] else person["name"], "anonymous": False,
                          "avatar": person.get("avatar"), "color": person.get("color")}
            out.append({"id": message_id, "fromSender": bool(from_sender), "author": author, "body": text,
                        "createdAt": created, "mine": (viewer == "sender") == bool(from_sender)
                        and (from_sender or author_id == user["id"])})
        column = "sender_unread" if viewer == "sender" else "recipient_unread"
        self.db.execute(f"UPDATE anon_threads SET {column} = 0 WHERE id = ?", (row[0],))
        return {"thread": {
            "id": row[1], "viewer": viewer, "status": row[5], "invited": row[6] is not None,
            "target": {"kind": row[3], "id": target["id"], "name": target["name"]},
            "createdAt": row[7], "lastMessageAt": row[8], "messages": out,
            "canReply": row[5] != "declined" and bool(target["recipients"]),
            "canInvite": viewer == "recipient" and row[3] in ("team", "event"),
        }}

    def anon_reply(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        if row[5] == "declined" or not target["recipients"]:
            raise HubError(409, "This conversation is closed.")
        text = clean_text(body.get("body"), "Message", MAX_ANON_MESSAGE, required=True)
        self._take("anon_reply", user, "You are sending many messages. Try again later.")
        stamp = now()
        with self.db.transaction():
            self.db.execute("INSERT INTO anon_messages (thread_id, from_sender, author_id, body, created_at) VALUES (?, ?, ?, ?, ?)",
                            (row[0], 1 if viewer == "sender" else 0, user["id"], text, stamp))
            other = "recipient_unread" if viewer == "sender" else "sender_unread"
            status = "reviewed" if viewer == "recipient" and row[5] == "new" else row[5]
            self.db.execute(f"UPDATE anon_threads SET {other} = 1, last_message_at = ?, status = ? WHERE id = ?",
                            (stamp, status, row[0]))
            link = f"#/anonymous/{row[1]}"
            if viewer == "recipient":
                self.notify(row[2], "anonymous_reply", f"Reply about {target['name']}",
                            text[:140], link, ref_key=f"anon:{row[1]}:sender")
            else:
                for recipient in target["recipients"]:
                    self.notify(recipient, "anonymous_reply", "New reply in an anonymous conversation",
                                text[:140], link, ref_key=f"anon:{row[1]}:recipient")
        return 201, {"ok": True}

    def anon_status(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        if viewer != "recipient":
            raise HubError(403, "Only the recipient can change the status.")
        status = body.get("status")
        if status not in ("reviewed", "interested", "declined"):
            raise HubError(400, "Status must be reviewed, interested or declined.")
        self.db.execute("UPDATE anon_threads SET status = ? WHERE id = ?", (status, row[0]))
        if status in ("interested", "declined") and status != row[5]:
            text = {"interested": f"{target['name']} is interested in your message.",
                    "declined": f"{target['name']} has declined your message."}[status]
            self.notify(row[2], "anonymous_status", text, "", f"#/anonymous/{row[1]}")
        return {"status": status}

    def anon_invite(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        if viewer != "recipient" or row[3] not in ("team", "event"):
            raise HubError(403, "Only a team's leaders or an event's organiser can invite.")
        if not target["recipients"]:
            raise HubError(409, "This conversation is closed.")
        self.db.execute("UPDATE anon_threads SET invited_at = ?, status = 'interested' WHERE id = ?", (now(), row[0]))
        link = f"#/teams/{target['id']}?apply=1" if row[3] == "team" else f"#/events/{target['id']}?apply=1"
        self.notify(row[2], "invitation", f"You're invited to apply to {target['name']}",
                    "They read your anonymous message and would like your application. Applying shows them your profile.",
                    link)
        self.email(row[2], "invitation", target=target["name"], kind=row[3])
        return {"invited": True}

    def anon_report(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        reason = clean_text(body.get("reason"), "Reason", 500, required=True)
        self._take("report", user, "You have sent many reports. Try again later.")
        if self.db.one("SELECT 1 FROM anon_reports WHERE thread_id = ? AND reporter_id = ? AND status = 'open'",
                       (row[0], user["id"])):
            raise HubError(409, "You have already reported this conversation; an admin will look at it.")
        self.db.execute("INSERT INTO anon_reports (thread_id, reporter_id, reason, created_at) VALUES (?, ?, ?, ?)",
                        (row[0], user["id"], reason, now()))
        for (admin_id,) in self.db.all("SELECT id FROM users WHERE role = 'admin'"):
            self.notify(admin_id, "report", "An anonymous conversation was reported", reason[:140], "/admin",
                        ref_key="reports")
        return 201, {"reported": True}

    def anon_block(self, user, public_id, q, body):
        row, viewer, target = self._thread_for(user, public_id)
        if viewer != "recipient":
            raise HubError(403, "Only the recipient can block the sender.")
        with self.db.transaction():
            self.db.execute(
                "INSERT INTO anon_blocks (target_kind, target_id, sender_id, created_at) VALUES (?, ?, ?, ?) "
                "ON CONFLICT DO NOTHING", (row[3], row[4], row[2], now()))
            self.db.execute("UPDATE anon_threads SET status = 'declined' WHERE id = ?", (row[0],))
        return {"blocked": True, "status": "declined"}

    # ------------------------------------------------------------------ moderation (admins)
    def admin_reports(self, user, q, body):
        """Reported conversations. Admins see the sender's account here, and only here: the
        privacy note on every anonymous form says so."""
        if user["role"] != "admin":
            raise HubError(403, "Admins only.")
        status = q.get("status") or "open"
        rows = self.db.all(
            """SELECT r.id, r.reason, r.status, r.resolution, r.created_at, t.id, t.public_id, t.target_kind,
                      t.target_id, t.status, s.email, r.reporter_id
               FROM anon_reports r JOIN anon_threads t ON t.id = r.thread_id JOIN users s ON s.id = t.sender_id
               WHERE r.status = ? ORDER BY r.created_at DESC LIMIT 100""", (status,))
        reporters = self.profiles([r[11] for r in rows])
        items = []
        for r in rows:
            messages = self.db.all("SELECT from_sender, body, created_at FROM anon_messages WHERE thread_id = ? ORDER BY id",
                                   (r[5],))
            target = self._target_or_gone(r[7], r[8])
            items.append({"id": r[0], "reason": r[1], "status": r[2], "resolution": r[3], "createdAt": r[4],
                          "thread": {"id": r[6], "status": r[9], "target": {"kind": r[7], "name": target["name"]}},
                          "senderEmail": r[10], "reporter": (reporters.get(r[11]) or {}).get("name", "Unknown"),
                          "messages": [{"fromSender": bool(m[0]), "body": m[1], "createdAt": m[2]} for m in messages]})
        return {"items": items}

    def admin_resolve_report(self, user, report_id, q, body):
        if user["role"] != "admin":
            raise HubError(403, "Admins only.")
        action = body.get("action")
        if action not in ("dismiss", "block"):
            raise HubError(400, "Action must be dismiss or block.")
        row = self.db.one(
            """SELECT r.thread_id, t.target_kind, t.target_id, t.sender_id FROM anon_reports r
               JOIN anon_threads t ON t.id = r.thread_id WHERE r.id = ? AND r.status = 'open'""", (int(report_id),))
        if row is None:
            raise HubError(404, "No open report with that id.")
        with self.db.transaction():
            if action == "block":
                self.db.execute(
                    "INSERT INTO anon_blocks (target_kind, target_id, sender_id, created_at) VALUES (?, ?, ?, ?) "
                    "ON CONFLICT DO NOTHING", (row[1], row[2], row[3], now()))
                self.db.execute("UPDATE anon_threads SET status = 'declined' WHERE id = ?", (row[0],))
            self.db.execute(
                """UPDATE anon_reports SET status = 'resolved', resolution = ?, resolved_by = ?, resolved_at = ?
                   WHERE thread_id = ? AND status = 'open'""", (action, user["id"], now(), row[0]))
        return {"ok": True}
