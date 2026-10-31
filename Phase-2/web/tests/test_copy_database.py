"""Tests for web/tools/copy_database.py (SQLite to SQLite; PostgreSQL uses the same SQL through
server.Database). Run from the project folder:  python web/tests/test_copy_database.py"""
import os, subprocess, sys, tempfile
from pathlib import Path
WEB = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WEB)); sys.path.insert(0, str(WEB / "tools"))
import server, copy_database  # noqa: E402

results = []
def check(name, cond, detail=""):
    results.append(bool(cond)); print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))

work = Path(tempfile.mkdtemp())
source, target = str(work / "render.db"), str(work / "azure.db")

# A source like a live server: seeded data, two accounts, a session, a saved change, own session file.
store = server.TeamForgeStore(server.Database("", Path(source)), server.SEED_DIR)
store.seed_if_empty()
alice = store.create_user("alice@test.edu", "alicepass1")
bob = store.create_user("bob@test.edu", "bobpass123")
token = store.create_session(bob["id"])
name, (content, version) = next(iter(store.shared_files().items()))
store.save_user_file(alice["id"], "session.json", '{"version": 1, "lastWorkspace": "participant"}')

copied = copy_database.copy(source, target)
check("every table copied", copied == copy_database.counts(store.db) and copied["users"] == 2 and copied["sessions"] == 1, copied)

moved = server.TeamForgeStore(server.Database("", Path(target)), server.SEED_DIR)
check("accounts keep their password hashes (same password works)",
      server.verify_password("alicepass1", moved.find_user_by_email("alice@test.edu")["password_hash"]))
check("sessions carry over (nobody is signed out)", moved.user_for_token(token)["email"] == "bob@test.edu")
check("shared data and versions carry over", moved.shared_files()[name] == (content, version))
check("each account's own session file carries over", moved.user_file(alice["id"], "session.json") is not None)
carol = moved.create_user("carol@test.edu", "carolpass1")
check("new accounts get new ids after the copied ones", carol["id"] > bob["id"], carol["id"])

try:
    copy_database.copy(source, target)
    check("a non-empty target is refused", False)
except SystemExit as stop:
    check("a non-empty target is refused", "already has data" in str(stop), stop)
check("--replace empties the target first", copy_database.copy(source, target, replace=True)["users"] == 2)

env = dict(os.environ, TEAMFORGE_COPY_FROM="postgresql://tf:s3cret@db.example.com/teamforge", TEAMFORGE_COPY_TO="")
out = subprocess.run([sys.executable, str(WEB / "tools" / "copy_database.py")], env=env, capture_output=True, text=True)
check("missing settings are explained", out.returncode != 0 and "TEAMFORGE_COPY_TO" in out.stderr, out.stderr)
check("passwords are never printed", "s3cret" not in out.stdout + out.stderr
      and "s3cret" not in copy_database.describe("postgresql://tf:s3cret@db.example.com/teamforge"))

print(f"\n{sum(results)}/{len(results)} passed")
sys.exit(0 if all(results) else 1)
