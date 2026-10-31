#!/usr/bin/env python3
"""Copies TeamForge's database (accounts, sessions, shared data, each account's session file)
from one server to another, for example from Render's PostgreSQL to Azure's.

    TEAMFORGE_COPY_FROM=<database URL or SQLite file> TEAMFORGE_COPY_TO=<...> python web/tools/copy_database.py

The URLs hold passwords, so they are read from those environment variables rather than the
command line (which shell history keeps). In Azure Cloud Shell:

    read -rs TEAMFORGE_COPY_FROM; export TEAMFORGE_COPY_FROM    # paste Render's External Database URL
    read -rs TEAMFORGE_COPY_TO; export TEAMFORGE_COPY_TO        # paste the Azure URL (?sslmode=require)
    python3 web/tools/copy_database.py

The target gets the same tables as the server creates (server.Database). It must be empty, so
nothing is overwritten by accident; --replace empties it first. Ids, password hashes, sessions and
versions are copied unchanged, so everyone signs in as before. The source is only read. Prints
row counts, never contents.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from server import Database  # noqa: E402  (the same schema and SQL dialect handling)

# Parents before children (sessions and user_files refer to users).
TABLES = {
    "users": ["id", "email", "password_hash", "role", "created_at", "last_login"],
    "sessions": ["token_hash", "user_id", "expires_at"],
    "shared_files": ["name", "content", "version", "updated_at", "updated_by"],
    "user_files": ["user_id", "name", "content", "version", "updated_at"],
}


def open_database(target: str) -> Database:
    if target.startswith(("postgres://", "postgresql://")):
        return Database(target, Path())
    return Database("", Path(target))


def describe(target: str) -> str:
    """Where a database is, without its password."""
    if target.startswith(("postgres://", "postgresql://")):
        parts = urlsplit(target)
        return f"PostgreSQL {parts.hostname}{parts.path}"
    return f"SQLite {target}"


def counts(db: Database) -> dict[str, int]:
    return {table: db.one(f"SELECT COUNT(*) FROM {table}")[0] for table in TABLES}


def copy(source: str, target: str, replace: bool = False) -> dict[str, int]:
    src, dst = open_database(source), open_database(target)
    existing = counts(dst)
    if any(existing.values()):
        if not replace:
            raise SystemExit(f"The target already has data ({existing}). Nothing was copied. "
                             "Run again with --replace to empty it first.")
        for table in reversed(list(TABLES)):
            dst.execute(f"DELETE FROM {table}")
    for table, columns in TABLES.items():
        rows = src.all(f"SELECT {', '.join(columns)} FROM {table}")
        marks = ", ".join("?" for _ in columns)
        for row in rows:
            dst.execute(f"INSERT INTO {table} ({', '.join(columns)}) VALUES ({marks})", tuple(row))
    if dst.is_pg:  # new accounts must continue after the copied ids
        dst.execute("SELECT setval(pg_get_serial_sequence('users', 'id'), COALESCE((SELECT MAX(id) FROM users), 0) + 1, false)")
    copied, expected = counts(dst), counts(src)
    if copied != expected:
        raise SystemExit(f"Row counts differ after copying: source {expected}, target {copied}.")
    return copied


def main() -> None:
    source = os.environ.get("TEAMFORGE_COPY_FROM", "").strip()
    target = os.environ.get("TEAMFORGE_COPY_TO", "").strip()
    if not source or not target:
        raise SystemExit("Set TEAMFORGE_COPY_FROM and TEAMFORGE_COPY_TO (see the top of this file).")
    if source == target:
        raise SystemExit("Source and target are the same database.")
    print(f"Copying {describe(source)} -> {describe(target)}")
    copied = copy(source, target, replace="--replace" in sys.argv[1:])
    print("Copied: " + ", ".join(f"{table} {n}" for table, n in copied.items()))


if __name__ == "__main__":
    main()
