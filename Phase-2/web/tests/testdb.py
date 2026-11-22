"""Direct database access for tests that need to change stored data (move a date into the past,
put an old-style row in place). Uses the server's own database: PostgreSQL when DATABASE_URL is
set (web/tests/run_on_postgres.py), otherwise the suite's temporary SQLite file. Queries use "?"
placeholders on both."""
import os
import sqlite3


class _Postgres:
    def __init__(self, url):
        import psycopg
        self.conn = psycopg.connect(url, autocommit=True)

    def execute(self, sql, params=()):
        return self.conn.execute(sql.replace("?", "%s"), params or None)

    def commit(self):
        pass  # autocommit

    def close(self):
        self.conn.close()


def connect(sqlite_path):
    url = os.environ.get("DATABASE_URL", "").strip()
    return _Postgres(url) if url else sqlite3.connect(sqlite_path)
