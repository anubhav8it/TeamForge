# Phase 2 – TeamForge application

The TeamForge application built in Phase 2: a C++17 / Qt 6 (Qt Quick, QML) app that matches
students to hackathon and project teams by complementary skills.

| Folder | Contents |
|---|---|
| `src/core` | C++ core: domain model, matching engine (weighted scoring 40/25/15/10/10), SkillIndex (hash map), SkillBST (AVL tree), SkillGraph (BFS), top-k ranking, JSON storage |
| `src/app` | Bridge between the C++ core and the QML screens (`TeamForgeController`, exposed as `Backend`) |
| `src/web` | Browser-only bridge for the WebAssembly build |
| `ui` | QML screens: entry screen, Participant, Host and Developer workspaces |
| `tests` | 7 Qt Test programs (`ctest`) |
| `data` | Sample data: 560 participants, 8 projects, 549 skills, 86 interest requests |
| `web` | Web version: sign-in, accounts and shared data server; see [`web/README-WEB.md`](web/README-WEB.md) |

Desktop build: see `SOURCE-PACKAGE-README.txt`. Web build and deployment: `web/README-WEB.md`.
