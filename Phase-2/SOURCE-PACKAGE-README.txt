TEAMFORGE SOURCE PACKAGE

Purpose:
This package contains the complete current TeamForge application source required for
another developer to port the application to Qt WebAssembly and deploy it as a browser
application.

The existing Windows application is the reference implementation. The browser version should
preserve the same product, UI and functionality, making only necessary platform-specific
adaptations.

Contents
- C++ backend ............ src/core (domain model, validation, exceptions) and src/app
                           (TeamForgeController, the QML bridge, exposed to QML as "Backend")
- Qt Quick/QML UI ........ ui/ (Main.qml, Theme.qml, components/, pages/, participant/,
                           host/, developer/); three workspaces: Participant, Host, Developer
- DSA .................... src/core: SkillIndex (hash map), SkillBST (AVL tree), SkillGraph
                           (BFS), Repository<T> (template), priority-queue top-k ranking
- Matching engine ........ src/core/MatchingEngine, MatchingStrategy (abstract),
                           WeightedMatchingStrategy, CoverageOnlyStrategy, Team, InterestBook
- Persistence ............ src/core/JsonStorage (the only core file using Qt) and
                           src/app/SampleData (seeds the runtime data folder)
- Tests .................. tests/ (7 Qt Test executables; ctest)
- Data ................... data/ (clean seed: 560 participants, 8 projects, 549 skills,
                           86 interest requests, 0 saved teams)
- Assets ................. assets/ (logos, Manrope fonts + OFL.txt licence)
- Tools .................. tools/generate_sample_data.py, tools/make_logo_layers.ps1
- WebAssembly notes ...... WEB-CONVERSION-NOTES.txt
- Documentation .......... CLAUDE.md (detailed architecture and conventions; read first),
                           README.md, Phase-1/ (design report and slides, LaTeX + PDF)

Desktop build (reference)
- Qt 6.8 or later (developed with Qt 6.12.0), CMake 3.22+, a C++17 compiler.
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<Qt>/6.12.0/<kit>
    cmake --build build
    ctest --test-dir build --output-on-failure
- CLAUDE.md quotes the original machine's toolchain paths (D:\QT ...). Replace them with
  your own Qt install; nothing in the CMake files depends on them.
- New .qml files must be added to QML_FILES in CMakeLists.txt.

Seed data
- Regenerate:  python tools/generate_sample_data.py   (Python 3, standard library only)
- It rewrites the five JSON files in data/ (students, requirements, teams, skills,
  interest_requests).
- Reproducible: the generator uses a fixed random seed (SEED = 2026) and keeps the first 12
  existing records of data/students.json, so re-running gives byte-identical files (checked).
- data/ is bundled into the app as Qt resources; at startup the app copies it into its
  runtime data folder. The app never writes to data/ in the source tree.
