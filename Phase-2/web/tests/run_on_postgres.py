"""Runs the server test suites against a real PostgreSQL instead of SQLite.

Production (Render, Azure) uses PostgreSQL; the suites normally run on SQLite, which accepts
some things PostgreSQL does not (a literal % in a query once crashed the server on start only on
PostgreSQL). This starts a private, temporary PostgreSQL with the `pgserver` package
(pip install --user pgserver), gives every suite a fresh database, and runs it with DATABASE_URL
pointing there. Nothing outside the temporary folder is touched.

Run from Phase-2:  python web/tests/run_on_postgres.py [suite ...]
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

SUITES = ["test_server", "test_hub", "test_accounts", "test_recruitment", "test_copy_database"]
HERE = Path(__file__).resolve().parent


def main():
    try:
        import pgserver
    except ImportError:
        raise SystemExit("Install the test PostgreSQL first:  python -m pip install --user pgserver")
    suites = sys.argv[1:] or SUITES
    folder = Path(tempfile.mkdtemp(prefix="tf-pg-"))
    server = pgserver.get_server(folder / "data", cleanup_mode="delete")
    failed = []
    try:
        for suite in suites:
            name = suite.replace("test_", "t_")
            server.psql(f"DROP DATABASE IF EXISTS {name};")
            server.psql(f"CREATE DATABASE {name};")
            env = dict(os.environ, DATABASE_URL=server.get_uri(name))
            result = subprocess.run([sys.executable, str(HERE / f"{suite}.py")], env=env, capture_output=True, text=True,
                                    timeout=900)
            lines = (result.stdout + result.stderr).strip().splitlines()
            summary = next((l for l in reversed(lines) if "passed" in l), lines[-1] if lines else "(no output)")
            print(f"{suite} on PostgreSQL: {summary}")
            for line in lines:
                if line.startswith("FAIL") or "Traceback" in line or "Error" in line:
                    print("   ", line[:300])
            if result.returncode != 0:
                failed.append(suite)
    finally:
        server.cleanup()
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
