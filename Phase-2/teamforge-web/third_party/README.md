# third_party

TeamForge needs three files here. They are not included in the ZIP so you get them from the official sources:

| File | Library | Licence |
|---|---|---|
| `httplib.h` | [cpp-httplib](https://github.com/yhirose/cpp-httplib) (HTTP server, header-only) | MIT |
| `sqlite3.c`, `sqlite3.h` | [SQLite amalgamation](https://www.sqlite.org/download.html) | Public domain |

Run `get-deps.ps1` (Windows) or `get-deps.sh` (Linux/macOS).

If a download link has moved, get the files by hand:
- `httplib.h` from the cpp-httplib GitHub page (any release from v0.15 on works).
- The "amalgamation" ZIP from sqlite.org/download.html. Copy `sqlite3.c` and `sqlite3.h` out of it.
